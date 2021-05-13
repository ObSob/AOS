#include "param.h"
#include "types.h"
#include "traps.h"
#include "x86.h"
#include "memlayout.h"
#include "defs.h"
#include "data.h"


// Local APIC registers, divided by 4 for use as uint[] indices.
#define ID      (0x0020/4)   // ID
#define VER     (0x0030/4)   // Version
#define TPR     (0x0080/4)   // Task Priority
#define EOI     (0x00B0/4)   // EOI
#define SVR     (0x00F0/4)   // Spurious Interrupt Vector
#define ENABLE     0x00000100   // Unit Enable
#define ESR     (0x0280/4)   // Error Status
#define ICRLO   (0x0300/4)   // Interrupt Command
#define INIT       0x00000500   // INIT/RESET
#define STARTUP    0x00000600   // Startup IPI
#define DELIVS     0x00001000   // Delivery status
#define ASSERT     0x00004000   // Assert interrupt (vs deassert)
#define DEASSERT   0x00000000
#define LEVEL      0x00008000   // Level triggered
#define BCAST      0x00080000   // Send to all APICs, including self.
#define BUSY       0x00001000
#define FIXED      0x00000000
#define ICRHI   (0x0310/4)   // Interrupt Command [63:32]
#define TIMER   (0x0320/4)   // Local Vector Table 0 (TIMER)
#define X1         0x0000000B   // divide counts by 1
#define PERIODIC   0x00020000   // Periodic
#define PCINT   (0x0340/4)   // Performance Counter LVT
#define LINT0   (0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1   (0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR   (0x0370/4)   // Local Vector Table 3 (ERROR)
#define MASKED     0x00010000   // Interrupt masked
#define TICR    (0x0380/4)   // Timer Initial Count
#define TCCR    (0x0390/4)   // Timer Current Count
#define TDCR    (0x03E0/4)   // Timer Divide Configuration

volatile uint *lapic;  // Initialized in mp.c

static void
lapicw(int index, int value)
{
    lapic[index] = value;
    lapic[ID];
}

void
lapicinit(void)
{
    if (!lapic)
        return;

    // enable local APIC; set spurious interrupt vector
    lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

    // the timer repeatedly counts down at bus frequency
    // from lapic[TICR] and then issues an interrupt
    // if xv6 cared more about precise timekeeping
    // TICR would be calibrated using an external time source
    lapicw(TDCR, X1);
    lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
    lapicw(TICR, 10000000);

    // disable logical interrupt lines
    lapicw(LINT0, MASKED);
    lapicw(LINT1, MASKED);

    // disable performance counter overflow interrupts on machines that provide that interrupt entry
    // on machines that provide that interrupt entry
    if (((lapic[VER] >> 16) & 0xFF) >= 4)
        lapicw(PCINT, MASKED);

    // map error interrupt to IRQ_ERROR
    lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

    // clear error status register (requires back-to-back writes)
    lapicw(ESR, 0);
    lapicw(ESR, 0);

    // ack any outstanding interrupts
    lapicw(EOI, 0);

    // send an init level de-assert to synchronise arbitration ID's
    lapicw(ICRHI, 0);
    lapicw(ICRLO, BCAST | INIT | LEVEL);
    while (lapic[ICRLO] & DELIVS)
        ;

    lapicw(TPR, 0);
}

int
lapicid(void)
{
    if (!lapic)
        return 0;
    return lapic[ID] >> 24;
}

// acknowledge interrupt
void
lapiceoi(void)
{
    if (lapic)
        lapicw(EOI, 0);
}

// spin for a given number of microseconds
// on real hardware would want to tune this dynamically
void
microdelay(int us)
{
    // ???
}

#define CMOS_PORT   0x70
#define CMOS_RETURN 0x71

// start additional processor running entry code at addr
// see appendix B of MultiProcessor Specification
void
lapicstartap(uchar apicid, uint addr)
{
    int i;
    ushort *wrv;

    // the BSP must initialize CMOS shutdown code to 0AH
    // and the warm rest vector (DWORD based at 40:67) to point at
    // the AP startup code prior to the [universal startup algorithm]
    outb(CMOS_PORT + 0, 0xF);
    outb(CMOS_PORT + 1, 0x0A);
    wrv = (ushort*)P2V((0x40 << 4 | 0x67)); // warm reset vector
    wrv[0] = 0;
    wrv[1] = addr >> 4;

    // universal start alglrithm
    // send INIT (level-triggered) interrupt to reset other CPU
    lapicw(ICRHI, apicid << 24);
    lapicw(ICRLO, INIT | LEVEL | ASSERT);
    microdelay(200);
    lapicw(ICRLO, INIT | LEVEL);
    microdelay(100);        // should be 10ms, but too slow in bochs (how about qemu?)

    // send startup IPI (twice!) to enter code
    // regular hardware is supposed to only accept a STARTUP
    // when it is in the halted state due to an INIT, So the second
    // should be ignored, but it is part of the official Intel algorithm
    for (i = 0; i < 2; i ++) {
        lapicw(ICRHI, apicid << 24);
        lapicw(ICRLO, STARTUP | (addr >> 12));
        microdelay(200);
    }
}

#define CMOS_STATA  0x0A
#define CMOS_STATB  0x0B
#define CMOS_UIP    (1 << 7)        // RTC update in progress

#define SECS    0x00
#define MINS    0x02
#define HOURS   0x04
#define DAY     0x07
#define MONTH   0x08
#define YEAR    0x09

static uint
cmos_read(uint reg)
{
    outb(CMOS_PORT, reg);
    microdelay(200);

    return inb(CMOS_RETURN);
}

static void
fill_rtcdata(struct rtcdata *r)
{
    r->second = cmos_read(SECS);
    r->minute = cmos_read(MINS);
    r->hour   = cmos_read(HOURS);
    r->day    = cmos_read(DAY);
    r->month  = cmos_read(MONTH);
    r->year   = cmos_read(YEAR);
}

// qemu seems to use 24-hour GWT and the values are BCD encoded
void
cmostime(struct rtcdata * r)
{
    struct rtcdata t1, t2;
    int sb, bcd;

    sb = cmos_read((CMOS_STATA) & CMOS_UIP);
    bcd = (sb & (1 << 2)) == 0;

    // make sure CMOS doesn't modify time while we read it
    for (;;) {
        fill_rtcdata(&t1);
        if (cmos_read(CMOS_STATA) & CMOS_UIP)
            continue;
        fill_rtcdata(&t2);
        if (memcmp(&t1, &t2, sizeof t1) == 0)
            break;
    }

    // convert
    if (bcd) {
#define CONV(x) (t1.x = ((t1.x >> 4) * 10) + (t1.x & 0xF))
        CONV(second);
        CONV(minute);
        CONV(hour);
        CONV(month);
        CONV(year);
#undef CONV
    }

    *r = t1;
    r->year += 2000;
}