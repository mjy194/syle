# Source this for the current cloud workspace's official Debian package extraction.
# This is a convenience path, not a requirement for building elsewhere.
DEPS_ROOT=${DEPS_ROOT:-/workspace/shared/qemu-deps/root}
export EFI_PREFIX="$DEPS_ROOT/usr"
export QEMU="$DEPS_ROOT/usr/bin/qemu-system-x86_64"
export GDB="$DEPS_ROOT/usr/bin/gdb"
export QEMU_DATA="$DEPS_ROOT/usr/share/qemu"
export OVMF_CODE="$DEPS_ROOT/usr/share/OVMF/OVMF_CODE_4M.fd"
export OVMF_VARS="$DEPS_ROOT/usr/share/OVMF/OVMF_VARS_4M.fd"
export LD_LIBRARY_PATH="$DEPS_ROOT/usr/lib/x86_64-linux-gnu:$DEPS_ROOT/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

export VGA_ROM="$DEPS_ROOT/usr/share/seabios/vgabios-stdvga.bin"
