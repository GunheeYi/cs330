#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#ifdef EFILESYS
	#include <string.h>
#endif

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

	lock_init(&lock_file);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {

	// TODO: Your implementation goes here.
	thread_current()->rsp = f->rsp;

	uint64_t a1 = f->R.rdi;
	uint64_t a2 = f->R.rsi;
	uint64_t a3 = f->R.rdx;
	uint64_t a4 = f->R.r10;
	uint64_t a5 = f->R.r8;
	uint64_t a6 = f->R.r9;

	switch (f->R.rax) {
		case SYS_HALT: haltt(); break;
		case SYS_EXIT: exitt((int) a1); break;
		case SYS_FORK: f->R.rax = forkk((const char*) a1, f); break;
		case SYS_EXEC: f->R.rax = execc((const char*) a1); break;
		case SYS_WAIT: f->R.rax = waitt((pid_t) a1); break;
		case SYS_CREATE: f->R.rax = createe((const char*) a1, (unsigned) a2); break;
		case SYS_REMOVE: f->R.rax = removee((const char*) a1); break;
		case SYS_OPEN: f->R.rax = openn((const char*) a1); break;
		case SYS_FILESIZE: f->R.rax = filesizee((int) a1); break;
		case SYS_READ: f->R.rax = (uint32_t) readd((int) a1, (void*) a2, (unsigned) a3); break;
		case SYS_WRITE: f->R.rax = (uint32_t) writee((int) a1, (const void*) a2, (unsigned) a3); break;
		case SYS_SEEK: seekk((int) a1, (unsigned) a2); break;
		case SYS_TELL: f->R.rax = telll((int) a1); break;
		case SYS_CLOSE: closee((int) a1); break;
		case SYS_MMAP: f->R.rax = mmapp((void*) a1, (size_t) a2, (int) a3, (int) a4, (off_t) a5); break;
		case SYS_MUNMAP: munmapp((void*) a1); break;
		case SYS_CHDIR: f->R.rax = chdirr((const char*) a1); break;
		case SYS_MKDIR: f->R.rax = mkdirr((const char*) a1); break;
		case SYS_READDIR: f->R.rax = readdirr((int) a1, (char) a2); break;
		case SYS_ISDIR: f->R.rax = isdirr((int) a1); break;
		case SYS_INUMBER: f->R.rax = inumberr((int) a1); break;
		// case SYS_SYMLINK: symlinkk(); break;
		// case SYS_MOUNT: mountt(); break;
		// case SYS_UMOUNT: umountt(); break;
	}
}

void haltt() { power_off(); }

void exitt(int status) {
	struct thread *curr = thread_current();
	curr->exit_status = status;
	printf ("%s: exit(%d)\n", curr->name, curr->exit_status);

	thread_exit();
}

pid_t forkk(const char *thread_name, struct intr_frame* f) { return process_fork(thread_name, f); }

bool is_not_mapped(uint64_t va) {
	bool not_mapped = pml4e_walk(thread_current()->pml4, va, false) == NULL;
	return not_mapped;
}

int execc(const char *file) {
	if (is_not_mapped(file) || process_exec(file) < 0) exitt(-1); // is bad ptr or process_exec() not successful
}

int waitt(pid_t pid) { return process_wait(); }
	
bool createe(const char *file, unsigned initial_size) {
	if (file == NULL || is_not_mapped(file) || file[0] == NULL) exitt(-1); // if null pointer / virtual address for file is not mapped / file is empty
	if ( strlen(file) > NAME_MAX ) return false; // if file name too long
	
	return filesys_create(file, initial_size);
}

bool removee(const char *file) { return filesys_remove(file); }

