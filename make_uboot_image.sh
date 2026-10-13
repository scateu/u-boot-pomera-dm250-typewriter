#!/bin/sh

set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 {dm200|dm250|dm250us}" >&2
    exit 2
fi

case "$1" in
    dm200)
        defconfig=pomera-dm200_defconfig
        ;;
    dm250)
        defconfig=pomera-dm250_defconfig
        ;;
    dm250us)
        defconfig=pomera-dm250us_defconfig
        ;;
    *)
        echo "Usage: $0 {dm200|dm250|dm250us}" >&2
        exit 2
        ;;
esac

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=$source_dir/build
cross_compile=${CROSS_COMPILE:-arm-linux-gnueabihf-}
jobs=${JOBS:-$(nproc)}
output=$source_dir/uboot_${1}.img

make -C "$source_dir" O="$build_dir" \
    CROSS_COMPILE="$cross_compile" \
    "$defconfig"

make -C "$source_dir" O="$build_dir" \
    CROSS_COMPILE="$cross_compile" \
    -j"$jobs"

"$build_dir/tools/loaderimage" --pack \
    "$build_dir/u-boot.bin" \
    "$output" \
    0x60200000

echo "Created: $output"
