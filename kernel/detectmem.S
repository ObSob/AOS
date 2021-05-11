
#define mmap_ent 0x8000
.long mmap_ent
do_e820:
  mov $0x8004, %di      # Set di to 0x8004. Otherwise this code will get stuck in `int 0x15` after some entries are fetched
  xor %ebx, %ebx        # ebx must be 0 to start
  xor %bp, %bp		    # keep an entry count in bp
  mov $0x0534D4150, %edx    # Place "SMAP" into edx
  mov $0xe820, %eax
  movl $0x01, %es:20(%di)   # force a valid ACPI 3.X entry
  mov $24, %ecx             # ask for 24 bytes
  int $0x15
  jc .failed                # carry set on first call means "unsupported function"
  mov $0x0534D4150, %edx    # Some BIOSes apparently trash this register?
  cmpl %edx, %eax           # on success, eax must have been reset to "SMAP"
  jne .failed
  test %ebx, %ebx		    # ebx = 0 implies list is only 1 entry long (worthless)
  je .failed
  jmp .jmpin

.e820lp:
  mov $0xe820, %eax		    # eax, ecx get trashed on every int 0x15 call
  movl $0x01, %es:20(%di)	# force a valid ACPI 3.X entry
  mov $24, %ecx             # ask for 24 bytes
  int $0x15
  jc .e820f		            # carry set means "end of list already reached"
  mov $0x0534D4150, %edx	# repair potentially trashed register

.jmpin:
  jcxz .skipent		    # skip any 0 length entries
  cmp %cl, 20		        # got a 24 byte ACPI 3.X response?
  jbe .notext
  testb $1, %es:20(%di)	# if so: is the "ignore this data" bit clear?
  je .skipent
.notext:
  mov %es:8(%di), %ecx  # get lower uint32_t of memory region length
  or %es:12(%di), %ecx  # "or" it with upper uint32_t to test for zero
  jz .skipent		    # if length uint64_t is 0, skip entry
  inc %bp			    # got a good entry: ++count, move to next storage spot
  add 24, %di
.skipent:
  test %ebx, %ebx		# if ebx resets to 0, list is complete
  jne .e820lp
.e820f:
  mov %bp, mmap_ent     # store the entry count
  clc			# there is "jc" on end of list to this point, so the carry must be cleared
  ret
.failed:
  stc			# "function unsupported" error exit
  ret