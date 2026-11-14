OUTPUT_FORMAT(elf64-x86-64)
ENTRY(probe_reset)
SECTIONS {
 . = 0xffff0000;
 .text : { *(.text) }
 /DISCARD/ : { *(.note*) *(.eh_frame*) }
 ASSERT(SIZEOF(.text) == 0x10000, "probe ROM must be 64KiB")
 ASSERT(probe_reset == 0xfffffff0, "reset vector must end at 4GiB")
}
