#include "spinlock.h"
#include "buf.h"
#include "x86.h"
#include "defs.h"
#include "traps.h"
#include "proc.h"
#include "fs.h"

#define SECTOR_SIZE 512
#define IDE_BUSY    0x80
#define IDE_DRDY    0x40
#define IDE_DF      0x20
#define IDE_ERR     0x01

#define IDE_CMD_READ    0x20
#define IDE_CMD_WRITE   0x30
#define IDE_CMD_RDMUL   0xC4
#define IDE_CMD_WRMUL   0xC5

// idequeue point to the buf from now being read/write to the disk
// idequeue->qnext points to the next buf to be processed.
// you must hold idelock while manipulating queue

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf *);
static struct buf b;

void
idetest()
{
    b.dev = 1;
    initsleeplock(&(b.lock), "ide test");
    acquire(&idelock);
    acquiresleep(&(b.lock));
    iderw(&b);
    releasesleep(&(b.lock));
    release(&idelock);
}

// wait for IDE disk to become ready
static int
idewait(int checkerr)
{
    int r;

    while (((r = inb(0x1F7)) & (IDE_BUSY | IDE_DRDY)) != IDE_DRDY)
        ;
    if (checkerr && (r & (IDE_DF | IDE_ERR)) != 0)
        return -1;
    return 0;
}

void
ideinit(void)
{
    int i;

    initlock(&idelock, "ide");
    ioapicenable(IRQ_IDE, ncpu - 1);
    idewait(0);

    // check if disk:1 present
    outb(0x1F6, 0xE0 | (1 << 4));
    for (i = 0; i < 1000; i++) {
        if (inb(0x1F7) != 0) {
            havedisk1 = 1;
            break;
        }
    }

    // switch back to disk:0
    outb(0x1F6, 0xE0 | (0 << 4));

    cprintf("disk:1 %sexistence\n", havedisk1 ? "" : "non-");
}

// start the request for b. caller must hold idelock
static void
idestart(struct buf *b)
{
    if (b == 0)
        panic("idestart: buf should provide\n");
    if (b->blockno >= FSSIZE)
        panic("idestart:incorrect blockno");
    int sector_per_block = BSIZE/SECTOR_SIZE;
    int sector = b->blockno * sector_per_block;
    int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ : IDE_CMD_RDMUL;
    int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

    if (sector_per_block > 7)
        panic("indestart:");

    idewait(0);
    outb(0x3F6, 0); // generate interrupt
    outb(0x1F2, sector_per_block);  // number of sectors
    outb(0x1F3, sector & 0xFF);
    outb(0x1F4, (sector >> 8) & 0xFF);
    outb(0x1F5, (sector >> 16) & 0xFF);
    outb(0x1F6, 0xE0 | ((b->dev & 1) << 4) | ((sector >> 24) & 0x0f));
    if(b->flags & B_DIRTY){
        outb(0x1f7, write_cmd);
        outsl(0x1f0, b->data, BSIZE/4);
    } else {
        outb(0x1f7, read_cmd);
    }
}

// interrupt handler
void
ideintr(void)
{
    struct buf *b;

    // first queued buffer is the active request.
    acquire(&idelock);

    if ((b = idequeue) == 0) {
        release(&idelock);
        return ;
    }
    idequeue = b->qnext;

    // read data if need
    if (!(b->flags & B_DIRTY) && idewait(1) >= 0)
        insl(0x1F0, b->data, BSIZE / 4);

    // wake process waiting for this buf.
    b->flags |= B_VALID;
    b->flags &= !B_DIRTY;
    wakeup(b);

    // start disk on next buf in queue
    if (idequeue != 0)
        idestart(idequeue);

    release(&idelock);
}

// sync buf with disk
// if B_DIRTY is set, write buf to disk, clear B_DIRTY, st B_VALID
// else if B_VALID is not set, read buf from dsk, set B_VALID
void
iderw(struct buf *b)
{
    struct buf **pp;

//    if (!holdingsleep(&b->lock))
//        panic("iderw: buf not locked");
    if ((b->flags & (B_VALID | B_DIRTY)) == B_VALID)
        panic("iderw: nothing to do");
    if (b->dev != 0 && !havedisk1)
        panic("iderw: ide disk:1 not present");
    acquire(&idelock);

    // append b to idequeue
    b->qnext = 0;

    for (pp = &idequeue; *pp; pp = &(*pp)->qnext)
        ;
    *pp = b;

    // start disk if necessary
    if (idequeue == b)
        idestart(b);

    // wait for request to finish
    while ((b->flags & (B_VALID | B_DIRTY)) != B_DIRTY) {
        sleep(b, &idelock);
    }

    release(&idelock);
}