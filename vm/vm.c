/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

static struct list frame_table;

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();

#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
      vm_initializer *init, void *aux) {

   	ASSERT (VM_TYPE(type) != VM_UNINIT)

   	struct supplemental_page_table *spt = &thread_current ()->spt;

   	/* Check wheter the upage is already occupied or not. */
   	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		* TODO: and then create "uninit" page struct by calling uninit_new. You
		* TODO: should modify the field after calling the uninit_new. */
      
		struct page* p = malloc(sizeof(struct page));
		ASSERT(p!=NULL);
		if (VM_TYPE(type) == VM_ANON){
			uninit_new(p, upage, init, type, aux, anon_initializer);
		}
		if (VM_TYPE(type) == VM_FILE){
			uninit_new(p, upage, init, type, aux, file_backed_initializer);
		}
		p->writable = writable;

		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, p)){
			goto err;
		}
		return true;
   	}
err:
   	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	struct hash_iterator i;
	if (hash_empty(spt->hash_table)){
		return NULL;
	}
	hash_first (&i, spt->hash_table);
	while (hash_next (&i)) {
		struct page *p = hash_entry (hash_cur(&i), struct page, hash_elem);
		if (p->va==pg_round_down(va)) {
			return p;
		}
	}
	return NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	struct hash_elem *elem = hash_insert(spt->hash_table, &page->hash_elem);
	if (elem == NULL){
		succ = true;
	}
	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	ASSERT(pg_ofs(page->va)==0);
	pml4_clear_page(thread_current()->pml4, page->va);
	hash_delete(spt->hash_table, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	struct list_elem* e = list_pop_front(&frame_table);
	victim = list_entry(e, struct frame, frame_elem);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	if (victim == NULL){
		ASSERT(0);
	}
	if (victim->page == NULL){
		ASSERT(0);
	}
	while (!is_user_vaddr(victim->page->va)){
		victim = vm_get_victim();
	}
	swap_out(victim->page);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER);
	if (frame->kva==NULL) {
		free(frame);
		frame = vm_evict_frame();
		frame->page->frame == NULL;
		pml4_clear_page(thread_current()->pml4, frame->page->va);
	}
	frame->page = NULL;
	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	list_push_back(&frame_table, &frame->frame_elem);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
	void* allocating_page_addr = pg_round_down(addr);
	vm_alloc_page(VM_ANON | VM_STACK, allocating_page_addr, true);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	if (is_kernel_vaddr(addr) && user) {
		return false;
	}

	page = spt_find_page(spt, addr);
	if (page == NULL){
		void *rsp = user ? f->rsp : thread_current()->rsp;
		if((uint64_t)addr > USER_STACK - (1<<20) && USER_STACK > (uint64_t)addr && (uint64_t)addr > (uint64_t)rsp - 32){
			vm_stack_growth (addr);
			page = spt_find_page(spt, addr);
		}
		else{
			// return true;
			exitt(-1); // mmap-unmap
		}
	}
	else if(write && !page->writable){
		exitt(-1); // mmap-ro
	}

	ASSERT(page != NULL);
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable)) {
		return false;
	}
	bool b = true;

	return swap_in (page, frame->kva);
}

uint64_t hash_bytes_hash(const struct hash_elem *e, void *aux) {
	struct page* p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof(p->va));
}

bool hash_bytes_less(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
	struct page* a_ = hash_entry(a, struct page, hash_elem);
	struct page* b_ = hash_entry(b, struct page, hash_elem);
	return a_->va < b_->va;
	// return hash_bytes_hash(a, NULL) < hash_bytes_hash(b, NULL);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	spt->hash_table = malloc(sizeof(struct hash));
	hash_init(spt->hash_table, hash_bytes_hash, hash_bytes_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	struct hash_iterator i;
	hash_first(&i, src->hash_table);
	while (hash_next(&i)) {
		struct page* page_src = hash_entry (hash_cur(&i), struct page, hash_elem);

		if (page_src->operations->type==VM_UNINIT) {
			
			struct aux* aux = malloc(sizeof(struct aux));
			memcpy(aux, page_src->uninit.aux, sizeof(struct aux));
			// aux->file = file_duplicate(&page_src->file);
			if (!vm_alloc_page_with_initializer(page_src->uninit.type, page_src->va, page_src->writable, page_src->uninit.init, aux)) {
				return false;
			}
		} 
		else{
			if(page_get_type(page_src)==VM_ANON){

				if (!vm_alloc_page(page_get_type(page_src), page_src->va, page_src->writable)) {
					return false;
				}
				if (!vm_claim_page(page_src->va)) {
					return false;
				}
				struct page* page_dst = spt_find_page(dst, page_src->va);
				memcpy(page_dst->frame->kva, page_src->frame->kva, PGSIZE);

			} else if(page_get_type(page_src)==VM_FILE){
				struct aux* aux = malloc(sizeof(struct aux));

				struct file_page *file_page = &page_src->file;
				aux->file = file_reopen(file_page->fp);
				aux->page_read_bytes = file_page->size;
				aux->page_zero_bytes = NULL;
				aux->ofs = file_page->ofs;
				vm_alloc_page_with_initializer(VM_FILE, page_src->va, page_src->writable, lazy_load_segment__, aux);

				struct page *page_dst = spt_find_page(dst, page_src->va);
				vm_do_claim_page(page_dst);
				
				page_dst->writable = false;
			} else {
				ASSERT(0); // should not reach
			}
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	struct hash_iterator i;
	if (spt->hash_table==NULL){
		return;
	}
	hash_first(&i, spt->hash_table);
	while (hash_next(&i)) {
		struct page* p = hash_entry(hash_cur(&i), struct page, hash_elem);
		
		if (page_get_type(p)==VM_FILE) {
			write_if_dirty(p);
		}
		destroy(hash_entry(hash_cur(&i), struct page, hash_elem));
	}
	free(spt->hash_table);
}
