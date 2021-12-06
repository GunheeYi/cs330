#ifndef FILESYS_FAT_H
#define FILESYS_FAT_H

#include "devices/disk.h"
#include "filesys/file.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>

typedef uint32_t cluster_t;  /* Index of a cluster within FAT. */

#define FAT_MAGIC 0xEB3C9000 /* MAGIC string to identify FAT disk */
#define EOChain 0x0FFFFFFF   /* End of cluster chain */

/* Sectors of FAT information. */
#define SECTORS_PER_CLUSTER 1 /* Number of sectors per cluster */
#define FAT_BOOT_SECTOR 0     /* FAT boot sector. */
#define ROOT_DIR_CLUSTER 1    /* Cluster for the root directory */

/* Should be less than DISK_SECTOR_SIZE */
struct fat_boot {
	unsigned int magic;
	unsigned int sectors_per_cluster; /* Fixed to 1 */
	unsigned int total_sectors;
	unsigned int fat_start;
	unsigned int fat_sectors; /* Size of FAT in sectors. */
	unsigned int root_dir_cluster;
};

/* FAT FS */
struct fat {
	struct fat_boot bs;
	unsigned int *fat;
	unsigned int fat_length;
	disk_sector_t data_start;
	cluster_t last_clst;
	struct lock write_lock;
};

struct diskk {
	struct disk* disk;
	struct fat* fat;
};


void fat_init (struct diskk* diskk);
void fat_open (struct diskk* diskk);
void fat_close (struct diskk* diskk);
void fat_create (struct diskk* diskk);
void fat_close (struct diskk* diskk);

cluster_t fat_create_chain (
    struct diskk* diskk, 
    cluster_t clst /* Cluster # to stretch, 0: Create a new chain */
);
void fat_remove_chain (
    struct diskk* diskk, 
    cluster_t clst, /* Cluster # to be removed */
    cluster_t pclst /* Previous cluster of clst, 0: clst is the start of chain */
);
cluster_t fat_get (struct diskk* diskk, cluster_t clst);
void fat_put (struct diskk* diskk, cluster_t clst, cluster_t val);
disk_sector_t cluster_to_sector (struct diskk* diskk, cluster_t clst);
cluster_t sector_to_cluster (struct diskk* diskk, disk_sector_t sect);

cluster_t fat_find_empty(struct diskk* diskk);
bool fat_enough_space(struct diskk* diskk, size_t need);

#endif /* filesys/fat.h */
