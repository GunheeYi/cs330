#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#ifdef EFILESYS
	#include "filesys/fat.h"
	#include "threads/thread.h"
#endif

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();

	fat_open ();

	thread_current()->curr_dir = dir_open_root();
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
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0;
#ifdef EFILESYS
	struct dir *dir = dir_reopen(thread_current()->curr_dir);
#else
	struct dir *dir = dir_open_root();
#endif
#ifdef EFILESYS
	cluster_t clst = fat_create_chain(0);
	
	inode_sector = cluster_to_sector(clst);
	char* path = malloc(sizeof(char)*PATH_MAX);
	strlcpy(path, name, strlen(name)+1);
	if (path[strlen(path)-1]=='/') {
		path[strlen(path)-1] = '\0';
	}
	char* ptr_slash = strrchr(path, '/');
	if (ptr_slash!=NULL) {
		*ptr_slash = '\0';
		struct inode* inode = NULL;
		if (!dir_lookup(thread_current()->curr_dir, path, &inode) || inode==NULL) {
			return false;
		}
		// dir_close(dir); ?
		dir = dir_open(inode);
		path = ptr_slash + 1;
		ASSERT(path!="");
	}
	bool success = (dir != NULL
			&& inode_create (inode_sector, initial_size, INODE_FILE)
			&& dir_add (dir, path, inode_sector));
	if (!success) {
		fat_remove_chain(clst, 0);
	}

#else
	bool success = (dir != NULL
			&& free_map_allocate (1, &inode_sector)
			&& inode_create (inode_sector, initial_size)
			&& dir_add (dir, name, inode_sector));
	if (!success && inode_sector != 0)
		free_map_release (inode_sector, 1);
#endif
	dir_close (dir);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
void*
filesys_open (const char *name, bool* is_dir) {
#ifdef EFILESYS
	if (thread_current()->curr_dir == NULL){
		ASSERT(0);
	}
	ASSERT(thread_current()->curr_dir!=NULL); // ??
	struct dir *dir = dir_reopen(thread_current()->curr_dir); 
#else
	struct dir *dir = dir_open_root();
#endif
	struct inode *inode = NULL;

	if (dir != NULL) {
		if (!dir_lookup (dir, name, &inode)) {
			return NULL;
		}
	}

	dir_close (dir); // ??? - reopen해줬으니 닫는게 맞을 듯

	// dir_lookup이 실패한 경우에 대한 handling은 필요 없나?
	
	// inode type에 따라 적합한 open()을 부르고 void*에 cast함
	bool is_dir_ = (inode->data.type==INODE_DIR);
	if (is_dir!=NULL) {
		*is_dir = is_dir_;
	}
	return (void*) (is_dir_ ? dir_open(inode) : file_open (inode));
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
#ifdef EFILESYS
	struct dir *dir = dir_reopen(thread_current()->curr_dir);
#else
	struct dir *dir = dir_open_root();
#endif
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	if (!dir_create (cluster_to_sector(ROOT_DIR_CLUSTER), 16))
		PANIC ("err");
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}