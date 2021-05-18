#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b)) ? (a) : (b)
static void itrunc(struct inode *);

// there should be one superblock per disk device, but we run with only on device
struct superblock sb;

// Read the super block.
void
readsb(int dev, struct superblock *superblock)
{
    struct buf *bp;

    bp = bread(dev, 1);
    memmove(superblock, bp->data, sizeof(*superblock));
    brelse(bp);
}

// zero a block
static void
bzero(uint dev, int bno)
{
    struct buf *bp;

    bp = bread(dev, bno);
    memset(bp->data, 0, BSIZE);
    log_write(bp);
    brelse(bp);
}

// blocks

// allocate a zeroed disk block
static uint
balloc(uint dev)
{
    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) {
        bp = bread(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            m = 1 << (bi % 8);
            if ((bp->data[bi/8] & m) == 0) {    // is block free?
                bp->data[bi/8] |= m;    // mark block in use
                log_write(bp);
                brelse(bp);
                bzero(dev, b + bi);
                return b + bi;
            }
        }
        brelse(bp);
    }
    panic("balloc: out of blocks");
}

// free a disk block
static void
bfree(int dev, uint b)
{
    struct buf *bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0)
        panic("bfree: freeing free block");
    bp->data[bi/8] &= ~m;
    log_write(bp);
    brelse(bp);
}
/*
// inodes
//
// an inode describes a single unnamed file
// the inode disk structure holds metadata: the file's type.
// its size, the number of links referring to it, and the list
// of blocks holding the file's content
//
// the inodes are laid out sequentially on disk at sb.startinonode. each inode has a number.
// indicating tis position on the disk
//
// the kernel keeps a cache of in-use inodes in memory to provide a place for synchronizing access
// to inodes used by multiple process. the cached inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid
//
// an inode and its in-memory representation go through a sequence of states before they can be used
// by the rest of the file system code
//
// * allocation: an inode is allocated if its type (on disk) is non-zero.
//   ialloc() allocates, and iput() frees if the reference and link counts have fallen to zero
//
// * referencing in cache: an entry in the inode cache is free if ip->ref is zero. otherwise ip->ref
//   the number of in-memory pointers to the entry (open files and current directories). iget() finds
//   or creates a cache entry and increments its ref; iput() decrements ref.
//
// * valid: the information (type, size, &c) in an inode cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from the disk and set ip->valid, while iput() clears ip->valid if p->ref
//   fallen to zero.
//
// * locked: file system coe may only examine and modify the information in an inode and tis content if
//   it has first locked the inode
//
// thus a typical sequence is:
//      ip = iget(dev, inum)
//      ilock(ip)
//      ... examine and modify ip->xxx
//      iunlock(ip)
//      iput(ip)
//
// ilock() is separate from iget() so that system calls can get a long-term reference to an inode (as for
// an open file) and only lock it for short periods (e.g., in read()). the separation also helps avoid
// deadlock and races during pathname lookup. iget() increment ip->ref so that the inode stays cached and
// pointers to it remain valid
//
// many internal file system functions expect the caller to have locked the inodes involved; this lets caller
// create multi-step atomic operations.
//
// the icache.lock spin-lock protects the alloction of icache entries. since ip->ref indicates whether an entry
// is free, and ip->dev and ip->num indicate which i-node an entry holds, one must hold icahce.lock while using
// any of those fields
//
// an i[->lock sleep-lock protects all ip-> fields other than ref, dev, and inum. one must hold ip->lock in order
// to read or write that inode's ip->valid, ip->size, ip->type, &c.
*/

struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} icache;

void
iinit(int dev)
{
    int i;

    initlock(&icache.lock, "icache");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&icache.inode[i].lock, "inode");
    }

    readsb(dev, &sb);
    cprintf("sb: size %d nblocks %d ninodes %d nlog %d logstart %d inodestrt %d bmap start %d\n",
            sb.size, sb.nblocks, sb.ninodes, sb.nlog, sb.logstart, sb.inodestart, sb.bmapstart);
}

