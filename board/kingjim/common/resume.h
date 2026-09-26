/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __POMERA_RESUME_H
#define __POMERA_RESUME_H

struct blk_desc;

/* Boot the SD kernel, using initialized p3 and waiting only for an image. */
int pomera_bootz(struct blk_desc *desc, ulong kernel, ulong fdt);

#endif
