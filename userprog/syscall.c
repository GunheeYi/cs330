#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
	// ASSERT(0);
	uint64_t a1 = f->R.rdi;
	uint64_t a2 = f->R.rsi;
	uint64_t a3 = f->R.rdx;
	uint64_t a4 = f->R.r10;
	uint64_t a5 = f->R.r8;
	uint64_t a6 = f->R.r9;

	// printf("----------------%d--------------------------------------------\n", f->R.rax);

	switch (f->R.rax) {
		case SYS_HALT: haltt(); break;
		case SYS_EXIT: exitt((int) a1); break;
		case SYS_FORK: f->R.rax = forkk((const char*) a1); break;
		case SYS_EXEC: f->R.rax = execc((const char*) a1); break;
		case SYS_WAIT: f->R.rax = waitt((pid_t) a1); break;
		case SYS_CREATE: f->R.rax = createe((const char*) a1, (unsigned) a2); break;
		case SYS_REMOVE: break;
		case SYS_OPEN: f->R.rax = openn((const char*) a1); break;
		case SYS_FILESIZE: f->R.rax = filesizee((int) a1); break;
		case SYS_READ: f->R.rax = (uint32_t) readd((int) a1, (void*) a2, (unsigned) a3); break;
		case SYS_WRITE: f->R.rax = (uint32_t) writee((int) a1, (const void*) a2, (unsigned) a3); break;
		case SYS_SEEK: break;
		case SYS_TELL: break;
		case SYS_CLOSE: closee((int) a1); break;
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
	// thread_exit ();/
}

void haltt() {
	power_off();
}
void exitt(int status) {

	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf ("%s: exit(%d)\n", curr->name, curr->exit_status);

	thread_exit();
}

pid_t forkk(const char *thread_name) {
	return process_fork(thread_name);
}

int execc(const char *file) {
	if (process_exec(file) < 0 ){
		return -1;
	}
}
int waitt(pid_t pid) {
	return process_wait();
}
bool createe(const char *file, unsigned initial_size) {
	// if null pointer / virtual address for file is not mapped / file is empty
	if (file == NULL || is_not_mapped(file) || file[0] == NULL) exitt(-1);

	// if file name too long
	if ( strlen(file) > NAME_MAX ) return false;
	

	return (filesys_create(file, initial_size));
}
bool removee(const char *file) {
	if (!filesys_remove(file)) return false;
	
	// remove file descriptor
	// add dir to fd?

	return true;

	// "removing an open file does not close it"
	// 주의해야하나???
}
int openn(const char *file) {
	// null pointer for file name / file name virtual address not mapped
	if ( file==NULL || is_not_mapped(file) ) exitt(-1);
	// empty file
	if ( file[0]==NULL ) return -1;
	
	struct file* fp = filesys_open(file);
	if (fp==NULL) return -1;
	
	struct thread* curr = thread_current();
	// use palloc instead of initializing struct fd directly????
	struct fm* new_file_map = palloc_get_page(0);
	// will palloc_get_page() ever fail? if so, should we immediately free the page back?????????
	new_file_map->fd = curr->fd_next;
	new_file_map->fp = fp;
	// new_file_map->elem = (struct list_elem) NULL; // unnecessary????

	list_push_back(&curr->fm_list, &new_file_map->elem);

	return curr->fd_next++;
}
int filesizee(int fd) {
	return file_length(get_fm(fd)->fp);
}
int readd(int fd, void *buffer, unsigned size) {
	// null pointer for buffer / buffer virtual address not mapped
	if ( buffer==NULL || is_not_mapped(buffer) ) {
		exitt(-1);
	}
	// requested to read for size of 0
	if ( size==0 ) {
		return 0;
	}
	// trying to read from stdout
	if ( fd==1 ){
		return -1;
	} 
	// reading from
	// stdin
	if ( fd==0 ) {
		return input_getc();
	}
	// file
	else {		
		struct fm* fm = get_fm(fd);
		if ( fm==NULL ) {
			exitt(-1);
		} // fd has not been issued (bad)
		
		return file_read(fm->fp, buffer, size);
	}
}
int writee(int fd, const void *buffer, unsigned size) {

	// requested to write for size of 0
	if ( size==0 ) return 0;

	// trying to write to stdin
	if ( fd==0 ) return -1;

	// virtual address for buffer is not mapped
	if ( is_not_mapped(buffer) ) exitt(-1);
	
	// writing to 
	// stdout
	if ( fd==1 ) {
		putbuf(buffer, size);
		return size;
	}
	// file
	else {
		struct fm* fm = get_fm(fd);
		if ( fm==NULL ) exitt(-1); // fd has not been issued (bad)
		return file_write(fm->fp, buffer, size);
	}
}
void seekk(int fd, unsigned position) {

}
unsigned telll(int fd) {

}
void closee(int fd) {
	struct fm* fm = get_fm(fd);
	if ( get_fm(fd)==NULL ) exitt(-1); // fd has not been issued (bad)
	file_close(fm->fp);
	list_remove(&fm->elem);
	palloc_free_page(fm);
}
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

struct fm* get_fm(int fd) {
	struct thread* t = thread_current();
	struct fm* fm;
	for (struct list_elem *e = list_begin(&t->fm_list); e != list_end (&t->fm_list); e = list_next(e))
	{
		fm = list_entry (e, struct fm, elem);
		// printf("Current fd: %d-----------------------------------------\n", fm->fd);
		if (fm->fd==fd) return fm;
	}
	return NULL;
}

bool is_not_mapped(uint64_t va) {
	return pml4e_walk(thread_current()->pml4, va, false) == NULL;
}