#define EFILESYS

#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"
#include "filesys/inode.h"
#ifdef EFILESYS
	#include "filesys/fat.h"
#endif

/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14
#define PATH_MAX 124 * sizeof(uint32_t) / sizeof(char)

struct inode;

/* A directory. */
struct dir {
	struct inode *inode;                /* Backing store. */
	off_t pos;                          /* Current position. */
};

bool dir_parse(struct diskk* diskk, struct dir* current_dir, const char* path_, struct dir** parsed_dir, char** name);
bool dir_removed(struct diskk* diskk, struct dir* dir);

/* Opening and closing directories. */
bool dir_create (struct diskk* diskk, disk_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct diskk* diskk, struct inode *);
struct dir *dir_open_root (struct diskk* diskk);
struct dir *dir_reopen (struct diskk* diskk, struct dir *);
void dir_close (struct diskk* diskk, struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (struct diskk* diskk, const struct dir *, const char *name, struct inode **);
bool dir_add (struct diskk* diskk, struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct diskk* diskk, struct dir *, const char *name);
bool dir_readdir (struct diskk* diskk, struct dir *, char name[NAME_MAX + 1]);

#endif /* filesys/directory.h */
