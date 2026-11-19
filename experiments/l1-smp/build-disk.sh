#!/bin/sh
# Usage: build-disk.sh bzImage base-root.ext2 gnu-efi-prefix new-output-dir [--nested|--concurrent|--migrate]
set -eu
case "$#:${5:-}" in
    4:|5:--nested|5:--concurrent|5:--migrate) ;;
    *) exit 2 ;;
esac
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
mkdir -- "$4"
out=$(cd -- "$4" && pwd)
flags=
if [ "$#" = 5 ]; then
    flags=-DSMP_NESTED_PROBE
    case "$5" in
        --concurrent) flags="$flags -DSMP_CONCURRENT_PROBE" ;;
        --migrate) flags="$flags -DSMP_MIGRATION_PROBE" ;;
    esac
    cc -static -Os -Wall -Wextra -Werror "$repo/experiments/m4-linux-kvm/l2-probe.c" -o "$out/l2-probe"
fi
musl-gcc $flags -static -Wall -Wextra -Werror -Os "$repo/experiments/l1-smp/smp-init.c" -o "$out/smp-init"
cp -- "$2" "$out/root.ext2"
(cd "$out" && debugfs -w -R 'write smp-init /sbin/smp-init' root.ext2 &&
 debugfs -R 'dump /sbin/smp-init verify-init' root.ext2 && cmp smp-init verify-init)
if [ -n "$flags" ]; then
    (cd "$out" && debugfs -w -R 'write l2-probe /sbin/smp-l2-probe' root.ext2 &&
     debugfs -R 'dump /sbin/smp-l2-probe verify-l2' root.ext2 && cmp l2-probe verify-l2)
fi
UEFI_DISK_CFLAGS='-DSYLE_DISK_LINUX -DSYLE_DISK_ROOT -DSYLE_DISK_SMP_PROBE' \
    sh "$repo/guest/uefi-disk/build.sh" "$out/efi" "$3"
python3 "$repo/guest/uefi-disk/make-disk.py" --efi "$out/efi/BOOTX64.EFI" \
    --linux "$1" --rootfs "$out/root.ext2" --output "$out/seed.img"
