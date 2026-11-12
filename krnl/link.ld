OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kernel_entry)
PHDRS {
    text PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data PT_LOAD FLAGS(6);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
    . = 0x04000000;
    __kernel_start = .;
    .text : ALIGN(4096) { KEEP(*(.text.entry)) *(.text*) } :text
    . = ALIGN(4096);
    __text_end = .;
    __rodata_start = .;
    .rodata : { *(.rodata*) } :rodata
    . = ALIGN(4096);
    __rodata_end = .;
    .data : { *(.data*) } :data
    .bss (NOLOAD) : ALIGN(4096) { *(.bss*) *(COMMON) } :data
    . = ALIGN(4096);
    __kernel_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
    ASSERT(__kernel_end - __kernel_start <= 0x800000, "kernel exceeds 8 MiB bootstrap window")
}
