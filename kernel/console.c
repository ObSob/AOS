#include "types.h"
#include "defs.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

static void consputc(int);

static int panicked = 0;

#define BACKSPACE 0x100

static struct {
    struct spinlock lock;
    int locking;
} cons;

void
consoleinit(void)
{
    initlock(&cons.lock, "console");
//
//    devsw[CONSOLE].write = consolewrite;
//    devsw[CONSOLE].read = consoleread;
    cons.locking = 1;

    ioapicenable(IRQ_KBD, 0);
}

static void
printint(int xx, int base, int sign)
{
    static char digits[] = "0123456789abcdef";
    char buf[16];
    int i;
    uint x;

    if (sign && (sign = (xx < 0))) {
        x = -xx;
    }
    else {
        x = xx;
    }

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (i-- >= 0) {
        consputc(buf[i]);
    }
}

void
consputc(int c)
{
    if(panicked){
        cli();
        for(;;)
            ;
    }

    if(c == BACKSPACE){
        uartputc('\b'); uartputc(' '); uartputc('\b');
    } else
        uartputc(c);
    cgaputc(c);
}

#define INPUT_BUF 128
struct {
    char buf[INPUT_BUF];
    uint r;     // read index
    uint w;     // write index
    uint e;     // edit index
} input;

// Control-x
#define C(x) ((x) - '@')

void
consoleintr(int (*getc)(void))
{
    int c, doprocdump = 0;

    //    acquire(&cons.lock);
    while ((c = getc()) >= 0) {
        switch (c) {
            // process listing
            case C('P'):
                // procdump() locks cons.lock indirectly; invoke later
                doprocdump = 1;
                break;
            // kill line
            case C('U'):
                while (input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            case C('H'):
            case C('\x7f'): // backspace
                if (input.e != input.w) {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            default:
                if (c != 0 && input.e - input.r < INPUT_BUF) {
                    c = (c == '\r') ? '\n' : c;
                    input.buf[input.e++ % INPUT_BUF] = c;
                    consputc(c);
                    if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) {
                        input.w = input.e;
//                        wakeup(&input.r);
                    }
                }
                break;
        }
    }
//    release(&cons.lock);
    if(doprocdump) {
//        procdump();  // now call procdump() wo. cons.lock held
    }
}

void
cprintf(char *fmt, ...)
{
    int i, c, locking;
    uint *argp;
    char *s;

    locking = cons.locking;
    if (locking)
        acquire(&cons.lock);

    if (fmt == 0)
        panic("cprinf: null fmt");

    argp = (uint*)(void*)(&fmt + 1);
    for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
        if(c != '%'){
            consputc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if(c == 0)
            break;
        switch(c){
            case 'd':
                printint(*argp++, 10, 1);
                break;
            case 'x':
            case 'p':
                printint(*argp++, 16, 0);
                break;
            case 's':
                if((s = (char*)*argp++) == 0)
                    s = "(null)";
                for(; *s; s++)
                    consputc(*s);
                break;
            case '%':
                consputc('%');
                break;
            default:
                // Print unknown % sequence to draw attention.
                consputc('%');
                consputc(c);
                break;
        }
    }
    if(locking)
        release(&cons.lock);
}

void
panic(char *s)
{
    int i;
    uint pcs[10];

    cli();
    cons.locking = 0;
    cprintf("lapicid %d: panic: ", lapicid());
    cprintf(s);
    cprintf("\n");
    getcallerpcs(&s, pcs);
    for(i=0; i<10; i++)
        cprintf(" %p", pcs[i]);
    panicked = 1; // freeze other CPU
    for(;;)
        ;

}

