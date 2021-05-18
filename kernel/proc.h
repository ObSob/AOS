#ifndef AOS_PROC_H
#define AOS_PROC_H

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
    SLEEPING,
    RUNNABLE,
    RUNNING,
    ZOMBIE,
};

struct proc {
    uint    sz;                 // size of process memory (bytes)
    pde_t * pgdir;              // page table
    char *  kstack;             // bottom of kernel stack for this process
    enum procstate state;       // process state
    int pid;                    // process ID
    struct proc *parent;        // parent process
    struct trapframe *tf;       // trap frame for current syscall
    struct context *context;    // swtch() here to run process
    void *chan;                 // if non-zero, sleeping on chan
    int killed;                 // if non-zero, have been killed
    struct file *ofile;         // open files
    struct inode *cwd;          // current directory
    char name[16];              // process name (debugging)
};

// process memory is laid out contiguously, low addresses first:
//      text
//      original data and bss
//      fixed-size stack
//      expandable heap

#endif //AOS_PROC_H
