/*
 * Resume the UEFI context that was stored by the Linux kernel
 * when it started.  This assumes that kexec-load has been
 * given a copy of the first page at the bottom of RAM,
 * which includes the "well-known" context structure.
 */

_start:
	.global _start
	cli
#define UEFI_CONTEXT 0x100
	// restore control registers first so that pages are mapped
	// UEFI includes a linear mapping of memory, so the kexec
	// based linear map *should* still be good
	movq UEFI_CONTEXT + 0x38, %rbx
	mov  %rbx, %cr0
	movq UEFI_CONTEXT + 0x40, %rbx
	mov  %rbx, %cr3
	movq UEFI_CONTEXT + 0x48, %rbx
	mov  %rbx, %cr4
	movq UEFI_CONTEXT + 0x50, %rbx
	mov  %rbx, %cr8

	// restore the segment and interrupt descriptors
	lidt UEFI_CONTEXT + 0x60
	lgdt UEFI_CONTEXT + 0x70
	// lldt UEFI_CONTEXT + 0x80 // faults?

	// restore the segments, except for code segment
	// which has to wait for the next long jump
	mov $0x30, %rcx
	mov %cx, %ds
	mov %cx, %es
	mov %cx, %fs
	mov %cx, %gs
	mov %cx, %ss

	movq UEFI_CONTEXT + 0x00, %rbx
	movq UEFI_CONTEXT + 0x08, %rbp
	movq UEFI_CONTEXT + 0x10, %r12
	movq UEFI_CONTEXT + 0x18, %r13
	movq UEFI_CONTEXT + 0x20, %r14
	movq UEFI_CONTEXT + 0x28, %r15
	movq UEFI_CONTEXT + 0x30, %rsp

	// top of the stack is the return address for UEFI,
	// but for a long return it needs to be the code
	// segment and then the return address.
	popq %rdi
	pushq $0x38 // cs
	pushq %rdi

	// return EFI_SUCCESS
	movq $0x0, %rax
	lretq

