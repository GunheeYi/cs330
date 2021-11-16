/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

#define BITMAP_ERROR SIZE_MAX

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};
struct bitmap* swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk)*DISK_SECTOR_SIZE/PGSIZE);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;
	anon_page->swapped_out = false;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	size_t swap_idx = anon_page->swap_idx;
	for (int i=0; i<8; i++){
		disk_read(swap_disk, swap_idx*8+i, kva+i*DISK_SECTOR_SIZE);
	}
	bitmap_set(swap_table, swap_idx, 0);
	anon_page->swapped_out = false;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	size_t swap_idx = bitmap_scan(swap_table, 0, 1, 0);
	if (swap_idx == BITMAP_ERROR){
		ASSERT(0);
	}
	bitmap_set(swap_table, swap_idx, 1);
	for (int i=0; i<8; i++){
		disk_write(swap_disk, swap_idx*8 + i, page->frame->kva+i*DISK_SECTOR_SIZE);
	}
	anon_page->swap_idx = swap_idx;
	anon_page->swapped_out = true;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
