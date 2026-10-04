/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __POMERA_LEGACY_UBOOT_H
#define __POMERA_LEGACY_UBOOT_H

#include <linux/types.h>

/* Adjust only the verified RAM copy, never the eMMC image. */
int pomera_legacy_uboot_fixup(void *image, u32 size, u32 crc);

#endif
