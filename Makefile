CFLAGS = \
	-O3 \
	-W \
	-Wall \


all: kexec-load uefi-resume.bin

kexec-load: kexec-load.c

uefi-resume.elf: uefi-resume.S
	$(CROSS)$(CC) \
		-static \
		-nostdlib \
		-o $@ \
		$<

%.bin: %.elf
	$(CROSS)objcopy \
		-O binary \
		-j .text \
		-j .rodata \
		$< \
		$@