int openn(const char *path) {
	if ( path==NULL || is_not_mapped(path) ) exitt(-1); // null pointer for file name / file name virtual address not mapped
	if ( path[0]==NULL ) return -1; // empty file

	lock_acquire(&lock_file);
	bool is_dir;
	void* fdp = filesys_open(path, &is_dir); // file pointer or directory pointer
	lock_release(&lock_file);
	
	if (fdp==NULL) {
		// lock_release(&lock_file);
		return -1;
	}

	struct thread* curr = thread_current();

	if (list_size(&curr->fm_list) > 135) {
		// lock_release(&lock_file);
		return -1;
	}

	// struct fm* new_file_map = palloc_get_page(PAL_USER);
	struct fm* new_file_map = (struct fm*)malloc(sizeof(struct fm));
	if (new_file_map==NULL) {
		// lock_release(&lock_file);
		return -1;
	}
	new_file_map->fd = curr->fd_next;
	new_file_map->fdp = fdp;
	new_file_map->is_dir = is_dir;
	new_file_map->copied_fd = -1;
	new_file_map->file_exists = true;
	list_push_back(&curr->fm_list, &new_file_map->elem);

	// lock_release(&lock_file);
	return curr->fd_next++;
}

struct fm* get_fm(int fd) {
	struct thread* t = thread_current();
	struct fm* fm;
	for (struct list_elem *e = list_begin(&t->fm_list); e != list_end (&t->fm_list); e = list_next(e))
	{
		fm = list_entry (e, struct fm, elem);
		if (fm->fd==fd) return fm;
	}
	return NULL;
}

int filesizee(int fd) {
	int length = file_length(get_fm(fd)->fdp);
	return length;
}

void check_buffer(const void *buffer, unsigned size, bool write) {
	for (int i = 0; i < size; i+=PGSIZE)
	{
		if(is_kernel_vaddr(buffer+i)) {
			exitt(-1);
		}
		struct page* p = spt_find_page(&thread_current()->spt, buffer+i);
		if (p==NULL) {
			return;
		}
		if (write && !p->writable) {
			exitt(-1);
		}
	}	
}

int readd(int fd, void *buffer, unsigned size) {
	check_buffer(buffer, size, true);
	if ( buffer==NULL || is_not_mapped(buffer)) exitt(-1); // null pointer for buffer / buffer virtual address not mapped
	if ( size==0 ) return 0; // requested to read for size of 0
	if ( fd==1 ) return -1; // trying to read from stdout

	// reading from
	if ( fd==0 ) { // stdin
		if (thread_current()->stdin_allowed) return input_getc();
		else return -1;
	}
	else { // file	
		struct fm* fm = get_fm(fd);
		if ( fm==NULL ) exitt(-1); // fd has not been issued (bad)
		lock_acquire(&lock_file);
		int size_read = file_read(fm->fdp, buffer, size);
		lock_release(&lock_file);
		return size_read;
	}
}

int writee(int fd, const void *buffer, unsigned size) {
	check_buffer(buffer, size, false);
	if ( size==0 ) return 0; // requested to write for size of 0
	if ( fd==0 ) return -1; // trying to write to stdin
	if ( is_not_mapped(buffer) ) exitt(-1); // virtual address for buffer is not mapped
	
	// writing to 
	if ( fd==1 ) { // stdout
		if (thread_current()->stdout_allowed) {
			putbuf(buffer, size);
			return size;
		} else return -1;
	}
	else { // file
		struct fm* fm = get_fm(fd);
		if ( fm==NULL ) return -1; // fd has not been issued (bad)
		lock_acquire(&lock_file);
		int size_wrote = file_write(fm->fdp, buffer, size);
		lock_release(&lock_file);
		return size_wrote;
	}
}

void seekk(int fd, unsigned position) {
	if (get_fm(fd) == NULL) return;
	file_seek(get_fm(fd)->fdp, position);
}

unsigned telll(int fd) {
	if (get_fm(fd) == NULL) return;
	return file_tell(get_fm(fd)->fdp);
}

void closee(int fd) {
	switch (fd) {
		case 0: thread_current()->stdin_allowed = false; return;
		case 1: thread_current()->stdout_allowed = false; return;
	}

	struct fm* main_fm = get_fm(fd);
	if ( get_fm(fd)==NULL ) return; // fd has not been issued (bad)
	
	if (main_fm->file_exists == true) file_close(main_fm->fdp);
	list_remove(&main_fm->elem);
	free(main_fm);
	// palloc_free_page(main_fm);////////////////////////////////
}

