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
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (int chan, int dev, bool format) {
	struct diskk* new_diskk = malloc(sizeof(struct diskk));
	new_diskk->chan = chan;
	new_diskk->dev = dev;
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL) {
		free(new_diskk);
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");
	}
	new_diskk->disk = filesys_disk;
	
	new_diskk->ois = inode_init ();

#ifdef EFILESYS
	new_diskk->fat = fat_init ();

	if (format)
		do_format ();

	fat_open ();

	thread_current()->curr_dir = dir_open_root();
	dir_add(thread_current()->curr_dir, ".", cluster_to_sector(ROOT_DIR_CLUSTER));

	list_push_back(&diskk_list, &new_diskk);

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
filesys_create (const char *path, off_t initial_size) {
	disk_sector_t inode_sector = 0;
#ifdef EFILESYS
	struct dir *dir = dir_reopen(thread_current()->curr_dir);
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
		parent_dir = dir_reopen(dir);
		strlcpy(name, path, strlen(path)+1);
	} else if (!dir_parse(dir, path, &parent_dir, &name)) {
		return false;
	}

	cluster_t clst = fat_create_chain(0);
	if (clst==0) {
		return false;
	}
	inode_sector = cluster_to_sector(clst);
	
	bool success = ( !dir_removed(parent_dir)
			&& inode_create (inode_sector, initial_size, NULL, INODE_FILE)
			&& dir_add (parent_dir, name, inode_sector) );
	if (!success) {
		fat_remove_chain(clst, 0);
	}

	if (parent_dir!=dir) {
		dir_close(parent_dir);
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
void* filesys_open (const char *name, enum inode_type* type) {
#ifdef EFILESYS
	struct dir *dir = dir_reopen(thread_current()->curr_dir); 
#else
	struct dir *dir = dir_open_root();
#endif
	struct inode *inode = NULL;	

	if (dir==NULL
		|| !dir_lookup (dir, name, &inode)
		|| dir_removed(dir)
	) {
		return NULL;
	}

	dir_close (dir); // ??? - reopen해줬으니 닫는게 맞을 듯
	if (inode->data.type==INODE_LINK) {
		return filesys_open(inode->data.target, type);
	}

	// dir_lookup이 실패한 경우에 대한 handling은 필요 없나?
	
	// inode type에 따라 적합한 open()을 부르고 void*에 cast함
	enum inode_type type_ = inode->data.type;
	if (type!=NULL) {
		*type = type_;
	}


	return (type_==INODE_DIR ? (void*) dir_open(inode) : (void*) file_open (inode));
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *path) {
#ifdef EFILESYS
	struct dir *dir = dir_reopen(thread_current()->curr_dir);
#else
	struct dir *dir = dir_open_root();
#endif
	if (dir==NULL) {
		return false;
	}
	struct dir* parent_dir;
	char* name = malloc(sizeof(char)*(NAME_MAX));
	if (!dir_parse(dir, path, &parent_dir, &name)) {
		return false;
	}
	bool success = dir_remove (parent_dir, name);
	if (parent_dir!=dir) {
		dir_close(parent_dir);
	}
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