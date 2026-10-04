// SPDX-License-Identifier: GPL-2.0+
/* StorageInit() in the DM250 stock loader tries SD before eMMC. */
#include <stdio.h>
#include <asm/unaligned.h>
#include <linux/errno.h>

#include "legacy_uboot.h"

#define LEGACY_BASE		0x60200000
#define DM250_IMAGE_SIZE		0x51df8
#define DM250_STORAGE_TABLE	0x3fe04
#define DM250_SD_OPS		0x476e8
#define DM250_EMMC_OPS		0x4772c
#define DM250_NAND_OPS		0x477b4
#define DM250_SD_RELOC		0x50904
#define DM250_EMMC_RELOC		0x5090c
#define R_ARM_RELATIVE		23

int pomera_legacy_uboot_fixup(void *image, u32 size, u32 crc)
{
	u8 *buf = image;
	u32 sd = LEGACY_BASE + DM250_SD_OPS;
	u32 emmc = LEGACY_BASE + DM250_EMMC_OPS;

	/* The inspected DM200 firmware already tries eMMC first, without SD. */
	if (size == 0x50368 && crc == 0x67cca333)
		return 0;

	/* These JP/US binaries have the same storage code and pointer layout. */
	if (size != DM250_IMAGE_SIZE ||
	    (crc != 0xb72955b3 && crc != 0x1983b8e4)) {
		printf("Pomera: unsupported legacy U-Boot (size=%u, CRC=%08x)\n",
		       size, crc);
		return -EOPNOTSUPP;
	}

	/* Check the original table, device identities and relative relocations. */
	if (get_unaligned_le32(buf + DM250_STORAGE_TABLE) != sd ||
	    get_unaligned_le32(buf + DM250_STORAGE_TABLE + 4) != emmc ||
	    get_unaligned_le32(buf + DM250_STORAGE_TABLE + 8) !=
					LEGACY_BASE + DM250_NAND_OPS ||
	    get_unaligned_le32(buf + DM250_SD_OPS) != 0x00040000 ||
	    get_unaligned_le32(buf + DM250_EMMC_OPS) != 0x00020002 ||
	    get_unaligned_le32(buf + DM250_SD_RELOC) !=
					LEGACY_BASE + DM250_STORAGE_TABLE ||
	    get_unaligned_le32(buf + DM250_SD_RELOC + 4) != R_ARM_RELATIVE ||
	    get_unaligned_le32(buf + DM250_EMMC_RELOC) !=
					LEGACY_BASE + DM250_STORAGE_TABLE + 4 ||
	    get_unaligned_le32(buf + DM250_EMMC_RELOC + 4) != R_ARM_RELATIVE)
		return -EBADMSG;

	/* R_ARM_RELATIVE adds the relocation delta to the swapped values. */
	put_unaligned_le32(emmc, buf + DM250_STORAGE_TABLE);
	put_unaligned_le32(sd, buf + DM250_STORAGE_TABLE + 4);
	puts("Pomera: legacy storage order: eMMC before SD\n");
	return 0;
}
