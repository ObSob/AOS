#ifndef AOS_SPINLOCK_H
#define AOS_SPINLOCK_H

#include "types.h"

// mutual exclusion lock, reentrant lock
struct spinlock {
    uint locked;        // is the lock held;

    char *name;         // name of lock (for debug)
    struct cpu *cpu;    // the cpu holding the lock
    uint pcs[10];       // the call stack (an array of program counters) that locked the lock;
};

#endif //AOS_SPINLOCK_H
