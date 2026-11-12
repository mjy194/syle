ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    stack PT_GNU_STACK FLAGS(6);
}
SECTIONS {
    . = 0x8000000000;
    .text : { *(.text*) *(.rodata*) } :code
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