static struct inode* iget(uint dev, uint num);

// allocate an inode on device dev.
// mark it as allocated by giving it type type.
// return an unlocked but allocated and referenced inode.
struct inode*
ialloc(uint dev, short type)
{
    int inum;
    struct buf *bp;
    struct dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = bread(dev, IBLOCK(inum, sb));
        dip = (struct dinode*)bp->data + inum % IPB;
        if (dip->type == 0) {   // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_write(bp);  // mark it allocated on disk
            return iget(dev, inum);
        }
        brelse(bp);
    }
    panic("ialloc: no inodes");
}

// copy a modified in-memory inode to disk
// must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// caller must hold ip->lock.
void
iupdate(struct inode *ip)
{
    struct buf *bp;
    struct dinode *dip;

    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode*)bp->data + ip->inum%IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    log_write(bp);
    brelse(bp);
}

// find the inode with number inum on device dev and return the in-memory copy.
// does not lock the inode and does not read it from disk.
static struct inode*
iget(uint dev, uint inum)
{
    struct inode *ip, *empty;

    acquire(&icache.lock);

    // is the lock already cached?
    empty = 0;
    for (ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            release(&icache.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0) // remember empty slot
            empty = ip;
    }

    // recycle an inode cache entry
    if (empty == 0)
        panic("iget: no inodes");

    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&icache.lock);

    return ip;
}

// increment reference count for ip
// return ip to enable ip = idup(ip1) idiom.
struct inode*
idup(struct inode *ip)
{
    acquire(&icache.lock);
    ip->ref++;
    release(&icache.lock);
    return ip;
}

// lock the given inode
// reads the inode from disk if necessary
void
ilock(struct inode *ip)
{
    struct buf *bp;
    struct dinode *dip;

    if (ip == 0 || ip->ref < 1)
        panic("ilock: ref < 1");

    acquiresleep(&ip->lock);

    if (ip->valid == 0) {
        bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct dinode*)bp->data + ip->inum*IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        brelse(bp);
        ip->valid = 1;
        if (ip->type == 0)
            panic("ilock: no type");
    }
}

// unlock the given inode
void
iunlock(struct inode *ip)
{
    if (ip == 0 || !(holdingsleep(&ip->lock)) || ip->ref < 1)
        panic("iunlock");

    releasesleep(&ip->lock);
}

// drop a reference to an in-memory inode
// if that was the last reference, the inode cache entry can be recycles.
// if that was the last reference and the inode has no links to it, free the inode
// (and its content) on disk.
// all calls to ipu() ust be inside a transaction in case it has to free the node
void
iput(struct inode *ip)
{
    acquiresleep(&ip->lock);
    if (ip->valid && ip->nlink == 0) {
        acquire(&icache.lock);
        int r = ip->ref;
        release(&icache.lock);
        if (r == 1) {
            // inode has no links and no other references: truncate and free.
            itrunc(ip);
            ip->type = 0;
            iupdate(ip);
            ip->valid = 0;
        }
    }
    releasesleep(&ip->lock);
}

// common idiom: unlock, the put
void
iunlockput(struct inode *ip)
{
    iunlock(ip);
    iput(ip);
}

// inode content
//
// the conte (data) associated with each inode is stored in block on the disk.
// the first NDIRECT block numbers are listed in ip->addr[].
// the next NINDIECT blocks are listed in block ip->addr[NDIRECT].

// return the disk block address of the nth block in inode ip.
// if there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
    uint addr, *a;
    struct buf *bp;

    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0)
            ip->addrs[bn] = addr = balloc(ip->dev);
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT) {
        // load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0)
            ip->addrs[NDIRECT] = addr = balloc(ip->dev);
        bp = bread(ip->dev, addr);
        a = (uint*)bp->data;
        if ((addr = a[bn]) == 0) {
            a[bn] = addr = balloc(ip->dev);
            log_write(bp);
        }
        brelse(bp);
        return addr;
    }

    panic("bmap: out of range");
}

