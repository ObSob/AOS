#include "x86.h"
#include "defs.h"

#define IO_PIC1     0x20    // master (IRQS 0-7)
#define IO_PIC2     0xA0    // slave (IRQs 8-15)

// don't use the 8259A interrupt controllers, xv6 assumes SMP hardware
void
picinit(void)
{
    // mask all interrupts
    outb(IO_PIC1, 0xFF);
    outb(IO_PIC2, 0xFF);
}