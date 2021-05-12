#ifndef AOS_PROC_H
#define AOS_PROC_H

#include "types.h"
#include "mmu.h"
#include "param.h"


// CPUS state
struct cpu {
    uchar apicid;               // local APIC ID
    struct context *scheduler;  // swtch() here to enter scheduler
    struct taskstate ts;        // used by x86 to find stack for interrupt
    struct segdesc gdt[NSEGS];  // x86 global descriptor table
    volatile uint started;      // has the CPU started?
    int ncli;                   // depth of pushcli nesting
    int intena;                 // ware interrupt enabled before pushcli?
    struct proc *proc;          // the process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
    uint edi;
    uint esi;
    uint ebx;
    uint ebp;
    uint eip;
};

enum procstate {
    UNUSED,
    EMBRTO,
    SLEPPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE,
};



#endif //AOS_PROC_H
