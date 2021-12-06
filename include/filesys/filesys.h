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

/* Disk used for file system. */
extern struct diskk filesys_diskk;

void filesys_init (struct diskk* diskk, int chan_no, int dev_no, bool format);
void filesys_done (struct diskk* diskk);
bool filesys_create (struct diskk* diskk, const char *path, off_t initial_size);
void* filesys_open (struct diskk* diskk, const char *name, enum inode_type* type);
bool filesys_remove (struct diskk* diskk, const char *name);

#endif /* filesys/filesys.h */
