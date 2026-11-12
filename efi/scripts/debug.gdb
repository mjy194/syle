set pagination off
set architecture i386:x86-64
target remote 127.0.0.1:1234
# With -S, CPU starts at the firmware reset vector; symbols are not yet loaded.
# First boot once with `run`, copy IMAGE.BASE from serial, then use:
# add-symbol-file build/stage0.so -o 0xIMAGE_BASE
# hbreak efi_main
# continue
# hbreak stage0_post_ebs
# continue
# Use hardware breakpoints before code is loaded into RAM.
# If the second boot logs a different base, reload symbols at that actual base.
