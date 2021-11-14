/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "lib/user/syscall.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct aux* aux = (struct aux*) page->uninit.aux;
	file_page->fp = aux->file;
	file_page->size = aux->page_read_bytes;
	file_page->ofs = aux->ofs;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

// FROM PROCESS.c --------------------------------------------------------------------
static bool
lazy_load_segment (struct page *page, void *aux) {

	struct aux* aux_ = (struct aux*) aux;
	struct file* file = aux_->file;
	size_t page_read_bytes = aux_->page_read_bytes;
	ASSERT(aux_->page_read_bytes!=0);
	// size_t page_zero_bytes = aux_->page_zero_bytes;
	off_t ofs = aux_->ofs;
	// printf("File pointer %d, page_read_bytes %d, page_zero_bytes %d, ofs %d ------------------\n", file, page_read_bytes, page_zero_bytes, ofs);
	// printf("File size: %d ---------------------------------------------------------------------\n", file_length(file));

	uint8_t* kva = page->frame->kva;
	if (kva == NULL){
		free(page);
		return false;
	}
	off_t read_result = file_read_at(file, kva, page_read_bytes, ofs);
	// printf("Read %d bytes.\n", read_result);
	// if (read_result != page_read_bytes) {
	// 	free(page);
	// 	return false;
	// }
	memset(kva + page_read_bytes, 0, (page_read_bytes-read_result)); // page_zero_bytes 있어야돼..?
	return true;
}

static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	if (ofs%PGSIZE!=0) return false;
	// ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		// 도는 와중에 page가 이미 allocate 되어있으면? 그게 곧 vm_alloc_page~==false 상황인가?

		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct aux* aux = malloc(sizeof(struct aux));
		aux->file = file_reopen(file); // file_reopen necessary?
		aux->page_read_bytes = page_read_bytes;
		aux->page_zero_bytes = page_zero_bytes;
		aux->ofs = ofs;
		// printf("Allocating page with read bytes %d and zero bytes %d-----------------\n", page_read_bytes, page_zero_bytes);
		if (!vm_alloc_page_with_initializer (VM_FILE, upage, writable, lazy_load_segment, aux)){ // edited
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	return true;
}
// -----------------------------------------------------------------------------------------------

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	uint32_t zero_bytes = pg_ofs(length)==0 ? 0 : PGSIZE-pg_ofs(length);
	if (load_segment(file, offset, addr, length, zero_bytes, writable)) return addr;
	// vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, NULL);

	return MAP_FAILED;
}

void write_if_dirty(struct page* p) {
	if (pml4_is_dirty(thread_current()->pml4, p->va)) {
		// printf("File was dirty---------------------------\n");
		file_write_at(p->file.fp, p->va, p->file.size, p->file.ofs); // if file was written while mapped in memory
	}
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread* t = thread_current();
	while(true) {
		struct page* p = spt_find_page(&t->spt, addr);

		if (
			p==NULL ||
			page_get_type(p)!=VM_FILE
		) break;

		write_if_dirty(p);

		spt_remove_page(&thread_current()->spt, p);

		addr += PGSIZE;
	}
}
