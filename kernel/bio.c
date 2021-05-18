// Buffer cache
//
// the buffer cache is a linked list of buf structures holding
// cached copies of disk block contents. caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// interface:
// * to get a buffer for a particular disk block, call bread.
// * after changing buffer data, call bwrite to write it to disk
// * when done with the buffer, call brelease
// * do not use the buffer after calling brelease
// * only one process at a time can use a buffer, so do keep them longer than necessary
//
// the implementation uses two state flags internally
// * B_VALID: the buffer data has been read from the disk
// * B_DIRTY: the buffer data has been modified and needs to be written to disk

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct {
    struct spinlock lock;
    struct buf buf[NBUF];

    // linked list of all buffers, through prev/next
    // head.next is mostly recently used.
    struct buf head;
} bcache;

void
binit(void)
{
    struct buf *b;
    int count = 0;

    initlock(&bcache.lock, "bcache");

    // create linked list of buffers
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
        count++;
    }
    cprintf("bcache: %d blocks available in total\n", count);
}

// look through buffer cache for block on device dev
// if not found, allocate a buffer
// in either case, return locked buffer
static struct buf*
bget(uint dev, uint blockno)
{
    struct buf *b;

    acquire(&bcache.lock);

    // is the block already cached?
    for (b = bcache.head.next; b != &bcache.head; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            b->refcnt++;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }

    // not cached; recycle an unused buffer.
    // envn if refcnt=0, B_DIRTY indicates a buffer is in use
    // because log.c has modified it but bot yet committed it.
    for (b = bcache.head.prev; b != &bcache.head; b = b->next) {
        if (b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->flags = 0;
            b->refcnt = 1;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }
    panic("bget: no buffers available");
}

// return a locked buf with the contents of the indicated block
// if not valid, fetch from disk
struct buf*
bread(uint dev, uint blockno)
{
    struct buf *b;

    b = bget(dev, blockno);
    if ((b->flags & B_VALID) == 0) {
        iderw(b);
    }
    return b;
}

// write b's contents to disk, must be locked
void
bwrite(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    b->flags |= B_DIRTY;
    iderw(b);
}

// release a locked buffer
// move to the head of MRU list
void
brelse(struct buf *b)
{
    if (!holdingsleep(&b->lock))
        panic("brelse: must hold bcache sleeplock");

    releasesleep(&b->lock);

    acquire(&bcache.lock);
    b->refcnt--;
    if (b->refcnt == 0) {
        // no one is waiting for it
        // put it to the front of linked list (LRU)
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

    release(&bcache.lock);
}