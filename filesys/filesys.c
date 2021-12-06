#define EFILESYS

#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct diskk filesys_diskk;

static void do_format (struct diskk* diskk);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (struct diskk* diskk, int chan_no, int dev_no, bool format) {
	diskk->disk = disk_get (chan_no, dev_no);
	if (diskk->disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init (diskk);

	if (format)
		do_format (diskk);

	fat_open (diskk);

	thread_current()->curr_dir = dir_open_root(diskk);
	dir_add(diskk, thread_current()->curr_dir, ".", cluster_to_sector(diskk, ROOT_DIR_CLUSTER));
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (struct diskk* diskk) {
	/* Original FS */
#ifdef EFILESYS
	fat_close (diskk);
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (struct diskk* diskk, const char *path, off_t initial_size) {
	disk_sector_t inode_sector = 0;
#ifdef EFILESYS
	struct dir *dir = dir_reopen(diskk, thread_current()->curr_dir);
#else
	struct dir *dir = dir_open_root();
#endif
#ifdef EFILESYS
	if (dir==NULL) {
		return false;
	}

	struct dir* parent_dir;
	char* name = malloc(sizeof(char)*(NAME_MAX));
	if (strchr(path, '/')==NULL) {
		parent_dir = dir_reopen(diskk, dir);
		strlcpy(name, path, strlen(path)+1);
	} else if (!dir_parse(diskk, dir, path, &parent_dir, &name)) {
		return false;
	}

	cluster_t clst = fat_create_chain(diskk, 0);
	if (clst==0) {
		return false;
	}
	inode_sector = cluster_to_sector(diskk, clst);
	
	bool success = ( !dir_removed(diskk, parent_dir)
			&& inode_create (diskk, inode_sector, initial_size, NULL, INODE_FILE)
			&& dir_add (diskk, parent_dir, name, inode_sector) );
	if (!success) {
		fat_remove_chain(diskk, clst, 0);
	}

	if (parent_dir!=dir) {
		dir_close(diskk, parent_dir);
	}
	
#else
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
#endif
	dir_close (diskk, dir);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
void* filesys_open (struct diskk* diskk, const char *name, enum inode_type* type) {
#ifdef EFILESYS
	struct dir *dir = dir_reopen(diskk, thread_current()->curr_dir); 
#else
	struct dir *dir = dir_open_root();
#endif
	struct inode *inode = NULL;	

	if (dir==NULL
		|| !dir_lookup (diskk, dir, name, &inode)
		|| dir_removed(diskk, dir)
	) {
		return NULL;
	}

	dir_close (diskk, dir); // ??? - reopen해줬으니 닫는게 맞을 듯
	if (inode->data.type==INODE_LINK) {
		return filesys_open(diskk, inode->data.target, type);
	}

	// dir_lookup이 실패한 경우에 대한 handling은 필요 없나?
	
	// inode type에 따라 적합한 open()을 부르고 void*에 cast함
	enum inode_type type_ = inode->data.type;
	if (type!=NULL) {
		*type = type_;
	}


	return (type_==INODE_DIR ? (void*) dir_open(diskk, inode) : (void*) file_open (diskk, inode));
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (struct diskk* diskk, const char *path) {
#ifdef EFILESYS
	struct dir *dir = dir_reopen(diskk, thread_current()->curr_dir);
#else
	struct dir *dir = dir_open_root();
#endif
	if (dir==NULL) {
		return false;
	}
	struct dir* parent_dir;
	char* name = malloc(sizeof(char)*(NAME_MAX));
	if (!dir_parse(diskk, dir, path, &parent_dir, &name)) {
		return false;
	}
	bool success = dir_remove (diskk, parent_dir, name);
	if (parent_dir!=dir) {
		dir_close(diskk, parent_dir);
	}
	dir_close (diskk, dir);

	return success;
}

/* Formats the file system. */
static void
do_format (struct diskk* diskk) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create (diskk);
	if (!dir_create (diskk, cluster_to_sector(diskk, ROOT_DIR_CLUSTER), 16))
		PANIC ("err");
	fat_close (diskk);
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}