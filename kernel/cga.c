#include "memlayout.h"
#include "x86.h"
#include "defs.h"
#include "console.h"

#define CRTPORT 0x3d4
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory after paging

static const uint CGA_WIDTH = 80;
static const uint CGA_HEIGHT = 25;

uint pos;

void cgainit(void)
{
    pos = 0;
}

void
cgaputc(int c)
{
    // Cursor position: col + 80*row.
    outb(CRTPORT, 14);
    pos = inb(CRTPORT+1) << 8;
    outb(CRTPORT, 15);
    pos |= inb(CRTPORT+1);

    if(c == '\n')
        pos += CGA_WIDTH - pos % CGA_WIDTH;
    else if(c == BACKSPACE){
        if(pos > 0) --pos;
    } else
        crt[pos++] = (c&0xff) | 0x0700;  // black on white

    if(pos < 0 || pos > CGA_HEIGHT * CGA_WIDTH)
        panic("pos under/overflow");

    if((pos/80) >= CGA_HEIGHT - 1){  // Scroll up.
        memmove(crt, crt + CGA_WIDTH, sizeof(crt[0]) * (CGA_HEIGHT - 2) * CGA_WIDTH);
        pos -= CGA_WIDTH;
        memset(crt+pos, 0, sizeof(crt[0])*((CGA_HEIGHT - 1) * CGA_WIDTH - pos));
    }

    outb(CRTPORT, 14);
    outb(CRTPORT+1, pos>>8);
    outb(CRTPORT, 15);
    outb(CRTPORT+1, pos);
    crt[pos] = ' ' | 0x0700;
}