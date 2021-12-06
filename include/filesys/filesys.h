#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#ifdef EFILESYS
	#include "filesys/fat.h"
	#include "threads/thread.h"
#endif

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

struct diskk {
	int chan;
	int dev;
	struct disk* disk;
	struct list* ois;
	struct fat_fs* fat;
	struct list_elem elem;
};

/* Disk used for file system. */
extern struct disk *filesys_disk;
struct list diskk_list;

void filesys_init (int chan, int dev, bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
void* filesys_open (const char *name, enum inode_type* type);
bool filesys_remove (const char *name);

#endif /* filesys/filesys.h */
