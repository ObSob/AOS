#include "param.h"
#include "proc.h"
#include "x86.h"
#include "defs.h"
#include "spinlock.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

//static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);  // in trapasm.S

//static void wakeup1(void *chan);

void
pinit(void)
{
    initlock(&ptable.lock, "ptable");
}

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

// disable interrupts so that we are not rescheduled while reading proc from the cpu structure
struct proc*
myproc(void)
{
    struct cpu *c;
    struct proc *p;

    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

// proc's kernel stack
//
//  ------------------------------  kernel bottom
//            trapframe
//  ------------------------------
//   fucntion pointer to trapret
//  ------------------------------
//          proc's context
//       eip points to forkert
//  ------------------------------  kernel top

// look in the process table for an UNUSED proc
// if found, change state to EMBRYO and initialize
// state required to run in the kernel
// otherwise return 0;
static struct proc*
allocproc(void)
{
    struct proc *p;
    char *sp;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED)
            goto found;

    // not found
    release(&ptable.lock);
    return 0;

found:
    p->state = EMBRTO;
    p->pid = nextpid++;

    release(&ptable.lock);

    // allocate kernel stack
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // leave room for trap frame
    sp -= sizeof *p->tf;
    // proc's trapframe is bind to the bottom of proc's kernel stack page
    p->tf = (struct trapframe*)sp;

    // set up new context to start executing at forkret
    // which returns to trapret
    sp -= 4;
    *(uint*)sp = (uint)trapret;

    sp -= sizeof *p->context;
    p->context = (struct context*)sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint)forkret;

    return p;
}

// per-cpu process scheduler
// each cpu call scheduler() after setting it self up
// scheduler never returns, it loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfer control via swtch back to the scheduler
void
scheduler(void)
{
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;) {
        // enable interrupts on this process
        sti();

        // loop over process table looking for process to run.
        acquire(&ptable.lock);
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state != RUNNABLE)
                continue;

            // switch to chosen process. it is the processs's job to
            // release ptable.lock and then to reacquire it defore jumping back to us.
            c->proc = p;
            switchuvm(p);
            p->state = RUNNING;

            swtch(&(c->scheduler), p->context);
            switchkvm();

            // process is done running for now
            // it should have changed its p->state before coming back
            c->proc = 0;
        }
        release(&ptable.lock);
    }
}

// enter scheduler. must hold only ptable.lock and have changed proc->state.
// saves and restores intena because intena is a property of this kernel thread.
// not this CPU. it should be proc->intena and proc->ncli, but that would break
// in the few places where a lock is held but there's no process
void
sched(void)
{
    int intena;
    struct proc *p = myproc();

    if (!holding(&ptable.lock))
        panic("sched: ptable.lock should have held\n");
    if (mycpu()->ncli != 1)
        panic("sched: locks");
    if (p->state == RUNNING)
        panic("sched: proc is running");
    if (readeflags() & FL_IF)
        panic("sched: interrupt is enable");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// give up the cpu for one scheduling round.
void
yield(void)
{
    acquire(&ptable.lock);
    myproc()->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// a fork child's very first scheduling by scheduler()
// will swtch() here. "return" to user space
void
forkret(void)
{
    static int first = 1;
    // still holding ptable.lock from scheduler
    release(&ptable.lock);

    if (first) {
        // some initialization function must be run in the context
        // of a regular process (e.g. they call sleep), and thus connot
        // be run from main();
        first = 0;
        // todo: need file support
//        iinit(ROOTDEV);
//        initlog(ROOTDEV);
    }

    // return to "caller", actually trapret (see allocproc)
}

// atomically release lock and sleep on chan.
// reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    if (p == 0)
        panic("sleep:");
    if (lk == 0)
        panic("sleep: without lk");

    // must acquire ptable.lock in orde rto change p->state
    // and then call sched. once we hold ptable.lock, we can be guaranteed
    // that we won't miss any wakeup (wake up runs with ptable locked),
    // so it's ok to release lk
    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        release(lk);
    }

    // go to sleep
    p->chan = chan;
    p->state = SLEEPING;

    sched();

    // tidy up;
    p->chan = 0;

    // reacquire original lock
    if (lk != &ptable.lock) {
        release(&ptable.lock);
        acquire(lk);
    }
}

// wake up al processes sleeping on chan
// the ptable lock must be held
static void
wakeup1(void *chan)
{
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
}

// wake up all process sleeping on chan
void
wakeup(void *chan)
{
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// kill the process with the given pid
// process won't exit until it returns to user space (see trap in trap.c)
int
kill(int pid)
{
    struct proc *p;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // wake process from sleep if necessary
            // sleep on sleeplock is fine, because it won't run any more,
            // it will be killed in trap.c when scheduled
            if (p->state == SLEEPING) {
                p->state = RUNNABLE;
            }
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}

//