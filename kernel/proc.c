#include "param.h"
#include "proc.h"
#include "x86.h"
#include "defs.h"
#include "spinlock.h"

//struct {
//    struct spinlock lock;
//    struct proc proc[NPROC];
//} ptable;
//
//static struct proc *initproc;
//
//int nextpid = 1;
//extern void forkret(void);
//extern void trapret(void);
//
//static void wakeup1(void *chan);
//
//void
//pinit(void)
//{
//    initlock
//}

// Must be called with interrupts disabled
int
cpuid() {
    return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
    int apicid, i;

    if(readeflags() & FL_IF)
        panic("mycpu: mycpu called with interrupts enabled\n");

    apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid)
            return &cpus[i];
    }
    panic("mycpu: unknown apicid\n");
}