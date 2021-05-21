#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"


struct devsw devsw[NDEV];

struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

void
fileinit(void)
{
    initlock(&ftable.lock, "ftable");
}

// allocate a file structure
struct file*
filealloc(void)
{
    struct file *f;

    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

// increment ref count for file f
struct file*
fileup(struct file *f)
{
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileup: file.ref<1");
    f->ref++;
    release(&ftable.lock);
    return f;
}

// close file f (decrement ref count, close when reaches 0)
void
fileclose(struct file *f)
{
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose: file.ref<1");
    if (--f->ref > 0) {
        release(&ftable.lock);
        return;
    }
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);

    // todo: need pipe
//    if (ff.type == FD_PIPE)
//        pipeclose(ff.pipe, ff.writeable);

    if (ff.type == FD_INODE) {
        begin_op();
        iput(ff.ip);
        end_op();
    }
}

// read from file f
int
fileread(struct file *f, char *addr, int n)
{
    int r;

    if (f->readable == 0)
        return -1;
    // todo: need pipe
//    if (f->type == FD_PIPE)
//        return piperead(f->pipe, addr, n);
    if (f->type == FD_INODE) {
        ilock(f->ip);
        if ((r = readi(f->ip, addr, f->off, n)) > 0)
            f->off += r;
        iunlock(f->ip);
        return r;
    }
    panic("fileread: unknown file type");
}

// write to file f
int
filewrite(struct file *f, char *addr, int n)
{
    int r;

    if (f->writeable == 0)
        return -1;

    // todo: need pipe
//    if (f->type == FD_PIPE)
//        return pipewrite(f->pipe, addr, n);

    if (f->type == FD_INODE) {
        // write a few blocks at a time tp avoid exceeding the maximum log transaction
        // size, including i-node, indirect block, allocation blocks, and 2 blocks of slop
        // for non-aligned writes. this really belongs lower down, since writei() might be
        // writing a device like the console
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * 512;
        int i = 0;
        while (i < n) {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;

            begin_op();
            ilock(f->ip);
            if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
                f->off += r;
            iunlock(f->ip);
            end_op();

            if (f < 0)
                break;
            if (r != n1)
                panic("filewrite: short filewrite");
            i += r;
        }
        return i == n ? n : -1;
    }
    panic("filewrite: unknown file type");
}

// get metadata about file f
int
filestat(struct file *f, struct stat *st)
{
    if (f->type == FD_INODE) {
        ilock(f->ip);
        stati(f->ip, st);
        iunlock(f->ip);
        return 0;
    }
    return -1;
}