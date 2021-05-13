#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);

// first address after kernel loaded from ELF file
// defined by the kernel linker script in kernel.ld
extern char end[];

struct run {
    struct run *next;
};

struct {
    struct spinlock lock;
    int use_lock;
    struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just he pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages after installing a full page table that maps them on all cores.
void
kinit1(void* vstart, void *vend)
{
    initlock(&kmem.lock, "kmem");
    kmem.use_lock = 0;
    freerange(vstart, vend);
}


void
kinit2(void *vstart, void *vend)
{
    freerange(vstart, vend);
    kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
    char *p;
    p = (char*)PGROUNDUP((uint)vstart);
    for (; p + PGSIZE <= (char *)vend; p += PGSIZE) {
        kfree(p);
    }
}

// free a whole page
void
kfree(char *v)
{
    struct run *r;

    // v should be the start address f a page
    if ((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
        panic("kfree");

    memset(v, 5, PGSIZE);   // fill with junk

    if(kmem.use_lock)
        acquire(&kmem.lock);

    r = (struct run*)v;
    r->next = kmem.freelist;
    kmem.freelist = r;

    if(kmem.use_lock)
        release(&kmem.lock);
}

// allocate one page of physical memory
char *
kalloc(void)
{
    struct run *r;

    if(kmem.use_lock)
        acquire(&kmem.lock);

    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;

    if (kmem.use_lock)
        release(&kmem.lock);
    return (char*)r;
}