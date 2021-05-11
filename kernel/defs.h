#ifndef AOS_DEFS_H
#define AOS_DEFS_H


// console.c
void            consoleinit(void);
void            cprintf(char *, ...);
void            consoleintr(int(*)(void));
void            panic(char*) __attribute__((noreturn));

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

// vga.c
void            cgainit();
void            vgaputc(int c);

#endif //AOS_DEFS_H
