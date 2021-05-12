#include "proc.h"
#include "x86.h"
#include "memlayout.h"
#include "defs.h"

extern char data[]; // provided by kernel.ld
pde_t *kpgdir;      // used for scheduler;


// set up CPU's kernel segment descriptors
// each cpu run once
void
seginit(void)
{
    struct cpu *c;

    // Map "logical" addresses to virtual addresses using identity map.
    // Cannot share a CODE descriptor for both kernel and user
    // because it would have to have DPL_USR, but the CPU forbids
    // an interrupt from CPL=0 to DPL=3.
    c = &cpus[cpuid()];
    c->gdt[SEG_ZERO]  = SEG(0, 0, 0, 0);
    c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_KERN);
    c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, DPL_KERN);
    c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
    c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
    lgdt(c->gdt, sizeof(c->gdt));
}

// return the address of the PTE in page table pgdir that corresponds to virtual address va
// if alloc != 0, create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
    pde_t *pde;
    pte_t *pgtab;

    pde = &pgdir[PDX(va)];
    if(*pde & PTE_P){
        pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
    } else {
        if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
            return 0;
        // Make sure all those PTE_P bits are zero.
        memset(pgtab, 0, PGSIZE);
        // The permissions here are overly generous, but they can
        // be further restricted by the permissions in the page table
        // entries, if necessary.
        *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
    }
    return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not be page-aligned.
static int
mmapages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
    char *a, *last;
    pte_t *pte;

    a = (char *) PGROUNDDOWN((uint)va);
    last = (char *) PGROUNDDOWN(((uint)va) + size - 1);
    for (;;) {
        if ((pte = walkpgdir(pgdir, a, 1)) == 0)
            return -1;
        if (*pte & PTE_P)
            panic("mmapages: remap");
        *pte = pa | perm | PTE_P;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

/*--------------------------------------------------------------------*/

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
    void *virt;         // vm
    uint phys_start;    // pm
    uint phys_end;
    int perm;
} kmap[] = {
        {(void*)KERNBASE, 0, EXTMEM, PTE_W},    // IO space, 0 ~ 1mb
        {(void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kernel test+rodata
        {(void*)data, V2P(data), PHYSTOP, PTE_W},    // kernel data + memory
        {(void*)DEVSPACE, DEVSPACE, 0, PTE_W},   // more device
};

// set up kernel part of a page table
pde_t *
setupkvm(void)
{
    pde_t *pgdir;
    struct kmap *k;

    if ((pgdir = (pde_t*)kalloc()) == 0)
        return 0;
    memset(pgdir, 0, PGSIZE);
    if (P2V(PHYSTOP) > (void*)DEVSPACE)
        panic("PHYSTOP to high");
    for (k = kmap; k < &kmap[NELEM(kmap)]; k++) {
        if (mmapages(pgdir, k->virt,  k->phys_end - k->phys_start, (uint)k->phys_start, k->perm) < 0) {
            freevm(pgdir);
            return 0;
        }
    }
    return pgdir;
}

// alloc one page table for the machine for the kernel address space for scheduler processes
void
kvmalloc(void)
{
    kpgdir = setupkvm();
    cprintf("kernel page table directory at: 0x%p", V2P(kpgdir));
    switchkvm();
}

// switch h/w page table register to kernel-only page table for when no process is running
void
switchkvm(void)
{
    lcr3(V2P(kpgdir));
}

// switch TSS and h/w page table to correspond to process p


// deallocate user pages to bring the process size from oldsz to newsz.
// oldsz and newsz need not e page-aligned, nor does newsz need to be less than oldsz.
// oldsz can be larger than the actual process size. return the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
    pte_t *pte;
    uint a, pa;

    if (newsz >= oldsz)
        return oldsz;

    a = PGROUNDDOWN(newsz);
    for (; a < oldsz; a += PGSIZE) {
        pte = walkpgdir(pgdir, (char *)a, 0);
        if (!pte)
            a = PGADDR((a) + 1, 0, 0) - PGSIZE;
        else if ((*pte & PTE_P) != 0) {
            pa = PTE_ADDR(*pte);
            if (pa == 0)
                panic("dealloctuvm: kfree");
            char *V = P2V(pa);
            kfree(V);
            *pte = 0;
        }
    }
    return newsz;
}

// free a page table and all the physical memory pages in user part.
void
freevm(pde_t *pgdir)
{
    uint i;

    if (pgdir == 0)
        panic("freevm: no pgdir");
    // free pa
    deallocuvm(pgdir, KERNBASE, 0);
    // free pte
    for (i = 0; i < NPDENTRIES; i++) {
        if (pgdir[i] & PTE_P) {
            char *v = P2V(PTE_ADDR(pgdir[i]));
            kfree(v);
        }
    }
    // free pde
    kfree((char*)pgdir);
}