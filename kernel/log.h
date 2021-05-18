#ifndef AOS_LOG_H
#define AOS_LOG_H

// contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
    int n;
    int block[LOGSIZE];
};

struct log {
    struct spinlock lock;
    int start;
    int size;
    int outstanding;    // how man FS sys calls are executing
    int committing;     // in commit(), please wait
    int dev;
    struct logheader lh;
};

#endif //AOS_LOG_H
