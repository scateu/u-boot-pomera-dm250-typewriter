// SPDX-License-Identifier: GPL-2.0+
/* Initramfs-less hibernation resume for Pomera SD boot. */
#include <blk.h>
#include <command.h>
#include <env.h>
#include <malloc.h>
#include <memalign.h>
#include <part.h>
#include <stdio.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/string.h>

#include "resume.h"

#define SECTOR_SIZE	512
#define SWAP_PAGE_SIZE	4096
#define SWAP_SIGNATURE_SIZE	10

static bool has_arg(const char *args, const char *key, const char *value)
{
	size_t key_len = strlen(key), value_len = value ? strlen(value) : 0;
	const char *end;
	bool found = false;

	while (*args) {
		while (isspace(*args))
			args++;
		end = args;
		while (*end && !isspace(*end))
			end++;
		if (end - args >= key_len && !strncmp(args, key, key_len)) {
			/* Match Linux's __setup prefix handling for noresume. */
			if (!value)
				return true;
			/* Linux uses the last root= when it is specified twice. */
			found = end - args == key_len + value_len &&
				!strncmp(args + key_len, value, value_len);
		}
		args = end;
	}
	return found;
}

enum pomera_resume_state {
	POMERA_NO_RESUME,
	POMERA_SWAP,
	POMERA_HIBERNATE,
};

static enum pomera_resume_state resume_state(struct blk_desc *desc)
{
	ALLOC_CACHE_ALIGN_BUFFER(u8, sector, SECTOR_SIZE);
	struct disk_partition part;
	const char *sig = (char *)sector + SECTOR_SIZE - SWAP_SIGNATURE_SIZE;
	const u32 blocks = SWAP_PAGE_SIZE / SECTOR_SIZE;

	/* All three armhf configurations use 4 KiB pages and SD p3 for swap. */
	if (!desc || desc->blksz != SECTOR_SIZE ||
	    part_get_info(desc, 3, &part) || part.blksz != SECTOR_SIZE ||
	    part.size < blocks || part.start >= desc->lba ||
	    part.size > desc->lba - part.start)
		return POMERA_NO_RESUME;
	if (blk_dread(desc, part.start + blocks - 1, 1, sector) != 1)
		return POMERA_NO_RESUME;

	if (!memcmp(sig, "S1SUSPEND", SWAP_SIGNATURE_SIZE))
		return POMERA_HIBERNATE;
	if (!memcmp(sig, "SWAPSPACE2", SWAP_SIGNATURE_SIZE))
		return POMERA_SWAP;
	return POMERA_NO_RESUME;
}

int pomera_bootz(struct blk_desc *desc, ulong kernel, ulong fdt)
{
	const char *args = env_get("bootargs");
	const char *suffix;
	char command[96], *saved = NULL, *updated;
	enum pomera_resume_state state;
	size_t size;
	int ret;

	/* Respect custom roots and explicit resume/noresume choices. */
	if (args && has_arg(args, "root=", "/dev/mmcblk1p2") &&
	    !has_arg(args, "resume=", NULL) && !has_arg(args, "noresume", NULL)) {
		state = resume_state(desc);
		if (state == POMERA_NO_RESUME)
			goto boot;
		/*
		 * systemd needs a resume device on ordinary boots too.
		 * Only wait indefinitely when there is an image to restore.
		 */
		suffix = state == POMERA_HIBERNATE ?
			" resume=/dev/mmcblk1p3 resumewait" :
			" resume=/dev/mmcblk1p3";
		saved = strdup(args);
		if (!saved)
			return -ENOMEM;
		size = strlen(saved) + strlen(suffix) + 1;
		updated = malloc(size);
		if (!updated) {
			free(saved);
			return -ENOMEM;
		}
		snprintf(updated, size, "%s%s", saved, suffix);
		ret = env_set("bootargs", updated);
		free(updated);
		if (ret) {
			free(saved);
			return ret;
		}
		puts(state == POMERA_HIBERNATE ?
		     "Pomera: hibernation image found on /dev/mmcblk1p3\n" :
		     "Pomera: resume target /dev/mmcblk1p3\n");
	}

boot:
	/* '-' means no initramfs; Linux resumes before mounting its rootfs. */
	snprintf(command, sizeof(command), "bootz %lx - %lx", kernel, fdt);
	ret = run_command(command, 0);
	if (saved) {
		/* A failed boot or a retry must not retain another card's target. */
		if (env_set("bootargs", saved)) {
			puts("Pomera: failed to restore bootargs after bootz\n");
			ret = -EIO;
		}
		free(saved);
	}
	return ret;
}
