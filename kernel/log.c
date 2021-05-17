// simple logging that allows concurrent FS system calls
//
// a log transaction contains the updates of multiple FS system calls.
// the logging system only commits when there are no FS system calls active.
// thus there is never any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk
//
// a system call should call begin_op()/end_op() to mark its start and end.
// usually begin_op() just increments the count of in-progress FS system calls and returns.
// but if it thinks the log is close to running out, it sleeps until the last outstanding
// end_op() commits.
//
// the log is a physical re-do log contains disk blocks.
// the on-disk log format:
//      header block, containing block #s for block A, B, C, ...
//      block A
//      block B
//      block C
//      ...
// log appends are synchronous.

#include "log.h"
#include "fs.h"
#include "defs.h"
#include "buf.h"


struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev)
{
    if (sizeof(struct logheader) >= BSIZE)
        panic("initlog: too big log header");

    struct superblock sb;
    initlock(&log.lock, "log");
    readsb(dev, &sb);
    log.start = sb.logstart;
    log.size = sb.nlog;
    log.dev = dev;
    recover_from_log();
}

// copy committed blocks from log to their home location
static void
install_trans(void)
{
    int tail;

    for (tail = 0; tail < log.lh.n; tail++) {
        struct buf *lbuf = bread(log.dev, log.start + tail + 1);    // read log block
        struct buf *dbuf = bread(log.dev, log.lh.block[tail]);      // read dst
        memmove(dbuf->data, lbuf->data, BSIZE);
        bwrite(dbuf);
        brelse(lbuf);
        brelse(dbuf);
    }
}

// read the log header from disk into the in-memory log header
static void
read_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *lh = (struct logheader *) (buf->data);
    int i;

    log.lh.n = lh->n;
    for (i = 0; i < log.lh.n; i++) {
        log.lh.block[i] = lh->block[i];
    }
    brelse(buf);
}

// write in-memory log header to disk.
// this is the true point at which the current transaction commits.
static void
write_head(void)
{
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *hb = (struct logheader *) (buf->data);
    int i;
    hb->n = log.lh.n;
    for (i = 0; i < log.lh.n; i++) {
        hb->block[i] = log.lh.block[i];
    }
    bwrite(buf);
    brelse(buf);
}

static void
recover_from_log(void)
{
    read_head();
    install_trans();    // if committed, copy from log to disk
    log.lh.n = 0;
    write_head();   // clear the log
}

// called at the start of each FS system call;
void
begin_op()
{
    acquire(&log.lock);
    while (1) {
        if (log.committing) {
            sleep(&log, &log.lock);
        } else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGSIZE) {
            // this op might exhaust log space; wait for commit;
            sleep(&log, &log.lock);
        } else {
            log.outstanding += 1;
            release(&log.lock);
            break;
        }
    }
}

// called at the end of each FS system call
// commits if this was the last outstanding operation.
void
end_op(void)
{
    int do_commit = 0;

    acquire(&log.lock);
    log.outstanding -= 1;
    if (log.committing)
        panic("end_op: log is committing");
    if (log.outstanding == 0) {
        do_commit = 1;
        log.committing = 1;
    } else {
        // begin_op() may be waiting for log space,
        // and decrementing log.outstanding has decreased
        // the amount of reserved space.
        wakeup(&log);
    }
    release(&log.lock);

    if (do_commit) {
        // call commit w/o holding locks, since not allowed to sleep with locks
        commit();
        acquire(&log.lock);
        log.committing = 0;
        wakeup(&log);
        release(&log.lock);
    }
}

// copy modified blocks from cache to log
static void
write_log(void)
{
    int tail;

    for (tail = 0; tail < log.lh.n; tail++) {
        struct buf *to = bread(log.dev, log.start + tail + 1);  // log block
        struct buf *from = bread(log.dev, log.lh.block[tail]);  // cache block
        memmove(to->data, from->data, BSIZE);
        bwrite(to);     // write the log
        brelse(from);
        brelse(to);
    }
}

static void
commit()
{
    if (log.lh.n > 0) {
        write_log();    // write modified blocks from cache to log
        write_head();   // write header to disk -- the read commit
        install_trans();    // now install writes to home location
        log.lh.n = 0;
        write_head();   // erase the transaction from log
    }
}

// caller has modified b->data and is done with the buffer.
// record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//      bp = bread(...)
//      modify bp->data[]
//      log_write(bp)
//      brelse(bp)
void
log_write(struct buf *b)
{
    int i;

    if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
        panic("log_write: too big transaction");
    if (log.outstanding < 1)
        panic("log_write: outside of trans, please wrap in begin_op()/end_op()");

    acquire(&log.lock);
    for (i = 0; i < log.lh.n; i++) {
        if (log.lh.block[i] == b->blockno)  // log absorb
            break;
    }
    log.lh.block[i] = b->blockno;
    if (i == log.lh.n)
        log.lh.n ++;
    b->flags |= B_DIRTY;    // prevent eviction
    release(&log.lock);
}