void* mmapp(void *addr, size_t length, int writable, int fd, off_t offset) {
	// Fail cases of ap to address 0, not aligned, map length of 0, stdin, stdout
	if (addr==0
		|| is_kernel_vaddr(addr) 
		|| addr+length <= 0 // ???????????????
		|| pg_ofs(addr)!=0 
		|| length==0 
		|| fd==0 
		|| fd==1
	) return MAP_FAILED;
	struct fm* fm = get_fm(fd);
	if (fm==NULL || fm->fdp==NULL || file_length(fm->fdp)==0) return MAP_FAILED;
	return do_mmap(addr, length, writable, fm->fdp, offset);
};
void munmapp(void *addr) {
	do_munmap(addr);
};

#ifdef EFILESYS
bool chdirr(const char *dir) {
	ASSERT(0);
	ASSERT(dir!=NULL); // NULL
	ASSERT(*dir!=NULL); // empty string
	struct inode *inode = NULL;
	struct dir** curr_dir = thread_current()->curr_dir;
	if (!dir_lookup(*curr_dir, dir, &inode)) {
		return false;
	}
	*curr_dir = dir_open(inode);
	return *curr_dir!=NULL;
};

bool mkdirrr(struct dir* parent_dir, const char* name) { // recursive mkdirr
	ASSERT(parent_dir!=NULL);
	// empty string이거나 ".", ".."이면 return false.
	if (parent_dir=="" || parent_dir=="." || parent_dir=="..") {
		return false;
	}
	// name이 '/'로 시작하면 그걸 떼고 dir을 root로 설정해 다시 call
	if (name[0]=='/') {
		return mkdirrr(dir_open_root(), name+1);
	}
	char* path[PATH_MAX]; // 가공 가능한 name 복제품
	strlcpy(path, name, strlen(parent_dir));
	// name이 '/'로 끝나면 그걸 떼고 똑같은 dir로 다시 call
	if (path[strlen(path)-1]=='/') {
		path[strlen(path)-1] = '\0';
		return mkdirrr(parent_dir, path);
	}
	// 여기까지 온 name은 시작과 끝에 '/'를 가질 수 없음

	char* ptr_slash = strrchr(path, '/'); // path 뒤에서부터 '/'의 인덱스를 찾음
	
	// path의 중간에 '/'가 있는 경우. 없으면 지금 dir에 그대로 add하면 됨
	if (ptr_slash!=NULL) {
		// 마지막 '/'를 기준으로 path와 new_dir로 잘라서, 
		*ptr_slash = '\0';
		char* new_dir = ptr_slash + 1;
		ASSERT(path!="");
		// 지금 dir에서 타고 들어간 path를 찾아 들어가고
		struct inode* inode = NULL;
		if (!dir_lookup(thread_current()->curr_dir, path, &inode) || inode==NULL) {
			return false;
		}
		// 거기서 new_dir을 만들도록 다시 call
		mkdirrr(dir_reopen(inode), new_dir);
	}

	// 지금 dir에 path(name) 그대로 add
	disk_sector_t child_sector = cluster_to_sector(fat_create_chain(0));
	if (!dir_create(child_sector, 2)) { // 일단 .과 ..이 들어갈 entry 두 개만 할당
		return false;
	}
	struct dir* child_dir = dir_open(inode_open(child_sector));
	
	return child_dir!=NULL &&
		dir_add(parent_dir, name, child_sector) &&
		dir_add(child_dir, '.', child_sector) &&
		dir_add(child_dir, '..', parent_dir->inode->sector);
}

bool mkdirr(const char *path) {
	return mkdirrr(thread_current()->curr_dir, path);
};
bool readdirr(int fd, char name[READDIR_MAX_LEN + 1]) {
	return false;
};
bool isdirr(int fd) {
	return get_fm(fd)->is_dir;
};
int inumberr(int fd) {
	struct fm* fm = get_fm(fd);
	if (fm->is_dir) {
		return ((struct dir*)(fm->fdp))->inode->sector;
	} else {
		return ((struct file*)(fm->fdp))->inode->sector;
	}
};
#endif



//////////////////////////////////////
//
// stdio.h에 EFILESYS define되어있음! 최종 제출 전 지우기!!!!!!!!!!!!!!!!!
//
//////////////////////////////////////

