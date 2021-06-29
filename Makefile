CFLAGS = \
	-O3 \
	-W \
	-Wall \


all: kexec-load

kexec-load: kexec-load.c
