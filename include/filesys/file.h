#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H
#include "filesys/inode.h"
#include "filesys/off_t.h"
#include "filesys/filesys.h"

struct inode;

/* An open file. */
struct file {
	struct inode *inode;        /* File's inode. */
	off_t pos;                  /* Current position. */
	bool deny_write;            /* Has file_deny_write() been called? */
};

/* Opening and closing files. */
struct file *file_open (struct diskk*, struct inode*);
struct file *file_reopen (struct diskk*, struct file *);
struct file *file_duplicate (struct diskk*, struct file*);
void file_close (struct diskk*, struct file*);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct diskk*, struct file*, void*, off_t);
off_t file_read_at (struct diskk*, struct file*, void*, off_t, off_t);
off_t file_write (struct diskk*, struct file*, const void*, off_t);
off_t file_write_at (struct diskk*, struct file*, const void*, off_t, off_t);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */
