#ifndef AOS_FILE_H
#define AOS_FILE_H

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    int ref;
    char readable;
    char writeable;
    struct pipe *pipe;
    struct inode *ip;
    uint off;
};

// in-memory copy of an inode
struct inode {
    uint dev;           // device number
    uint inum;          // inode number
    int ref;            // reference count
    struct sleeplock lock;  // protect everything below here
    int valid;          // inode has been from disk?

    short type;         // copy of disk inode
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

// table mapping major device number to device function
struct devsw {
    int (*read)(struct inode*, char*, int);
    int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#endif //AOS_FILE_H
