#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "user/syscall.h"

void syscall_init (void);

void haltt();
void exitt(int status);
pid_t forkk(const char *thread_name);
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
// int dup22();
// void* mmapp();
// void munmapp();
// bool chdirr();
// bool mkdirr();
// bool readdirr();
// bool isdirr();
// int inumberr();
// int symlinkk();
// int mountt();
// int umountt();

#endif /* userprog/syscall.h */
