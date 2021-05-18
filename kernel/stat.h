#ifndef AOS_STAT_H
#define AOS_STAT_H

#define T_DIR   1   // directory
#define T_FILE  2   // file
#define T_DEV   3   // device

struct stat {
    short type;     // type of file
    int dev;        // file system's disk device
    uint ino;       // inode number
    short nlink;    // nuber of links to file
    uint size;      // size of file in bytes
};

#endif //AOS_STAT_H
