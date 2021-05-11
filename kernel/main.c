#include "types.h"
#include "defs.h"
#include "mmu.h"
#include "memlayout.h"
#include "x86.h"

extern pde_t entrypgdir[];  // For entry.S


int main(void)
{
    uartinit();
    cgainit();
    consoleinit();
    cprintf("This is the BSP.\n");

    for (int i = 0; i < 3; i++) {
        cprintf("This is the %d.\n", i);
    }
//    uint low = lowmem();
//    cprintf("low %d\n", low);

    while (1) {
        asm volatile("hlt");
    }
}

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
        // Map VA's [0, 4MB) to PA's [0, 4MB)
        [0] = (0) | PTE_P | PTE_W | PTE_PS,
        // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
        [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};