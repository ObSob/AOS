#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "memlayout.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(int bsp) __attribute((noreturn));
extern pde_t entrypgdir[];  // For entry.S
extern int end[];           // first address after kernel loaded from ELF file

int main(void)
{
    kinit1(end,P2V(4 * 1024 * 1024));
    kvmalloc();     // kernel page table
    mpinit();       // detect other processors
    lapicinit();    // interrupt controller
    seginit();      // segment descriptors
    picinit();      // disable legacy pic
    ioapicinit();   // another interrupt controller
    uartinit();     // serial port
    cgainit();      // CGA
    consoleinit();  // console hardware
    pinit();         // process table
    tvinit();       // trap vectors
    binit();         // buffer cache
//    fileinit();      // file table
    ideinit();       // disk
    startothers();   // start other processors
    kinit2(P2V(4 * 1024 * 1024), P2V(PHYSTOP)); // init after SMP init
//    userinit();      // first user



    mpmain(1);
}

static void
mpenter(void)
{
    switchkvm();
    seginit();
    lapicinit();
    mpmain(0);
}

// Common CPU setup code.
static void
mpmain(int bsp)
{
    idtinit();       // load idt register
    xchg(&(mycpu()->started), 1); // tell startothers() we're up
    cprintf("cpu%d: starting as %s\n", cpuid(), bsp ? "BSP" : "AP");

    while (1) {
        asm volatile("hlt");
    }

    // todo: need user space support
//    scheduler();     // start running processes
}

// start the non-boot (AP) processors
static void
startothers(void)
{
    extern char _binary_entryother_start[], _binary_entryother_size[];
    uchar *code;
    struct cpu *c;
    char *stack;

    // write entry code to unused memory at 0x7000
    // the linker has placed the image of entryother.S in _binary_entryother_start
    code = P2V(0x7000);
    memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

    for (c = cpus; c < cpus + ncpu; c++) {
        if (c == mycpu())   // we're stared already
            continue;

        // tell entryother.S what stack to use, where to enter, and what pgdir
        // tp use. We cannot use kpgdir yet, because the AP processors is running in low memory,
        // so we use entrypgdir for the APs too.
        stack = kalloc();
        *(void**)(code - 4) = stack + KSTACKSIZE;
        *(void(**)(void))(code -8) = mpenter;
        *(int**)(code-12) = (void *)V2P(entrypgdir);

        lapicstartap(c->apicid, V2P(code));

        // wait for cpu to finish mpmain()
        while (c->started == 0)
            ;
    }
}

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
        // Map VA's [0, 4MB) to PA's [0, 4MB)
        [0] = (0) | PTE_P | PTE_W | PTE_PS,
        // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
        [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};