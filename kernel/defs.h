#ifndef AOS_DEFS_H
#define AOS_DEFS_H

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

// cga.c
void            cgainit();
void            cgaputc(int c);

// console.c
void            consoleinit(void);
void            cprintf(char *, ...);
void            consoleintr(int(*)(void));
void            panic(char*) __attribute__((noreturn));

// kalloc.c
pde_t*          setupkvm(void);
char*           kalloc(void);
void            kfree(char *);
void            kinit1(void *, void *);
void            kinit2(void *, void *);

// proc.c
//int             cpuid(void);
//struct cpu*     mycpu(void);

// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);

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
//int             loaduvm(pde_t*, char *, struct inode*, uint, uint);
pde_t*          copyuvm(pde_t*, uint);
//void            switchvm(struct proc*);
void            switchkvm(void);
int             copyout(pde_t*, uint, void*, uint);
void            clearpteu(pde_t *pgdir, char *uva);

#endif //AOS_DEFS_H
