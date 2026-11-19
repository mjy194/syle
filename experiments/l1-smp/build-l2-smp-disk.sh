#!/bin/sh
# Usage: build-l2-smp-disk.sh bzImage base-root.ext2 gnu-efi-prefix gen_init_cpio new-output-dir [L2-bzImage]
set -eu
[ "$#" = 5 ] || [ "$#" = 6 ]
case "${L2_CLOCKEVENT:-lapic}" in
    lapic) mode=--smp-shell ;;
    tsc-deadline) mode=--smp-shell-deadline ;;
    *) echo 'L2_CLOCKEVENT must be lapic or tsc-deadline' >&2; exit 2 ;;
esac
busybox=$(realpath "${BUSYBOX:-$(command -v busybox)}")
[ -x "$busybox" ]
if readelf -l "$busybox" | grep -q INTERP; then
    echo 'L2 requires a statically linked BusyBox (set BUSYBOX)' >&2
    exit 1
fi
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
mkdir -- "$5"
out=$(cd -- "$5" && pwd)
musl-gcc -static -Wall -Wextra -Werror -Os "$repo/experiments/l1-smp/l2-smp-init.c" -o "$out/l2-init"
musl-gcc -static -Wall -Wextra -Werror -Os "$repo/experiments/l1-smp/l2-clock.c" -o "$out/l2-clock"
# gen_init_cpio creates the guest device nodes without privileges on the host.
cat >"$out/initramfs.list" <<LIST
dir /dev 0755 0 0
dir /bin 0755 0 0
dir /proc 0755 0 0
dir /sys 0755 0 0
nod /dev/console 0600 0 0 c 5 1
nod /dev/kmsg 0600 0 0 c 1 11
nod /dev/ttyS0 0600 0 0 c 4 64
file /bin/busybox $busybox 0755 0 0
file /bin/l2-clock $out/l2-clock 0755 0 0
slink /bin/sh busybox 0777 0 0
slink /bin/cat busybox 0777 0 0
slink /bin/ls busybox 0777 0 0
slink /bin/uname busybox 0777 0 0
slink /bin/halt busybox 0777 0 0
file /init $out/l2-init 0755 0 0
LIST
"$4" "$out/initramfs.list" >"$out/l2-initrd"
kvm="$repo/experiments/m4-linux-kvm"
cc -static -pthread -Os -Wall -Wextra -Werror -I"$repo/krnl" -DLB_RAM_SIZE=50331648 \
    "$kvm/l2-linux.c" "$kvm/l2-disk.c" "$repo/krnl/virtio_blk/rw_backend.c" \
    "$repo/krnl/linux_boot.c" "$repo/krnl/linux_acpi_topology.c" -o "$out/l2-run"
printf '#!/bin/sh\n/l2-run /l2-kernel /l2-initrd %s\n' "$mode" >"$out/smp-init"
cat >>"$out/smp-init" <<'INIT'
rc=$?
echo "L2_SMP_RUNNER_DONE rc=$rc"
while :; do /bin/busybox sleep 3600; done
INIT
chmod 755 "$out/smp-init"
cp -- "$2" "$out/root.ext2"
cp -- "${6:-$1}" "$out/l2-kernel"
(
    cd "$out"
    debugfs -w -f /dev/stdin root.ext2 <<'COMMANDS'
write smp-init /sbin/smp-init
write l2-run /l2-run
write l2-kernel /l2-kernel
write l2-initrd /l2-initrd
COMMANDS
)
UEFI_DISK_CFLAGS='-DSYLE_DISK_LINUX -DSYLE_DISK_ROOT -DSYLE_DISK_SMP_PROBE' \
    sh "$repo/guest/uefi-disk/build.sh" "$out/efi" "$3"
python3 "$repo/guest/uefi-disk/make-disk.py" --efi "$out/efi/BOOTX64.EFI" \
    --linux "$1" --rootfs "$out/root.ext2" --output "$out/seed.img"
