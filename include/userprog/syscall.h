#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

// Includes added from here.
#include "threads/init.h"
#include "filesys/file.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "vm/vm.h"

void syscall_init (void);

void haltt();
void exitt(int status);
pid_t forkk(const char *thread_name, struct intr_frame* f);
int execc(const char *file);
int waitt(pid_t pid);
bool createe(const char *file, unsigned initial_size);
bool removee(const char *file);
int openn(const char *file);
int filesizee(int fd);
int readd(int fd, void *buffer, unsigned size);
int writee(int fd, const void *buffer, unsigned size);
void seekk(int fd, unsigned position);
unsigned telll(int fd);
void closee(int fd);
void* mmapp(void *addr, size_t length, int writable, int fd, off_t offset);
void munmapp(void *addr);
// bool chdirr();
// bool mkdirr();
// bool readdirr();
// bool isdirr();
// int inumberr();
// int symlinkk();
// int mountt();
// int umountt();

struct lock lock_file;

struct fm* get_fm(int fd);
bool is_not_mapped(uint64_t va);
void close_fm(struct fm* fm);
void check_buffer(const void *buffer, unsigned size, bool write);

#endif /* userprog/syscall.h */
