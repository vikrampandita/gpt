
/* Author: Vikram Pandita <vikram.pandita@ti.com> */
/* Sources borrowed from TI u-boot: omap4_dev on omapzoom */

#include <linux/types.h>
#include <linux/fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>


#define EFI_VERSION 0x00010000
#define EFI_ENTRIES 128
#define EFI_NAMELEN 36

static const __u8 partition_type[16] = {
	0xa2, 0xa0, 0xd0, 0xeb, 0xe5, 0xb9, 0x33, 0x44,
	0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7,
};

//chrom-kernel guid
static const __u8 chrome_kernel_guid[16] = {
	0x5d, 0x2a, 0x3a, 0xfe, 0x32, 0x4f, 0xa7, 0x41,
	0xb7, 0x25, 0xac, 0xcc, 0x32, 0x85, 0xa3, 0x09
};
//chrom-rootfs guid
static const __u8 chrome_rootfs_guid[16] = {
	0x02, 0xe2, 0xb8, 0x3c, 0x7e, 0x3b, 0xdd, 0x47,
	0x8a, 0x3c, 0x7f, 0xf2, 0xa1, 0x3c, 0xfc, 0xec
};

static const __u8 random_uuid[16] = {
	0xff, 0x1f, 0xf2, 0xf9, 0xd4, 0xa8, 0x0e, 0x5f,
	0x97, 0x46, 0x59, 0x48, 0x69, 0xae, 0xc3, 0x4e,
};

static int print_ptable(int fd);

struct efi_entry {
	__u8 type_uuid[16];
	__u8 uniq_uuid[16];
	__u64 first_lba;
	__u64 last_lba;
	__u64 attr;
	__u16 name[EFI_NAMELEN];
};

struct efi_header {
	__u8 magic[8];

	__u32 version;
	__u32 header_sz;

	__u32 crc32;
	__u32 reserved;

	__u64 header_lba;
	__u64 backup_lba;
	__u64 first_lba;
	__u64 last_lba;

	__u8 volume_uuid[16];

	__u64 entries_lba;

	__u32 entries_count;
	__u32 entries_size;
	__u32 entries_crc32;
} __attribute__((packed));

struct ptable {
	__u8 mbr[512];
	union {
		struct efi_header header;
		__u8 block[512];
	};
	struct efi_entry entry[EFI_ENTRIES];
};

static void init_mbr(__u8 *mbr, __u32 blocks)
{
	mbr[0x1be] = 0x00; // nonbootable
	mbr[0x1bf] = 0xFF; // bogus CHS
	mbr[0x1c0] = 0xFF;
	mbr[0x1c1] = 0xFF;

	mbr[0x1c2] = 0xEE; // GPT partition
	mbr[0x1c3] = 0xFF; // bogus CHS
	mbr[0x1c4] = 0xFF;
	mbr[0x1c5] = 0xFF;

	mbr[0x1c6] = 0x01; // start
	mbr[0x1c7] = 0x00;
	mbr[0x1c8] = 0x00;
	mbr[0x1c9] = 0x00;

	memcpy(mbr + 0x1ca, &blocks, sizeof(__u32));

	mbr[0x1fe] = 0x55;
	mbr[0x1ff] = 0xaa;
}

static void start_ptbl(struct ptable *ptbl, unsigned blocks)
{
	struct efi_header *hdr = &ptbl->header;

	memset(ptbl, 0, sizeof(*ptbl));

	init_mbr(ptbl->mbr, blocks - 1);

	memcpy(hdr->magic, "EFI PART", 8);
	hdr->version = EFI_VERSION;
	hdr->header_sz = sizeof(struct efi_header);
	hdr->header_lba = 1;
	hdr->backup_lba = blocks - 1;
	hdr->first_lba = 34;
	hdr->last_lba = blocks - 1;
	memcpy(hdr->volume_uuid, random_uuid, 16);
	hdr->entries_lba = 2;
	hdr->entries_count = EFI_ENTRIES;
	hdr->entries_size = sizeof(struct efi_entry);
}

static void end_ptbl(struct ptable *ptbl)
{
	struct efi_header *hdr = &ptbl->header;
	__u32 n;

	n = crc32(0, 0, 0);
	n = crc32(n, (void*) ptbl->entry, sizeof(ptbl->entry));
	hdr->entries_crc32 = n;

	n = crc32(0, 0, 0);
	n = crc32(0, (void*) &ptbl->header, sizeof(ptbl->header));
	hdr->crc32 = n;
}

int add_ptn(struct ptable *ptbl, __u64 first, __u64 last, const char *name)
{
	struct efi_header *hdr = &ptbl->header;
	struct efi_entry *entry = ptbl->entry;
	unsigned n;

	if (first < 34) {
		printf("partition '%s' overlaps partition table\n", name);
		return -1;
	}

	if (last > hdr->last_lba) {
		printf("partition '%s' does not fit\n", name);
		return -1;
	}
	//printf("\n add part - %s\n", name);

	for (n = 0; n < EFI_ENTRIES; n++, entry++) {
		if (entry->last_lba)
			continue;
		if (!strcmp(name, "kernel"))
			memcpy(entry->type_uuid, chrome_kernel_guid, 16);
		else if (!strcmp(name, "rootfs"))
			memcpy(entry->type_uuid, chrome_rootfs_guid, 16);
		else
			memcpy(entry->type_uuid, partition_type, 16);

		memcpy(entry->uniq_uuid, random_uuid, 16);
		entry->uniq_uuid[0] = n;
		entry->first_lba = first;
		entry->last_lba = last;
		for (n = 0; (n < EFI_NAMELEN) && *name; n++)
			entry->name[n] = *name++;
		return 0;
	}
	printf("out of partition table entries\n");
	return -1;
}

struct partition {
	const char *name;
	unsigned size_kb;
};

#if 0
static struct partition partitions[] = {
	{ "-", 128 },
	{ "xloader", 128 },
	{ "bootloader", 256 },
	{ "STATE", 10*1024 },
	{ "kernel", 16*1024 },
	{ "rootfs", 90*1024 },
	{ "kernelb", 16*1024 },
	{ "rootfsb", 100*1024 },
	{ "kernelc", 16*1024 },
	{ "rootfsc", 100*1024 },
	{ "OEM", 16*1024 },
	{ "reserve1", 512 },
	{ "reserve2", 512 },
	{ "reserve3", 512 },
	{ "EFI-SYSTEM", 16*1024},
	{ 0, 0 },
};
#else
static struct partition partitions[] = {
#if 1
	{ "-", 512 },
#else
	{ "-", 128 },
	{ "xloader", 128 },
	{ "bootloader", 256 },
#endif
	{ "STATE", 1024*1024 },
	{ "kernel", 16*1024 },
	{ "rootfs", 900*1024 },
	{ "kernelb", 16*1024 },
	{ "rootfsb", 500*1024 },
	{ "kernelc", 16*1024 },
	{ "rootfsc", 500*1024 },
	{ "OEM", 16*1024 },
	{ "reserve1", 512 },
	{ "reserve2", 512 },
	{ "reserve3", 512 },
	{ "EFI-SYSTEM", 16*1024},
	{ 0, 0 },
};
#endif
static struct ptable the_ptable;

static int do_format(int fd)
{
	struct ptable *ptbl = &the_ptable;
	unsigned sector_sz, blocks;
	unsigned next;
	int n;

	if (ioctl(fd, BLKGETSIZE, &blocks) < 0) {
		printf("\n cannot read blksize on fd=%d\n", fd);
		return -1;
	}
	printf("blocks %d\n", blocks);

	start_ptbl(ptbl, blocks);
	n = 0;
	next = 0;
	for (n = 0, next = 0; partitions[n].name; n++) {
		unsigned sz = partitions[n].size_kb * 2;
		if (!strcmp(partitions[n].name,"-")) {
			next += sz;
			continue;
		}
		if (!strncmp(partitions[n].name,"reserve", 7)) {
			sz = sz / 1024;
		}
		if (sz == 0)
			sz = blocks - next;
		if (add_ptn(ptbl, next, next + sz - 1, partitions[n].name))
			return -1;
		next += sz;
	}
	end_ptbl(ptbl);

	if (lseek(fd, 0, SEEK_SET) < 0) {
		printf("\n seek error \n");
		return -1;
	}
	if (write(fd, (void*) ptbl, sizeof(struct ptable)) < 0) {
		printf("\n ptable write failure \n");
		return -1;
	}

	printf("\nWritten new partition table...\n");
	//print_ptable(fd);

	return 0;
}

void print_efi_partition(struct efi_entry *entry)
{
	int n;
	__u32 start, length;
	__u8 name[100];

	for (n = 0; n < (sizeof(name)-1); n++)
		name[n] = entry->name[n];
	name[n] = 0;
	start = entry->first_lba;
	length = (entry->last_lba - entry->first_lba + 1) * 512;

	if (length && start) {
	if (length > 0x100000)
		printf("%8d %7dM %s\n", start,
			(__u32)(length/0x100000), name);
	else
		if (length > 0x400)
			printf("%8d %7dK %s\n", start,
				(__u32)(length/0x400), name);
		else
			printf("%8d %7dB %s\n", start,
				(__u32)(length), name);
	}
}

static int print_ptable(int fd)
{
	struct efi_entry *entry;
	int n, gpt_size;
	unsigned char *buf = NULL;

	/* Alloc EFI sectors: 34 sectors */
	gpt_size = (1 + 1 + 32)*512;
	buf = malloc(gpt_size);
	if (!buf)
		goto fail;

	if (lseek(fd, 0, SEEK_SET) < 0) {
		printf("\n seek error \n");
		goto fail;
	}

	n = read(fd, buf, gpt_size);

	if (memcmp(buf + 512, "EFI PART", 8)) {
		printf("efi partition table not found\n");
		goto fail;
	}

	entry = (struct efi_entry *)(buf + 512 + 512);

	printf("\n EFI table is:\n");
	for (n = 0; n < 128; n++) {
		print_efi_partition(entry + n);
	}

fail:
	free(buf);
	return 0;
}


main(int argc, char **argv)
{
	int fd, i, n;
	struct stat s;
	unsigned long total_sectors;

	printf("\n Open file: %s\n", argv[1]);

	if (!strcmp(argv[1], "/dev/sda")) {
		printf("\n Damn risky: is this your hard-disk %s\n", argv[1]);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0)
		goto fail;

	memset(&s, 0, sizeof (s));
	if (fstat(fd, &s))
		goto fail;

	//printf(" st_size=%lld, st_blksize=%lu, st_blocks=%lu\n",
	//		s.st_size,
	//		s.st_blksize,
	//		s.st_blocks);


	//write GPT table
	if (argc == 3 && !strcmp(argv[2], "-w")) {
		do_format(fd);
		print_ptable(fd);
		return 0;
	} else {
		print_ptable(fd);
		return 0;
	}

fail:
	printf("\n bad file [%s]\n", argv[1]);
	return -1;
}
