#ifndef AOS_BUF_H
#define AOS_BUF_H

#include "sleeplock.h"
#include "fs.h"

struct buf {
    int flags;
    uint dev;
    uint blockno;
    struct sleeplock lock;
    uint refcnt;
    struct buf *prev;   // LRU cache list
    struct buf *next;
    struct buf *qnext;  // disk queue
    uchar data[BSIZE];
};

#define B_VALID 0x2 // buffer has been read from disk
#define B_DIRTY 004 // buffer needs to be written to disk

#endif //AOS_BUF_H
