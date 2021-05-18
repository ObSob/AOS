#ifndef AOS_SLEEPLOCK_H
#define AOS_SLEEPLOCK_H

// long-term locks for processes
struct sleeplock {
    uint locked;        // is the lock held?
    struct spinlock lk; // spinlock protecting this sleep lock

    // for debugging
    char *name;         // name of lock
    int pid;            // process holding lock
};

#endif //AOS_SLEEPLOCK_H
