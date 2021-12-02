#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "threads/malloc.h"

/* A single directory entry. */
struct dir_entry {
	disk_sector_t inode_sector;         /* Sector number of header. */
	char name[NAME_MAX + 1];            /* Null terminated file name. */
	bool in_use;                        /* In use or free? */
};

// path string을 받아 (상위 directory)/(directory 혹은 파일)로 나누고,
// current directory에서 (상위 directory)로 타고 들어간 결과를 parsed_dir에, (directory 혹은 파일) string을 name에 씀
bool dir_parse(struct dir* current_dir, const char* path_, struct dir** parsed_dir, char** name) {
	ASSERT(current_dir!=NULL);
	if (path_[0]=='\0') return false;
	if (path_[0]=='/') {
		return dir_parse(dir_open_root(), path_+1, parsed_dir, name);
	}
	char* path = malloc(sizeof(char)*PATH_MAX);
	strlcpy(path, path_, strlen(path_)+1);
	if (path[strlen(path)-1]=='/') {
		path[strlen(path)-1] = '\0';
		return dir_parse(current_dir, path, parsed_dir, name);
	}
	// 여기까지 온 path는 시작과 끝에 '/'를 가질 수 없음

	char* ptr_slash = strrchr(path, '/'); // path 뒤에서부터 '/'의 인덱스를 찾음
	
	// path의 중간에 '/'가 있는 경우.
	if (ptr_slash!=NULL) {
		// 마지막 '/'를 기준으로 path와 path_last로 잘라서, 
		*ptr_slash = '\0';
		char* path_last = ptr_slash + 1;
		ASSERT(path[0]!='\0');
		// current_dir에서 path를 찾아 들어가고
		struct inode* inode = NULL;
		if (!dir_lookup(current_dir, path, &inode) || inode==NULL) {
			return false;
		}
		// 다시 call
		return dir_parse(dir_open(inode), path_last, parsed_dir, name);
	}
	*parsed_dir = current_dir;
	*name = path;
	return true;
}

// dir 혹은 그 상위 directory들이 remove되었는지 확인함
bool dir_removed(struct dir* dir) {
	struct dir* child_dir = dir_reopen(dir);
	while (child_dir->inode->sector!=cluster_to_sector(ROOT_DIR_CLUSTER)) {
		struct inode** inode;
		ASSERT(dir_lookup(child_dir, "..", &inode));
		struct dir* parent_dir = dir_open(inode);
		struct dir_entry e;
		bool found = false;
		for (off_t ofs = 0; inode_read_at (parent_dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e) {
			if (e.inode_sector==child_dir->inode->sector) {
				found = true;
				dir_close(child_dir);
				if (!e.in_use) {
					dir_close(parent_dir);
					return true;
				}
				child_dir = parent_dir;
				break;
			}
		}
		ASSERT(found);
	}
	dir_close(child_dir);
	return false;
}

/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) {
	return inode_create (sector, entry_cnt * sizeof (struct dir_entry), INODE_DIR);
}

/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) {
	struct dir *dir = calloc (1, sizeof *dir);
	if (inode != NULL && dir != NULL) {
		dir->inode = inode;
		dir->pos = 0;
		return dir;
	} else {
		inode_close (inode);
		free (dir);
		return NULL;
	}
}

/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct dir *
dir_open_root (void) {
#ifdef EFILESYS
	return dir_open (inode_open (cluster_to_sector(ROOT_DIR_CLUSTER)));
#else
	return dir_open (inode_open (ROOT_DIR_SECTOR));
#endif
}

/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) {
	return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) {
	if (dir != NULL) {
		inode_close (dir->inode);
		free (dir);
		dir=NULL; // ??
	}
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) {
	return dir->inode;
}

/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
		struct dir_entry *ep, off_t *ofsp) {
	struct dir_entry e;
	size_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (e.in_use && !strcmp (name, e.name)) {
			if (ep != NULL)
				*ep = e;
			if (ofsp != NULL)
				*ofsp = ofs;
			return true;
		}
	return false;
}

/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
		struct inode **inode) {
	struct dir_entry e;
	ASSERT (dir != NULL);
	ASSERT (name != NULL);
	if (name[0]=='\0') {
		*inode = dir->inode;
		return *inode!=NULL;
	}
	// name is "/" or "/~"
	if (name[0]=='/') {
		return dir_lookup(dir_open_root(), name+1, inode);
	}

	// char name_first[PATH_MAX];
	char* name_first = malloc(sizeof(char)*PATH_MAX); // free on where ????
	strlcpy(name_first, name, strlen(name)+1);

	if (name_first[strlen(name_first)-1]=='/') {
		name_first[strlen(name_first)-1] ='\0';
		return dir_lookup(dir, name_first, inode);
	}

	char* ptr_slash = strchr(name_first, '/');

	if (ptr_slash==NULL) {
		if (!lookup (dir, name_first, &e, NULL)) {
			*inode = NULL; // necessary?
			return false;
		}
		ASSERT(e.in_use);
		*inode = inode_open (e.inode_sector);
		return *inode != NULL;
	}

	*ptr_slash = '\0';
	char* name_last = ptr_slash + 1;
	
	// name is "a" or "a/"

	ASSERT(name_first[0]!='\0');

	// name is "a/b"
	struct inode *inode_child = NULL;
	if (!dir_lookup (dir, name_first, &inode_child)) {
		return false;
	}
	struct dir* dir_child;
	if (!(dir_child = dir_open(inode_child))) {
		return false;
	}
	return dir_lookup(dir_child, name_last, inode);

	// Symlink, soft link..?
	
}

/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) {
	struct dir_entry e;
	off_t ofs;
	bool success = false;
	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Check NAME for validity. */
	if (*name == '\0' || strlen (name) > NAME_MAX) {
		return false;
	}

	/* Check that NAME is not in use. */
	if (lookup (dir, name, NULL, NULL))
		goto done;
	/* Set OFS to offset of free slot.
	 * If there are no free slots, then it will be set to the
	 * current end-of-file.

	 * inode_read_at() will only return a short read at end of file.
	 * Otherwise, we'd need to verify that we didn't get a short
	 * read due to something intermittent such as low memory. */
	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (!e.in_use)
			break;
	/* Write slot. */
	e.in_use = true;
	strlcpy (e.name, name, sizeof e.name);
	e.inode_sector = inode_sector;
	success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
	return success;
}

/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) {
	struct dir_entry e;
	struct inode *inode = NULL;
	bool success = false;
	off_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Find directory entry. */
	if (!lookup (dir, name, &e, &ofs))
		goto done;

	/* Open inode. */
	inode = inode_open (e.inode_sector);
	if (inode == NULL)
		goto done;
	
	/* Fail if entry exists, only when removing a directory */
	if (inode->data.type==INODE_DIR) {
		struct dir* dir_to_remove = dir_open(inode);
		char entry_name[READDIR_MAX_LEN + 1];
		bool has_entry = dir_readdir(dir_to_remove, entry_name);
		dir_close(dir_to_remove);
		if (has_entry) {
			goto done;
		}
	}
	

	/* Erase directory entry. */
	e.in_use = false;
	if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
		goto done;

	/* Remove inode. */
	inode_remove (inode);
	success = true;

done:
	inode_close (inode);
	return success;
}

/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1]) {
	struct dir_entry e;

	while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
		dir->pos += sizeof e;
		if (strcmp(e.name, ".") && strcmp(e.name, "..") && e.in_use) {
			strlcpy (name, e.name, NAME_MAX + 1);
			return true;
		}
	}
	return false;
}
