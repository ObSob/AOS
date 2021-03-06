#ifndef AOS_DEFS_H
#define AOS_DEFS_H

struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct file;

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

// bio.c
void            binit(void);
struct buf*     bread(uint, uint);
void            brelse(struct buf*);
void            bwrite(struct buf*);

// cga.c
void            cgainit();
void            cgaputc(int c);

// console.c
void            consoleinit(void);
void            cprintf(char *, ...);
void            consoleintr(int(*)(void));
void            panic(char*) __attribute__((noreturn));

// file.c
void            fileinit(void);
struct file*    filealloc(void);
struct file*    fileup(struct file*);
void            fileclose(struct file*);
int             fileread(struct file*, char*, int n);
int             filewrite(struct file*, char*, int n);
int             filestat(struct file*, struct stat*);

// fs.c
void            readsb(int dev, struct superblock *superblock);
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
void            iinit(int dev);
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, char*, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, char*, uint, uint);
int             dirlink(struct inode*, char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);

// ide.c
void            ideinit(void);
void            ideintr(void);
void            iderw(struct buf*);
void            idetest(void);

// ioapic.c
void            ioapicenable(int irq, int cpu);
extern uchar    ioapicid;
void            ioapicinit(void);

// kalloc.c
pde_t*          setupkvm(void);
char*           kalloc(void);
void            kfree(char *);
void            kinit1(void *, void *);
void            kinit2(void *, void *);

// kbd.c
void            kbdintr(void);

// lapic.c
void            cmostime(struct rtcdate *r);
int             lapicid(void);
extern volatile uint*   lapic;
void            lapiceoi(void);
void            lapicinit(void);
void            lapicstartap(uchar, uint);
void            microdelay(int);

// log.c
void            initlog(int dev);
void            log_write(struct buf*);
void            begin_op();
void            end_op();

// mp.c
extern int      ismp;
void            mpinit(void);


// picirq.c
void            picenable(int);
void            picinit(void);

// proc.c
int             cpuid(void);
struct cpu*     mycpu(void);
struct proc*    myproc(void);
void            pinit(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            yield(void);
void            sleep(void*, struct spinlock*);
void            wakeup(void*);
int             kill(int pid);

void            userinit(void);
int             fork(void);
void            setproc(struct proc*);
void            exit(void);
int             growproc(int);
void            procdump(void);


// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);

// swtch.S
void            swtch(struct context**, struct context*);

// spinlock.c
void            initlock(struct spinlock*, char*);
void            getcallerpcs(void*, uint*);
int             holding(struct spinlock*);
void            acquire(struct spinlock*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);

// sleeplock.c
void            initsleeplock(struct sleeplock*, char*);
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);

// trap.c
void            idtinit(void);
extern uint     ticks;
void            tvinit(void);
extern struct spinlock ticklock;

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);

// vm.c
void            seginit(void);
void            kvmalloc(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int             allocuvm(pde_t*, uint, uint);
void            freevm(pde_t*);
void            inituvm(pde_t*, char*, uint);
int             loaduvm(pde_t*, char *, struct inode*, uint, uint);
pde_t*          copyuvm(pde_t*, uint);
void            switchuvm(struct proc*);
void            switchkvm(void);
int             copyout(pde_t*, uint, void*, uint);
void            clearpteu(pde_t *pgdir, char *uva);

#endif //AOS_DEFS_H
