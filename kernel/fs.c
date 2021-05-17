#include "defs.h"
#include "buf.h"
#include "fs.h"

#define min(a, b) ((a) < (b)) ? (a) : (b)


//struct superblock sb;

// Read the super block.
void
readsb(int dev, struct superblock *sb)
{
    struct buf *bp;

    bp = bread(dev, 1);
    memmove(sb, bp->data, sizeof(*sb));
    brelse(bp);
}