#ifndef AOS_MP_H
#define AOS_MP_H

// floating pointer
struct mp {
    uchar signature[4];         // "_MP_"
    void *physaddr;             // phys addr of MP config table
    uchar length;               // 1
    uchar specrev;              // [14]
    uchar chechsum;             // all bytes must add up to 0;
    uchar type;                 // MP system config type
    uchar imcrp;
    uchar reserved[3];
};

// configuration table header
struct mpconf {
    uchar   signature[4];       // "PCMP"
    ushort  length;             // total table length
    uchar   version;            // [14]
    uchar   checksum;           // all bytes must add up to 0
    uchar   product[20];        // product id
    uint    *oemtable;          // OEM table pointer
    ushort  oemlength;          // OEM table length
    ushort  entry;              // entry count
    uint    *lapicaddr;        // address of local APIC
    ushort  xlength;            // extended table length
    uchar   xchecksum;          // extended table checksum
    uchar   reserved;           // reserved
};

// processor table entry
struct mpproc
{
    uchar type;                 // entry type (0)
    uchar apicid;               // local APIC id
    uchar version;              // local APIC version
    uchar flags;                // CPU flags
#define MPBOOT 0x02             // this proc is the bootstrap processor (BSP)
    uchar signature[4];         // CPUS signature
    uint  feature;               // feature flags from CPUID intruction
    uchar reserved[8];
};

// I/O APIC table entry
struct mpioapic {
    uchar type;                 // entry type (2)
    uchar apicno;               // I/O APIC id
    uchar version;              // I/O APIC version
    uchar flags;                // I/O APIC flags
    uchar *addr;                // I/O APIC address
};

// table entry types
#define MPPROC      0x00    // one per processor
#define MPBUS       0x01    // one per bus
#define MPIOAPIC    0x02    // one per I/O APIC
#define MPIOINTR    0x03    // one per bus interrupt source
#define MPLINTR     0x04    // one per system interrupt source

#endif //AOS_MP_H
