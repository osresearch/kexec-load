/*
 * Wrap the kexec_load system call to allow arbitrary files and
 * segments to be passed into the new kernel image.
 *
 * usage:
 * kexec-load entrypoint address=file [address=file ...]
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/kexec.h>
#include <linux/reboot.h>
#include <getopt.h>


/*
           struct kexec_segment {
               void   *buf;        // Buffer in user space
               size_t  bufsz;      // Buffer length in user space
               void   *mem;        // Physical address of kernel
               size_t  memsz;      // Physical address length
           };
*/

long kexec_load(
	unsigned long entry,
	unsigned long nr_segments,
	struct kexec_segment *segments,
	unsigned long flags
)
{
	return syscall(__NR_kexec_load, entry, nr_segments, segments, flags);
}

// TODO: where is this defined?
#define PAGESIZE (4096uL)

static int verbose = 0;

static void * read_file(const char * filename, size_t * size_out)
{
	const int fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto fail_open;
	
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0)
		goto fail_stat;

	const size_t size = statbuf.st_size;

	if (verbose)
		fprintf(stderr, "%s: %zu bytes\n", filename, size);

	uint8_t * buf = malloc(size);
	if (!buf)
		goto fail_malloc;

	size_t off = 0;
	while(off < size)
	{
		ssize_t rc = read(fd, buf + off, size - off);
		if (rc < 0)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;
			goto fail_read;
		}

		off += rc;
	}
	

	if (size_out)
		*size_out = size;

	return buf;

fail_read:
	free(buf);
fail_malloc:
fail_stat:
	close(fd);
fail_open:
	return NULL;
}


static const char usage[] =
"-h | --help              This help\n"
"-e | --entry address     Entry point address\n"
"-p | --preserve-context  Enable load-preserve-context flag\n"
"-v | --verbose           Print debug info\n"
"\n"
"address[+len]=file\n"
"address[+len]=physical-address\n"
"";

static const struct option options[] = {
	{ "help", no_argument, 0, 'h' },
	{ "entry", required_argument, 0, 'e' },
	{ "preserve-context", no_argument, 0, 'p' },
	{ "verbose", no_argument, 0, 'v' },
	{ "reboot", no_argument, 0, 'r' },
	{ 0, 0, 0, 0},
};


int main(int argc, char ** argv)
{
	unsigned long flags = KEXEC_ARCH_DEFAULT;
	struct kexec_segment segments[KEXEC_SEGMENT_MAX];

	int do_reboot = 0;
	unsigned long entrypoint = 0;
	const char * entrypoint_str = NULL;

	/* Reset getopt for the next pass. */
	opterr = 1;
	optind = 1;

	while (1)
	{
		const int opt = getopt_long(argc, argv, "h?e:pvr", options, 0);
		if (opt < 0)
			break;

		switch(opt) {
		case 'p':
			flags |= KEXEC_PRESERVE_CONTEXT;
			break;
		case 'e':
			entrypoint_str = optarg;
			break;
		case 'v':
			verbose++;
			break;
		case 'r':
			do_reboot = 1;
			break;
		case '?': case 'h':
			printf("%s", usage);
			return EXIT_FAILURE;
		default:
			fprintf(stderr, "%s", usage);
			return EXIT_FAILURE;
		}
	}

	const int num_segments = argc - optind;

	if (num_segments <= 0)
	{
		// no segments specified
		fprintf(stderr, "No segments specified!\n");
		return EXIT_FAILURE;
	}

	if (num_segments > KEXEC_SEGMENT_MAX)
	{
		fprintf(stderr, "Maximum %d segments, sorry\n", KEXEC_SEGMENT_MAX);
		return EXIT_FAILURE;
	}


	char * str_end;
	if (entrypoint_str)
	{
		entrypoint = strtoul(entrypoint_str, &str_end, 0);
		if (*str_end != '\0')
		{
			fprintf(stderr, "entrypoint parse error '%s'\n", entrypoint_str);
			return EXIT_FAILURE;
		}
	}


	for(int i=0 ; i < num_segments ; i++)
	{
		const char * addr_str = argv[optind + i];
		unsigned long addr = strtoul(addr_str, &str_end, 0);
		ssize_t memsz = -1;
		if (*str_end == '+')
		{
			// optional segment size in memory
			const char * memsz_str = str_end + 1;
			memsz = strtoul(memsz_str, &str_end, 0);
		}

		if (*str_end != '=')
		{
			fprintf(stderr, "segment parse error '%s'\n", addr_str);
			return EXIT_FAILURE;
		}

		const char * filename = str_end+1;
		size_t file_size;
		const void * buf = read_file(filename, &file_size);

		if (buf == NULL)
		{
			perror(filename);
			return EXIT_FAILURE;
		}

		// if the caller did not specify a segment size, use the 
		// size of the file.
		if (memsz == -1)
			memsz = file_size;

		// limit the file size if it exceeds the memory size
		if (file_size > (size_t) memsz)
			file_size = memsz;

		// page align the memory size
		memsz = (memsz + PAGESIZE - 1) & ~(PAGESIZE-1);


		if (verbose)
		fprintf(stderr, "%016lx+%016lx=%s: %p + %016lx\n",
			addr,
			memsz,
			filename,
			buf,
			file_size
		);

		segments[i] = (struct kexec_segment){
			.buf	= buf,
			.bufsz	= file_size,
			.mem	= (void*) addr,
			.memsz	= memsz,
		};
	}

	if (verbose)
		fprintf(stderr, "kexec-load: entrypoint=%lx flags=%lx segments=%d\n",
			entrypoint, flags, num_segments);

	int rc = kexec_load(entrypoint, num_segments, segments, flags);
	if (rc < 0)
	{
		perror("kexec_load");
		return EXIT_FAILURE;
	}

	if (!do_reboot)
		return EXIT_SUCCESS;

	if (verbose)
		fprintf(stderr, "kexec-load: rebooting\n");

	rc = reboot(LINUX_REBOOT_CMD_KEXEC);
	if (rc < 0)
	{
		perror("reboot");
		return EXIT_FAILURE;
	}

	printf("kexec-load: we shouldn't be here...\n");
	return EXIT_SUCCESS; // ???
}
