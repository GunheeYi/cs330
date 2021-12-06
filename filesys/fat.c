#include "filesys/fat.h"

void fat_boot_create (struct diskk* diskk);
void fat_fs_init (struct diskk* diskk);

void
fat_init (struct diskk* diskk) {
	diskk->fat = calloc (1, sizeof (struct fat));
	if (diskk->fat == NULL)
		PANIC ("FAT init failed");

	// Read boot sector from the disk
	unsigned int *bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT init failed");
	disk_read (diskk->disk, FAT_BOOT_SECTOR, bounce);
	memcpy (&diskk->fat->bs, bounce, sizeof (diskk->fat->bs));
	free (bounce);

	// Extract FAT info
	if (diskk->fat->bs.magic != FAT_MAGIC)
		fat_boot_create (diskk->fat);
	fat_fs_init (diskk->fat);
}

void
fat_open (struct diskk* diskk) {
	diskk->fat->fat = calloc (diskk->fat->fat_length, sizeof (cluster_t));
	if (diskk->fat->fat == NULL)
		PANIC ("FAT load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *) diskk->fat->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof (diskk->fat->fat);
	const off_t fat_size_in_bytes = diskk->fat->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < diskk->fat->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (diskk->disk, diskk->fat->bs.fat_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (diskk->disk, diskk->fat->bs.fat_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}
}

void
fat_close (struct diskk* diskk) {
	// Write FAT boot sector
	uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT close failed");
	memcpy (bounce, &diskk->fat->bs, sizeof (diskk->fat->bs));
	disk_write (diskk->disk, FAT_BOOT_SECTOR, bounce);
	free (bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *) diskk->fat->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof (diskk->fat->fat);
	const off_t fat_size_in_bytes = diskk->fat->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < diskk->fat->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (diskk->disk, diskk->fat->bs.fat_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (diskk->disk, diskk->fat->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}
}

void
fat_create (struct diskk* diskk) {
	// Create FAT boot
	fat_boot_create (diskk);
	fat_fs_init (diskk);

	// Create FAT table
	diskk->fat->fat = calloc (diskk->fat->fat_length, sizeof (cluster_t));
	if (diskk->fat->fat == NULL)
		PANIC ("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put (diskk->fat, ROOT_DIR_CLUSTER, EOChain);

	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc (1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC ("FAT create failed due to OOM");
	disk_write (diskk->disk, cluster_to_sector (diskk->fat, ROOT_DIR_CLUSTER), buf);
	free (buf);
}

void
fat_boot_create (struct diskk* diskk) {
	unsigned int fat_sectors =
	    (disk_size (diskk->disk) - 1)
	    / (DISK_SECTOR_SIZE / sizeof (cluster_t) * SECTORS_PER_CLUSTER + 1) + 1;
	diskk->fat->bs = (struct fat_boot){
	    .magic = FAT_MAGIC,
	    .sectors_per_cluster = SECTORS_PER_CLUSTER,
	    .total_sectors = disk_size (diskk->disk),
	    .fat_start = 1,
	    .fat_sectors = fat_sectors,
	    .root_dir_cluster = ROOT_DIR_CLUSTER,
	};
}

void
fat_fs_init (struct diskk* diskk) {
	/* TODO: Your code goes here. */
	// fat_fs->fat_length = fat_fs->bs.fat_sectors * DISK_SECTOR_SIZE / (sizeof(cluster_t) * SECTORS_PER_CLUSTER);
	diskk->fat->fat_length = (diskk->fat->bs.total_sectors - diskk->fat->bs.fat_sectors) / SECTORS_PER_CLUSTER;
	diskk->fat->data_start = diskk->fat->bs.fat_start + diskk->fat->bs.fat_sectors; // in sectors
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

cluster_t fat_find_empty(struct diskk* diskk) {
	cluster_t i = 1;
	while(i < diskk->fat->fat_length) {
		if (fat_get(diskk, i)==0) {
			return i;
		}
		i++;
	}
	return 0;
}

bool fat_enough_space(struct diskk* diskk, size_t need) {
	cluster_t i = 1;
	size_t num = 0;
	while(i < diskk->fat->fat_length) {
		if (num >= need) {
			return true;
		}
		if (fat_get(diskk, i)==0) {
			num++;
		}
		i++;
	}
	return false;
}

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain (struct diskk* diskk, cluster_t clst) {
	/* TODO: Your code goes here. */
	cluster_t new = fat_find_empty(diskk);
	if (new==0) {
		return 0;
	}

	fat_put(diskk, new, EOChain);

	if (clst!=0) {
		ASSERT(fat_get(diskk, clst)==EOChain);
		fat_put(diskk, clst, new);
	}

	return new;
}

/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
void
fat_remove_chain (struct diskk* diskk, cluster_t clst, cluster_t pclst) {
	/* TODO: Your code goes here. */
	if (pclst!=0) {
		fat_put(diskk, pclst, EOChain);
	}

	while (true) {
		cluster_t next = fat_get(diskk, clst);
		fat_put(diskk, clst, 0);
		if(next==EOChain) {
			return;
		}
		clst = next;
	}
}

/* Update a value in the FAT table. */
void
fat_put (struct diskk* diskk, cluster_t clst, cluster_t val) {
	/* TODO: Your code goes here. */
	diskk->fat->fat[clst] = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get (struct diskk* diskk, cluster_t clst) {
	/* TODO: Your code goes here. */
	return diskk->fat->fat[clst];
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector (struct diskk* diskk, cluster_t clst) {
	/* TODO: Your code goes here. */
	return diskk->fat->data_start + clst * SECTORS_PER_CLUSTER;
}

cluster_t
sector_to_cluster (struct diskk* diskk, disk_sector_t sect) {
	return (sect - diskk->fat->data_start) / SECTORS_PER_CLUSTER;
}