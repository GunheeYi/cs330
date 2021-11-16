#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "bitmap.h"

struct page;
enum vm_type;

struct anon_page {
    size_t swap_idx;
    bool swapped_out;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);
// struct bitmap* swap_table;

#endif
