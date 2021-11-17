#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	struct file* fp;
	size_t size;
	off_t ofs;
};

bool lazy_load_segment__ (struct page *page, void *aux);

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void write_if_dirty(struct page* p);
void do_munmap (void *va);
#endif
