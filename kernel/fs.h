#ifndef AOS_FS_H
#define AOS_FS_H

#define ROOTINO 1   // root i-number
#define BSIZE   512 // block size (byte, not bit)

// disk layout:
//  [ boot block | super block | log | inode blocks | free bit map | data blocks]
//
// mkfs compute the super block and builds an initial file system.
// the super block describes the disk layout:
struct superblock {
    uint size;      // size of file system image (blocks)
    uint nblocks;   // number of data blocks
    uint ninodes;   // number of inodes
    uint nlog;      // number of log blocks
    uint logstart;  // block number of first log block
    uint inodestart;// block number of first inode block
    uint bmapstart; // block number of first free map block
};

#define NDIRECT     12
#define NINDIRECT   (BSIZE/sizeof(uint))
#define MAXFILE     (NDIRECT+NINDIRECT)


// on-disk inode structure
struct dinode {
    short type;     // file type
    short major;    // major device number (T_DEV only)
    short minor;    // minor device number (T_DEV only)
    short nlink;    // number of links to inode in file system
    uint size;      // size of file (bytes)
    uint addrs[NINDIRECT + 1];  // data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + (sb).bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

#endif //AOS_FS_H
