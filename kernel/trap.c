#include "types.h"
#include "mmu.h"
#include "traps.h"
#include "x86.h"
#include "defs.h"
#include "spinlock.h"

//interrupt descriptor table (shared by all CPUs)
struct gatedesc idt[256];
extern uint vectors[];      // in vector.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
    int i;

    for (i = 0; i < 25; i++)
        SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
    SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

    initlock(&tickslock, "time");
}

void
idtinit(void)
{
    lidt(idt, sizeof(idt));
}

void
trap(struct trapframe *tf)
{
    if (tf->trapno == T_SYSCALL) {
        cprintf("trap: this is a syscall: %d\n, this will be implement before user space support\n", tf->trapno);
    }
    switch (tf->trapno) {
        case T_IRQ0 + IRQ_TIMER:
            if(cpuid() == 0){
                acquire(&tickslock);
                ticks++;
                wakeup(&ticks);
                release(&tickslock);
            }
            cprintf("trap: this is a tick interrupt\n");
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_IDE:
            ideintr();
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_IDE + 1:
            // Bochs generates spurious IDE1 interrupts.
            break;
        case T_IRQ0 + IRQ_KBD:
            kbdintr();
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_COM1:
            cprintf("trap: this is a uart interrupt\n");
            uartintr();
            lapiceoi();
            break;
        case T_IRQ0 + IRQ_SPURIOUS:
            cprintf("cpu%d: spurious interrupt at %x:%x\n",
                    cpuid(), tf->cs, tf->eip);
            lapiceoi();
            break;
        default:
            // todo: unknown interrupt, print error information here
            cprintf("trap: unknown interrupt number: 0x%x\n", tf->trapno);
    }

    // todo: deal with proc relevant operation
}