// truncate inode (discard contents)
// only called when the inode has no links to it (no directory entries referring to it)
// and ahs no in-memory reference to it (is not an open file or current directory)
static void
itrunc(struct inode *ip)
{
    int i, j;
    struct buf *bp;
    uint *a;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (uint*)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}

// copy stat information from inode.
// caller must hold ip->lock
//void
//stati(struct inode *ip, struct stat *st)
//{
//    st->dev = ip->dev;
//    st->ino = ip->inum;
//    st->type = ip->type;
//    st->nlink = ip->nlink;
//    st->size = ip->size;
//}

// read data from inode
// caller mst hold ip->lock
int
readi(struct inode *ip, char *dst, uint off, uint n)
{
    uint tot, m;
    struct buf *bp;

    if (ip->type == T_DEV) {
        if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].read)
            return -1;
        return devsw[ip->major].read(ip, dst, n);
    }

    if (off > ip->size || off + n < off)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        bp = bread(ip->dev, bmap(ip, off/BSIZE));
        m = min(n - tot, BSIZE - off%BSIZE);
        memmove(dst, bp->data + off%BSIZE, m);
        brelse(bp);
    }
    return n;
}

// write data to inode
// caller must hold ip->lock
int
writei(struct inode *ip, char *src, uint off, uint n)
{
    uint tot, m;
    struct buf *bp;

    if (ip->type == T_DEV) {
        if (ip->major < 0 || ip->major >= NDEV || !devsw[ip->major].write)
            return -1;
        return devsw[ip->major].write(ip, src, n);
    }

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        bp = bread(ip->dev, bmap(ip, off/BSIZE));
        m = min(n - tot, BSIZE - off%BSIZE);
        memmove(bp->data + off%BSIZE, src, m);
        log_write(bp);
        brelse(bp);
    }

    if (n > 0 && off > ip->size) {
        ip->size = off;
        iupdate(ip);
    }
    return n;
}

// copy stat information from inode
// caller must hold ip->lock
void
stati(struct inode *ip, struct stat *st)
{
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->nlink;
    st->size = ip->size;
}

// directories

int
namecmp(const char *s, const char *t)
{
    return strncmp(s, t, DIRSIZ);
}

// look for a directory entry in a directory
// if found, set *poff to byte offset of entry
struct inode*
dirlookup(struct inode *dp, char *name, uint *poff)
{
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup: not dir");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup: read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->dev, inum);
        }
    }
    return 0;
}

// write a new directory entry (name, inum) into the directory dp
int
dirlink(struct inode *dp, char *name, uint inum)
{
    int off;
    struct dirent de;
    struct inode *ip;

    // check that name is not present
    if ((ip = dirlookup(dp, name, 0)) != 0) {
        iput(ip);
        return -1;
    }

    // look for an empty dirent
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink: read");
        if (de.inum == 0)
            break;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
        panic("dirlink");

    return 0;
}

// paths

// copy the next path element from path into name
// return a pointer to the element following the copied one
// the returned path has no leading slashes, so the caller can
// check *path=='\0' to see if the name is the last one.
// if no name to remove, return 0;
//
// examples:
//      skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//      skipelem("///a//bb", name) = "bb", setting name = "a"
//      skipelem("a", name) = "", setting name = "a";
//      skipelem("", name) = skipelem("////", name) = 0
//
static char*
skipelem(char *path, char *name)
{
    char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path == 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ) {
        memmove(name, s, DIRSIZ);
    } else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// lock up and return the inode for a path name
// if parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSZ bytes.
// must be called inside a transaction since it calls iput().
static struct inode*
namex(char *path, int nameiparent, char *name)
{
    struct inode *ip, *next;

    if (*path == '/')
        ip = iget(ROOTDEV, ROOTINO);
    else
        ip = idup(myproc()->cwd);

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if ((ip->type != T_DIR)) {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // stop one level early
            iunlock(ip);
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

struct inode*
namei(char *path)
{
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode*
nameiparent(char *path, char *name)
{
    return namex(path, 1, name);
};