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