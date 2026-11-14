#!/bin/sh
set -eu
cd "$(dirname "$0")"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
localize() {
 nm -g --defined-only "$1" | awk '$3 ~ /^(pmm_raw_|pmm_owned_|vlm_)/ {print $3}' > "$tmp/private"
 test -s "$tmp/private"
 objcopy --localize-symbols="$tmp/private" "$1" "$2"
 if nm -g --defined-only "$2" | grep -E ' (pmm_raw_|pmm_owned_|vlm_)'; then
  echo 'private PMM symbol leak' >&2; exit 1
 fi
}
# Native artifact compiles real BSP/IF guard and full LGM capacity; no native run.
# Explicit no-LTO: localization contract is defined only for ELF object symbols.
# Material kernel ABI flags match efi/Makefile KFLAGS (no EFI headers needed).
cc -std=c11 -Wall -Wextra -Werror -pedantic -O1 -ffreestanding -fshort-wchar -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pic -fno-pie -mcmodel=small -DSYLE_KERNEL -fno-lto -c native_candidate.c -o "$tmp/native.raw.o"
localize "$tmp/native.raw.o" "$tmp/native.o"
objdump -d "$tmp/native.o" | grep -q rdmsr
if nm -g --defined-only "$tmp/native.o" | grep -q pna_test_; then exit 1; fi
bytes=$(nm -S "$tmp/native.o" | awk '$4 == "authority" {print $2}')
test "$((0x$bytes))" -ge "$((32837*40))"
# Host uses mock guard, small bounded capacity and actual PMM core.
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -DVLM_SLOTS=4 -std=c11 -Wall -Wextra -Werror -pedantic -fno-lto -fsanitize=address,undefined -g -c native_candidate.c -o "$tmp/host.raw.o"
localize "$tmp/host.raw.o" "$tmp/host.o"
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -DVLM_SLOTS=4 -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g native_candidate_test.c "$tmp/host.o" -o "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/test" fragmentation
cc -DPMM_TEST_PHYS_BITS=36 -DPNA_HOST_TEST -DVLM_SLOTS=4 -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g create_context.c create_context_test.c "$tmp/host.o" -o "$tmp/context_test"
ASAN_OPTIONS=detect_leaks=0 "$tmp/context_test"
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g create_context.c create_context_failure_test.c -o "$tmp/context_failure"
ASAN_OPTIONS=detect_leaks=0 "$tmp/context_failure"
cc -std=c11 -Wall -Wextra -Werror -pedantic -ffreestanding -mgeneral-regs-only -mno-red-zone -c create_context.c -o "$tmp/context.o"
# A separate client must not resolve private raw/owner/model entry points.
for symbol in pmm_raw_free_page pmm_raw_alloc_page pmm_raw_init pmm_owned_release vlm_init; do
 printf 'extern void %s(void); int main(void){%s();return 0;}\n' "$symbol" "$symbol" > "$tmp/escape.c"
 if cc -fsanitize=address,undefined "$tmp/escape.c" "$tmp/host.o" -o "$tmp/escape" 2>"$tmp/link.err"; then
  echo "private symbol unexpectedly linked: $symbol" >&2; exit 1
 fi
 grep -q "$symbol" "$tmp/link.err"
done
# Unchanged PMM tests through actual candidate public wrappers, mocked guard.
cc -DPMM_TEST_PHYS_BITS=36 -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -g ../../krnl/tests/memory.c "$tmp/host.o" -o "$tmp/compat"
for mode in normal fragmentation zero single; do
 if [ "$mode" = normal ]; then ASAN_OPTIONS=detect_leaks=0 "$tmp/compat"; else ASAN_OPTIONS=detect_leaks=0 "$tmp/compat" "$mode"; fi
done
echo 'optional PMM private ELF symbol and native-compile gates: PASS (no native execution)'
