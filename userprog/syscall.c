#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
// Includes added from here.
#include "threads/init.h"
#include <filesys/filesys.h>

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	uint64_t a1 = f->R.rdi;
	uint64_t a2 = f->R.rsi;
	uint64_t a3 = f->R.rdx;
	uint64_t a4 = f->R.r10;
	uint64_t a5 = f->R.r8;
	uint64_t a6 = f->R.r9;

	switch (f->R.rax) {
		case SYS_HALT: haltt(); break;
		case SYS_EXIT: exitt(); break;
		case SYS_FORK: forkk(); break;
		case SYS_EXEC: execc(); break;
		case SYS_WAIT: waitt(); break;
		case SYS_CREATE: createe(); break;
		case SYS_REMOVE: removee(); break;
		case SYS_OPEN: openn(); break;
		case SYS_FILESIZE: filesizee(); break;
		case SYS_READ: readd(); break;
		case SYS_WRITE: writee(); break;
		case SYS_SEEK: seekk(); break;
		case SYS_TELL: telll(); break;
		case SYS_CLOSE: closee(); break;
		// case SYS_DUP2: dup22(); break;
		// case SYS_MMAP: mmapp(); break;
		// case SYS_MUNMAP: munmapp(); break;
		// case SYS_CHDIR: chdirr(); break;
		// case SYS_MKDIR: mkdirr(); break;
		// case SYS_READDIR: readdirr(); break;
		// case SYS_ISDIR: isdirr(); break;
		// case SYS_INUMBER: inumberr(); break;
		// case SYS_SYMLINK: symlinkk(); break;
		// case SYS_MOUNT: mountt(); break;
		// case SYS_UMOUNT: umountt(); break;
	}

	// printf ("system call!\n");
	// thread_exit ();
}

void haltt() {
	power_off();
};
void exitt(int status);
pid_t forkk(const char *thread_name);
int execc(const char *file);
int waitt(pid_t pid);
bool createe(const char *file, unsigned initial_size) {
	return (filesys_create(file, initial_size));
};
bool removee(const char *file) {
	return filesys_remove(file);
	// "removing an open file does not close it"
	// 주의해야하나???
};
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
