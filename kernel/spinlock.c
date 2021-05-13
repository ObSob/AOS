#include "defs.h"
#include "spinlock.h"
#include "x86.h"
#include "proc.h"
#include "memlayout.h"

void
initlock(struct spinlock *lk, char *name)
{
    lk->name = name;
    lk->locked = 0;
    lk->cpu = 0;
}

// acquire the lock
// loop (spin) until the lock is acquired
// holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void
acquire(struct spinlock *lk)
{
    pushcli();  // disable interrupt to avoid deadlock
    if (holding(lk))
        panic("acquire");

    // the xchg instruct is atomic
    while (xchg(&lk->locked, 1) != 0)
        ;

    // tell the C compiler and the processor not to move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen after the locks is acquired
    __sync_synchronize();

    // record info about lock acquisition for debugging
    lk->cpu = mycpu();
    getcallerpcs(&lk, lk->pcs);
}

void
release(struct spinlock *lk)
{
    if (!holding(lk))
        panic("release: didn't hold the lock\n");

    lk->pcs[0] = 0;
    lk->cpu = 0;

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that all the stores in the critical
    // section are visible to other cores before the lock is released.
    // Both the C compiler and the hardware may re-order loads and
    // stores; __sync_synchronize() tells them both not to.
    __sync_synchronize();

    // release the lock, equivalent lock->locked = 0
    // this code can't use a C assignment, since it might not be atomic
    // a real OS would use C atomics here
    asm volatile("movl $0, %0" : "+m" (lk->locked) : );

    popcli();
}

// record the current call stack in pcs[] by following the %ebp chain
// still don't know how does it operate
// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
    uint *ebp;
    int i;

    ebp = (uint*)v - 2;
    for(i = 0; i < 10; i++){
        if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
            break;
        pcs[i] = ebp[1];     // saved %eip
        ebp = (uint*)ebp[0]; // saved %ebp
    }
    for(; i < 10; i++)
        pcs[i] = 0;
}

// check whether this cpu is holding the lock
int
holding(struct spinlock *lock)
{
    int r;

    pushcli();
    r = lock->locked && lock->cpu == mycpu();
    popcli();
    return r;
}

// pushcli/popcli are lick cli/sti except that they are matched:
// it takes two popcli to undo two pushchi. also, if interrupts
// are off, then pushcli, popcli leaves them off
void
pushcli(void)
{
    int eflags;

    eflags = readeflags();
    cli();
    if (mycpu()->ncli == 0)
        mycpu()->intena = eflags & FL_IF;
    mycpu()->ncli += 1;
}

void
popcli(void)
{
    if (readeflags() & FL_IF)
        panic("popcli: interrupt is enabled\n");
    if (--mycpu()->ncli < 0)
        panic("popcli: pushcli operation not matched");
    if (mycpu()->ncli == 0 && mycpu()->intena)
        sti();
}