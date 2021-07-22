# Boot arbitrary payloads with `kexec-load`

`kexec` is useful for booting well defined and supported payloads,
like other Linux kernels, although it is not as useful for experimenting
with arbitrary or new payload formats.  The underlying `kexec_load()`
system call takes an entry point plus list of virtual and physical addresses,
so it allows for more flexible experimentation with the payloads.

`kexec-load` wraps the system call to make it easy to call with a list
of files that will be mapped into the new kernel's initial memory.
For example, the UEFI Payload Package expects to be loaded at `0x800000`
and for a configuration file to be stored 64KB below its load address
and a GPT formatted ramdisk stored after the firmware image.

```
kexec-load \
	0x8004F0 \
	0x800000=UEFIPAYLOAD.fd \
	0x7F0000=/tmp/config.txt \
	0xC00000=ramdisk.gpt
```

The `uefi-boot` script shows a more complete example of how to build
the command line that UEFI Payload Package uses, based on the various
files in `/sys` that define the memory mapping, ACPI table locations,
etc.


## TODO
* [ ] What's the easiest way to locate the entry point for the UEFI payload?
* [ ] How to interface with secure boot signature checks?

