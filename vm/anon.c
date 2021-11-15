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

static struct bitmap* swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	swap_table = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	size_t swap_idx = page->swap_idx;

	for (int i=0; i<8; i++){
		disk_read(swap_disk, swap_idx*8+i, kva+i*DISK_SECTOR_SIZE);
	}
	
	bitmap_set(swap_table, swap_idx, 0);
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	ASSERT(0);
	struct anon_page *anon_page = &page->anon;
	// 그냥 page로 이것저것
	// page에 있던애를, swap_disk에 써주고, swap table에 넣어주자?
	size_t swap_idx = bitmap_scan(swap_table, 0, 1, 0);
	
	if (swap_idx == BITMAP_ERROR){
		ASSERT(0);
	}
	//sector size 512 fixed
	for (int i=0; i<8; i++){
		disk_write(swap_disk, swap_idx*8 + i, page->frame->kva+i*DISK_SECTOR_SIZE);
	}

	bitmap_set(swap_table, swap_idx, 1);
	page->swap_idx = swap_idx;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
