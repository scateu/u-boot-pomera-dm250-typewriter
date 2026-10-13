#!/bin/sh

set -eu

usage()
{
    echo "Usage: $0 {dm200|dm250|dm250us} <whole-SD-device|existing-SD-image>" >&2
    exit 2
}

fail()
{
    echo "ERROR: $*" >&2
    exit 1
}

[ "$#" -eq 2 ] || usage
case "$1" in
    dm200|dm250|dm250us) target=$1 ;;
    *) usage ;;
esac

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
image=$source_dir/uboot_${target}.img
destination=$(readlink -f -- "$2") || fail "Cannot resolve destination: $2"

# The 8 MiB absolute offset is for the Pomera SD boot layout, not raw eMMC.
image_limit=$((4 * 1024 * 1024))
boot_area=$((30 * 1024 * 1024))
[ -f "$image" ] && [ -r "$image" ] && [ -s "$image" ] ||
    fail "Missing, unreadable or empty image: $image"
[ ! "$destination" -ef "$image" ] || fail "Destination is the input image: $image"
image_size=$(stat -c '%s' -- "$image")
[ "$image_size" -le "$image_limit" ] || fail "U-Boot image exceeds the 4 MiB slot"

# Validate the four 1 MiB copies emitted by tools/rockchip/loaderimage.c.
# --unpack does not validate the header, CRC or hash, so it is not a verifier.
python3 - "$image" <<'PY'
import hashlib
import struct
import sys
from pathlib import Path

try:
    data = Path(sys.argv[1]).read_bytes()
    copy_size = 1024 * 1024
    header_size = 2048
    if len(data) != 4 * copy_size:
        raise ValueError("expected four 1 MiB LOADER copies")
    first = data[:copy_size]
    if first[:16] != b"LOADER  " + bytes(8):
        raise ValueError("invalid LOADER tag")
    address, size, stored_crc, hash_len = struct.unpack_from("<4I", first, 16)
    if address != 0x60200000:
        raise ValueError("unexpected Pomera load address")
    if not 0 < size <= copy_size - header_size:
        raise ValueError("invalid LOADER payload size")
    if hash_len != 20:
        raise ValueError("expected loaderimage SHA-1 hash")
    payload = first[header_size:header_size + size]
    table = []
    for index in range(256):
        value = index << 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04C10DB7 if value & 0x80000000 else 0)) & 0xFFFFFFFF
        table.append(value)
    crc = 0
    for byte in payload:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ table[(crc >> 24) ^ byte]
    if crc != stored_crc:
        raise ValueError("LOADER CRC mismatch")
    digest = hashlib.sha1(payload + struct.pack("<3I", address, size, hash_len)).digest()
    if digest != first[32:32 + hash_len]:
        raise ValueError("LOADER SHA-1 mismatch")
    for index in range(1, 4):
        if data[index * copy_size:(index + 1) * copy_size] != first:
            raise ValueError(f"LOADER copy {index + 1} differs")
except (OSError, ValueError) as error:
    print(f"ERROR: {error}", file=sys.stderr)
    sys.exit(1)
print(f"PASS: {sys.argv[1]}: LOADER copies=4 payload={size} crc=0x{crc:08x}")
PY

[ -w "$destination" ] || fail "Destination is not writable: $destination"
PATH=$PATH:/usr/sbin:/sbin
export PATH

check_device()
{
    device_type=$(lsblk -dn -o TYPE -- "$destination") ||
        fail "Cannot inspect destination"
    case "$device_type" in
        disk|loop) ;;
        *) fail "Specify the whole SD device, not a partition or mapped device" ;;
    esac
    read_only=$(blockdev --getro "$destination")
    [ "$read_only" -eq 0 ] || fail "Destination is read-only"
    mountpoints=$(lsblk -nr -o MOUNTPOINTS -- "$destination") ||
        fail "Cannot inspect mounts"
    case "$mountpoints" in
        *[![:space:]]*) fail "Unmount all destination filesystems and disable its swap first" ;;
    esac
    types=$(lsblk -nr -o TYPE -- "$destination") ||
        fail "Cannot inspect dependent devices"
    for type in $types; do
        case "$type" in
            disk|loop|part) ;;
            *) fail "Destination has an active mapped device: $type" ;;
        esac
    done
    starts=$(lsblk -nr -o START -- "$destination") ||
        fail "Cannot inspect partition boundaries"
    for start in $starts; do
        [ "$start" -ge 61440 ] ||
            fail "A partition overlaps the reserved first 30 MiB"
    done
}

check_image()
{
    # An image used by a loop device may contain a live mounted filesystem.
    loops=$(losetup -j "$destination") || fail "Cannot inspect image loop devices"
    [ -z "$loops" ] || fail "Detach loop devices using the destination image first"
}

if [ -b "$destination" ]; then
    check_device
    destination_size=$(blockdev --getsize64 "$destination")
elif [ -f "$destination" ]; then
    check_image
    destination_size=$(stat -c '%s' -- "$destination")
else
    fail "Destination must be a block device or an existing regular SD image"
fi
[ "$destination_size" -ge "$boot_area" ] ||
    fail "Destination is smaller than the reserved 30 MiB boot area"

printf 'Model: %s\nU-Boot: %s (%s bytes) at 8 MiB\n' "$target" "$image" "$image_size"
printf 'WARNING: overwriting the SD boot area of %s (%s bytes).\n' \
    "$destination" "$destination_size"
echo "This does not update IDB, kernel, resource or rootfs. Do not use raw eMMC."
printf 'Type the full destination path to continue: '
IFS= read -r confirmation || fail "Confirmation required"
[ "$confirmation" = "$destination" ] || fail "Cancelled; nothing written"

# Recheck mounts after the confirmation prompt; do not mount during the update.
if [ -b "$destination" ]; then
    check_device
fi
if [ -f "$destination" ]; then
    check_image
fi
dd if="$image" of="$destination" bs=1M seek=8 conv=notrunc,fsync status=progress
echo "Written: $destination"
