#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?usage: build.sh output-file}
${CC:-cc} -Os -Wall -Wextra -Werror -ffreestanding -fno-stack-protector -fno-pie \
 -nostdlib -static -no-pie -Wl,--build-id=none "$here/probe.c" "$here/start.S" -o "$out"
