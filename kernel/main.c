#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "memlayout.h"
#include "proc.h"
#include "x86.h"

static void mpmain(void) __attribute((noreturn));
extern pde_t entrypgdir[];  // For entry.S
extern int end[];

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
    tvinit();       // trap vectors

    cprintf("This is the BSP.\n");

    //    kinit2(P2V(4 * 1024 * 1024), P2V(PHYSTOP)); // init after SMP init

    mpmain();
}

// Common CPU setup code.
static void
mpmain(void)
{
    cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
    idtinit();       // load idt register
    xchg(&(mycpu()->started), 1); // tell startothers() we're up

    cprintf("cpu%d: ready\n", cpuid(), cpuid());

    cprintf("invoke a interrupt by divide zero\n");
    cprintf("%d", 1/0);

    while (1) {
        asm volatile("hlt");
    }

    // todo: need proc support
//    scheduler();     // start running processes
}
__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
        // Map VA's [0, 4MB) to PA's [0, 4MB)
        [0] = (0) | PTE_P | PTE_W | PTE_PS,
        // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
        [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};