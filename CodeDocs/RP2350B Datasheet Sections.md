# GPIO

Chapter 9. GPIO
 CAUTION
Under certain conditions, pull-down does not function as expected. For more information, see RP2350-E9.
## 9.1. Overview
RP2350 has up to 54 multi-functional General Purpose Input / Output (GPIO) pins, divided into two banks:
Bank 0
30 user GPIOs in the QFN-60 package (RP2350A), or 48 user GPIOs in the QFN-80 package
Bank 1
six QSPI IOs, and the USB DP/DM pins
You can control each GPIO from software running on the processors, or by a number of other functional blocks. To
meet USB rise and fall specifications, the analogue characteristics of the USB pins differ from the GPIO pads. As a
result, we do not include them in the 54 GPIO total. However, you can still use them for UART, I2C, or processorcontrolled GPIO through the single-cycle IO subsystem (SIO).
In a typical use case, the QSPI IOs are used to execute code from an external flash device, leaving 30 or 48 Bank 0
GPIOs for the programmer to use. The QSPI pins might become available for general purpose use when booting the chip
from internal OTP, or controlling the chip externally through SWD in an IO expander application.
All GPIOs support digital input and output. Several Bank 0 GPIOs can also be used as inputs to the chip’s Analogue to
Digital Converter (ADC):
• GPIOs 26 through 29 inclusive (four total) in the QFN-60 package
• GPIOs 40 through 47 (eight total) in the QFN-80 package
Bank 0 supports the following functions:
• Software control via SIO — Section 3.1.3, “GPIO control”
• Programmable IO (PIO) — Chapter 11, PIO
• 2 × SPI — Section 12.3, “SPI”
• 2 × UART — Section 12.1, “UART”
• 2 × I2C (two-wire serial interface) — Section 12.2, “I2C”
• 8 × two-channel PWM in the QFN-60 package, or 12 × in QFN-80 — Section 12.5, “PWM”
• 2 × external clock inputs — Section 8.1.2.4, “External clocks”
• 4 × general purpose clock output — Section 8.1, “Overview”
• 4 × input to ADC in the QFN-60 package, or 8 × in QFN-80 — Section 12.4, “ADC and Temperature Sensor”
• 1 × HSTX high-speed interface — Section 12.11, “HSTX”
• 1 × auxiliary QSPI chip select, for a second XIP device — Section 12.14, “QSPI memory interface (QMI)”
• CoreSight execution trace output — Section 3.5.7, “Trace”
• USB VBUS management — Section 12.7.3.10, “VBUS control”
• External interrupt requests, level or edge-sensitive — Section ## 9.5, “Interrupts”
Bank 1 contains the QSPI and USB DP/DM pins and supports the following functions:
RP2350 Datasheet
## .1. Overview 587
• Software control via SIO — Section 3.1.3, “GPIO control”
• Flash execute in place (Section 4.4, “External flash and PSRAM (XIP)”) via QSPI Memory Interface (QMI) — Section
12.14, “QSPI memory interface (QMI)”
• UART — Section 12.1, “UART”
• I2C (two-wire serial interface) — Section 12.2, “I2C”
The logical structure of an example IO is shown in Figure 41.
Figure 41. Logical
structure of a GPIO.
Each GPIO can be
controlled by one of a
number of peripherals,
or by software control
registers in the SIO.
The function select
(FSEL) selects which
peripheral output is in
control of the GPIO’s
direction and output
level, and which
peripheral input can
see this GPIO’s input
level. These three
signals (output level,
output enable, input
level) can also be
inverted or forced high
or low, using the GPIO
control registers.
## .2. Changes from RP2040
RP2350 GPIO differs from RP2040 in the following ways:
• 18 more GPIOs in the QFN-80 package
• Addition of a third PIO to GPIO functions
• USB DP/DM pins can be used as GPIO
• Addition of isolation register to pad registers (preserves pad state while in a low power state, cleared by software
on power up)
• Changed default reset state of pad controls
• Both Secure and Non-secure access to GPIOs (see Section 10.6)
• Double the number of GPIO interrupts to differentiate between Secure and Non-secure
• Interrupt summary registers added so you can quickly see which GPIOs have pending interrupts
## .3. Reset state
At first power up, Bank 0 IOs (GPIOs 0 through 29 in the QFN-60 package, and GPIOs 0 through 47 in the QFN-80
package) assume the following state:
• Output buffer is high-impedance
• Input buffer is disabled
• Pulled low
• Isolation latches are set to latched (Section ## .7)
The pad output disable bit (GPIO0.OD) for each pad is clear at reset, but the IO muxing is reset to the null function,
RP2350 Datasheet
## .2. Changes from RP2040 588
which ensures that the output buffer is high-impedance.
 IMPORTANT
The pad reset state is different from RP2040, which only disables digital inputs on GPIOs 26 through 29 (as of
version B2) and does not have isolation latches. Applications must enable the pad input (GPIO0.IE = 1) and disable
pad isolation latches (GPIO0.ISO = 0) before using the pads for digital I/O. The gpio_set_function() SDK function
performs these tasks automatically.
Bank 1 IOs have the same reset state as Bank 0 GPIOs, except for the input enable (IE) resetting to 1, and different pullup/pull-down states: SCK, SD0 and SD1 are pull-down, but SD2, SD3 and CSn are pull-up.
 NOTE
To use a Bank 0 GPIO as a second chip select, you need an external pull-up to ensure the second QSPI device does
not power up with its chip select asserted.
The pads return to the reset state on any of the following:
• A brownout reset
• Asserting the RUN pin low
• Setting SW-DP CDBGRSTREQ via SWD
• Setting RP-AP rescue reset via SWD
If a pad’s isolation latches are in the latched state (Section ## .7) then resetting the PADS and IO registers does not
physically return the pad to its reset state. The isolation latches prevent upstream signals from propagating to the pad.
Clear the ISO bit to allow signals to propagate.
# Chapter 4. Memory
RP2350 has embedded ROM, OTP and SRAM. RP2350 provides access to external flash via a QSPI interface.
## 4.1. ROM
A 32 kB read-only memory (ROM) appears at address 0x00000000. The ROM contents are fixed permanently at the time
the silicon is manufactured. Chapter 5 describes the ROM contents in detail, but in summary it contains:
• Core 0 Boot code (Section 5.2)
• Core 1 Launch code (Section 5.3)
• Runtime APIs (Section 5.4).
• USB bootloader
◦ Mass storage interface for drag and drop of UF2 flash and SRAM binaries (Section 5.5)
◦
PICOBOOT interface to support picotool and advanced operations like OTP programming (Section 5.6)
◦
Support for white-labelling all USB exposed information/identifiers (Section 5.7)
• UART bootloader: minimal shell to load an SRAM binary from a host microcontroller (Section 5.8)
The ROM offers single-cycle access, and has a dedicated AHB5 arbiter, so it can be accessed simultaneously with other
memory devices. Writing to the ROM has no effect, and no bus fault is generated on write.
The ROM is covered by IDAU regions enumerated in Section 10.2.2. These aid in partitioning the bootrom between
Secure and Non-secure code: in particular the USB/UART bootloader runs as a Non-secure client application on Arm, to
reduce the attack surface of the secure boot implementation.
Certain ROM features are not implemented on RISC-V, most notably secure boot.
4.2. SRAM
There is a total of 520 kB (520 × 1024 bytes) of on-chip SRAM. For performance reasons, this memory is physically
partitioned into ten banks, but logically it still behaves as a single, flat 520 kB memory. RP2350 does not restrict the
data stored in each bank: you can use any bank to store processor code, data buffers, or a mixture of the two. There are
eight 16,384 × 32-bit banks (64 kB each) and two 1024 × 32-bit banks (4 kB each).
 NOTE
Banking is a physical partitioning of SRAM which improves performance by allowing multiple simultaneous
accesses. Logically, there is a single 520 kB contiguous memory.
Each SRAM bank is accessed via a dedicated AHB5 arbiter. This means different bus managers can access different
SRAM banks in parallel, so up to six 32-bit SRAM accesses can take place every system clock cycle (one per manager).
SRAM is mapped to system addresses starting at 0x20000000. The first 256 kB address region, up to and including
0x2003ffff, is word-striped across the first four 64 kB banks. The next 256 kB address region, up to 0x2007ffff is wordstriped across the remaining four 64 kB banks. The watermark between these two striped regions, at 0x20040000, marks
the boundary between the SRAM0 and SRAM1 power domains.
Consecutive words in the system address space are routed to different RAM banks as shown in Table 434. This scheme
is referred to as sequential interleaving, and improves bus parallelism for typical memory access patterns.
RP2350 Datasheet
## 4.1. ROM 337
Table 434. SRAM
bank0/1/2/3 striped
mapping.
System address SRAM Bank SRAM word address
0x20000000 Bank 0 0
0x20000004 Bank 1 0
0x20000008 Bank 2 0
0x2000000c Bank 3 0
0x20000010 Bank 0 1
0x20000014 Bank 1 1
0x20000018 Bank 2 1
0x2000001c Bank 3 1
0x20000020 Bank 0 2
0x20000024 Bank 1 2
0x20000028 Bank 2 2
0x2000002c Bank 3 2
etc
The top two 4 kB regions (starting at 0x20080000 and 0x20081000) map directly to the smaller 4 kB memory banks.
Software may choose to use these for per-core purposes (e.g. stack and frequently-executed code), guaranteeing that
the processors never stall on these accesses. Like all SRAM on RP2350, these banks have single-cycle access from all
managers, (provided no other managers access the bank in the same cycle) so it is reasonable to treat memory as a
single 520 kB device.
 NOTE
RP2040 had a non-striped SRAM mirror. RP2350 no longer has a non-striped mirror, to avoid mapping the same
SRAM location as both Secure and Non-secure. You can still achieve some explicit bandwidth partitioning by
allocating data across two 256 kB blocks of 4-way-striped SRAM.
## 4.2.1. Other on-chip memory
Besides the 520 kB main memory, there are two other dedicated RAM blocks that may be used in some circumstances:
• Cache lines can be individually pinned within the XIP address space for use as SRAM, up to the total cache size of
16 kB (see Section 4.4.1.3). Unpinned cache lines remain available for transparent caching of XIP accesses.
• If USB is not used, the USB data DPRAM can be used as a 4 kB memory starting at 0x50100000.
There is also 1 kB of dedicated boot RAM, hardwired to Secure access only, whose contents and layout is defined by the
bootrom — see Chapter 5.
RP2350 Datasheet
## 4.2. SRAM 338
 NOTE
Memory in the peripheral address space (addresses starting with 0x4, 0x5 or 0xd) does not support code execution.
This includes USB RAM and boot RAM. These address ranges are made IDAU-Exempt to simplify assigning
peripherals to security domains using ACCESSCTRL, and consequently must be made non-executable to avoid the
possibility of Non-secure-writable, Secure-executable memory.
## 4.3. Boot RAM
Boot RAM is a 1 kB (256 × 32-bit) SRAM dedicated for use by the bootrom. It is slower than main SRAM, as it is
accessed over APB, taking three cycles for a read and four cycles for a write.
Boot RAM is used for myriad purposes during boot, including the initial pre-boot stack. After the bootrom enters the
user application, boot RAM contains state for the user-facing ROM APIs, such as the resident partition table used for
flash programming protection, and a copy of the flash XIP setup function (formerly known as boot2) to quickly reinitialise flash XIP modes following serial programming operations.
Boot RAM is hardwired to permit Secure access only (Arm) or Machine-mode access only (RISC-V). It is physically
impossible to execute code from boot RAM, regardless of MPU configuration, as it is on the APB peripheral bus
segment, which is not wired to the processor instruction fetch ports.
Since boot RAM is in the XIP RAM power domain, it is always powered when the switched core domain is powered. This
simplifies SRAM power management in the bootrom, because it doesn’t have to power up any RAM before it has a place
to store the call stack.
Boot RAM supports the standard atomic set/clear/XOR accesses used by other peripherals on RP2350 (Section 2.1.3).
It is possible to use boot RAM for user-defined purposes, but this is not recommended, as it may cause ROM APIs to
behave unpredictably. Calling into the ROM could modify data stored in boot RAM.
## 4.3.1. List of registers
A small number of registers are located on the same bus endpoint as boot RAM:
Write Once Bits
These are flags which once set, can only be cleared by a system reset. They are used in the implementation of
certain bootrom security features.
Boot Locks
These function the same as the SIO spinlocks (Section 3.1.4), however they are normally reserved for bootrom
purposes (Section 5.4.4).
These registers start from an offset of 0x800 above the boot RAM base address of 0x400e0000 (defined as
BOOTRAM_BASE in the SDK).
Table 435. List of
BOOTRAM registers
Offset Name Info
0x800 WRITE_ONCE0 This registers always ORs writes into its current contents. Once a
bit is set, it can only be cleared by a reset.
0x804 WRITE_ONCE1 This registers always ORs writes into its current contents. Once a
bit is set, it can only be cleared by a reset.
0x808 BOOTLOCK_STAT Bootlock status register. 1=unclaimed, 0=claimed. These locks
function identically to the SIO spinlocks, but are reserved for
bootrom use.
RP2350 Datasheet
4.3. Boot RAM 339
Offset Name Info
0x80c BOOTLOCK0 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x810 BOOTLOCK1 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x814 BOOTLOCK2 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x818 BOOTLOCK3 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x81c BOOTLOCK4 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x820 BOOTLOCK5 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x824 BOOTLOCK6 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
0x828 BOOTLOCK7 Read to claim and check. Write to unclaim. The value returned on
successful claim is 1 << n, and on failed claim is zero.
BOOTRAM: WRITE_ONCE0, WRITE_ONCE1 Registers
Offsets: 0x800, 0x804
Table 436.
WRITE_ONCE0,
WRITE_ONCE1
Registers
Bits Description Type Reset
31:0 This registers always ORs writes into its current contents. Once a bit is set, it
can only be cleared by a reset.
RW 0x00000000
BOOTRAM: BOOTLOCK_STAT Register
Offset: 0x808
Table 437.
BOOTLOCK_STAT
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 Bootlock status register. 1=unclaimed, 0=claimed. These locks function
identically to the SIO spinlocks, but are reserved for bootrom use.
RW 0xff
BOOTRAM: BOOTLOCK0, BOOTLOCK1, …, BOOTLOCK6, BOOTLOCK7
Registers
Offsets: 0x80c, 0x810, …, 0x824, 0x828
Table 438.
BOOTLOCK0,
BOOTLOCK1, …,
BOOTLOCK6,
BOOTLOCK7 Registers
Bits Description Type Reset
31:0 Read to claim and check. Write to unclaim. The value returned on successful
claim is 1 << n, and on failed claim is zero.
RW 0x00000000
## 4.4. External flash and PSRAM (XIP)
RP2350 can access external flash and PSRAM via its execute-in-place (XIP) subsystem. The term execute-in-place
refers to external memory mapped directly into the chip’s internal address space. This enables you to execute code as
is from the external memory without explicitly copying into on-chip SRAM. For example, a processor instruction fetch
from AHB address 0x10001234 results in a QSPI memory interface fetch from address 0x001234 in an external flash device.
A 16 kB on-chip cache retains the values of recent reads and writes. This reduces the chances that XIP bus accesses
must go to external memory, improving the average throughput and latency of the XIP interface. The cache is physically
structured as two 8 kB banks, interleaving odd and even cache lines of 8-byte granularity over the two banks. This
allows processors to access multiple cache lines during the same cycle. Logically, the XIP cache behaves as a single
16 kB cache.
APB: XIP_CTRL
XIP/Cache
Control Registers
Cache Bank 0
8 kB 2-way
Cache Bank 1
8 kB 2-way Streaming FIFO
AHB: XIP
(Even cache lines)
AHB: XIP
(Odd cache lines)
AHB: AUX
(Streaming DMA)
QSPI Memory Interface
AHB Arbiter
APB: QMI_CTRL
Data
SCK CSn[1:0] SD[3:0]
Configuration
Figure 16. Flash
execute-in-place (XIP)
subsystem. The cache
is split into two banks
for performance, but
behaves as a single
16 kB cache. XIP
accesses first query
the cache. If a cache
entry is not found, the
QMI generates an
external serial access,
adds the resulting
data to the cache, and
forwards it on to the
system bus (for reads)
or merges it with the
AHB write data (for
writes).
When booting from flash, the RP2350 bootrom (Chapter 5) sets up a baseline QMI execute-in-place configuration. User
code may later reconfigure this to improve performance for a specific flash device. QSPI clock divisors can be changed
at any time, including whilst executing from XIP. Other reconfiguration requires a momentary disable of the interface.
## 4.4.1. XIP cache
The cache is 16 kB, two-way set-associative, 1 cycle hit. It is internal to the XIP subsystem, and only involved in
accesses to the QSPI memory interface, so software does not have to consider cache coherence unless performing
flash programming operations. It caches accesses to a 26-bit downstream XIP address space. On RP2350, the lower
half of this space is occupied by two 16 MB windows for the two QMI chip selects. RP2350 reserves the remainder for
future expansion, but you can use the space to pin cache lines outside of the QMI address space for use as cache-asSRAM (Section 4.4.1.3). The 26-bit XIP address space is mirrored multiple times in the RP2350 address space, decoded
on bits 27:26 of the system bus address:
• 0x10… : Cached XIP access
• 0x14… : Uncached XIP access
• 0x18… : Cache maintenance writes
• 0x1c… : Uncached, untranslated XIP access — bypass QMI address translation
You can disable cache lookup separately for Secure and Non-secure accesses via the CTRL.EN_SECURE and
CTRL.EN_NONSECURE register bits. The CTRL register contains controls to disable Secure/Non-secure access to the
uncached and uncached/untranslated XIP windows, which avoids duplicate mappings that may otherwise require
additional SAU or PMP regions.
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 341
## 4.4.1.1. Cache maintenance
Cache maintenance is performed on a line-by-line basis by writing into the cache maintenance mirror of the XIP address
space, starting at 0x18000000. Cache lines are 8 bytes in size. Write data is ignored; instead, the 3 LSBs of the address
select the maintenance operation:
• 0x0: Invalidate by set/way
• 0x1: Clean by set/way
• 0x2: Invalidate by address
• 0x3: Clean by address
• 0x7: Pin cache set/way at address (Section 4.4.1.3)
Invalidate
Marks a cache line as no longer containing data; the next access to the same address will miss the cache.
Does not write back any data to external memory. Used when external memory has been modified in a way
that the cache would not automatically know about, such as a flash programming operation.
Clean
Instructs the cache to write out any data stored in the cache as a result of a previous cached write access that
has not yet been written out to external memory. Used to make cached writes available to uncached reads.
Also used when cache contents are about to be lost, but external memory is to stay powered (for example,
when the system is about to power down).
By set/way
Selects a particular cache line to be maintained, out of the 2048 × 8-byte lines that make up the cache. Bit 13 of
the system bus address selects the cache way. Bits 12:3 of the address select a particular cache line within
that way. Mainly used to iterate exhaustively over all cache lines (for example, during a full cache flush).
By address
Looks up an address in the cache, then performs the requested maintenance if that line is currently allocated
in the cache. Used when only a particular range of XIP addresses needs to be maintained, for example, a flash
page that was just programmed. Usually faster than a full flush, because the real cost of a cache flush is not in
the maintenance operations, but the large number of subsequent cache misses.
Pin
Prevents a particular cache line from being evicted. Used to mark important external memory contents that
must get guaranteed cache hits, or to allocate cache lines for use as cache-as-SRAM. If a cached access to
some other address misses the cache and attempts to evict a pinned cache line, the eviction fails, and the
access is downgraded to an uncached access.
Cache maintenance operations operate on the cache’s tag memory. This is the cache’s metadata store, which tracks
the state of each cache line. Maintenance operations do not affect the cache’s data memory, which contains the
cache’s copy of data bytes from external memory.
By default, cache maintenance is Secure-only. Non-secure writes to the cache maintenance address window have no
effect and return a bus error. Non-secure cache maintenance can be enabled by setting the CTRL.MAINT_NONSEC
register bit, but this is not recommended if Secure software may perform cached XIP accesses.
4.4.1.2. Cache line states
The changes to a cache line caused by cached accesses and maintenance operations can be summarised by a set of
state transitions.
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 342
Invalid
Pinned Dirty
Clean
Inv, Evict
Inv, Clean R
R
R, W, Clean, Pin R, W
Clean W
W
Inv Pin
Inv, Evict
Pin
Pin
Figure 17. State
transition diagram for
each cache line. Inv,
Clean and Pin
represent
invalidate/clean/pin
maintenance
operations,
respectively. R and W
represent cached
reads and writes. Evict
represents a cache
line deallocation to
make room for a new
allocation due to a
read/write cache
miss.
Initially, the state of all cache lines is undefined. When booting from flash, the bootrom performs an invalidate by
set/way on every line of the cache to force them to a known state. In the diagram above, all states have an Inv arc to the
invalid state.
A dirty cache line contains data not yet propagated to downstream memory.
A clean cache line contains data that matches the downstream memory contents.
Accessing an invalid cache line causes an allocation: the cache fetches the corresponding data from downstream
memory, stores it in the cache, then marks the cache line as clean or dirty. The cache also stores part of the
downstream address, known as the tag, to recall the downstream address stored in each cache line. Read allocations
enter the clean state, so the cache line can be safely freed at any time. Write allocations enter the dirty state, so the
cache line must propagate downstream before it can be freed.
Writing to a clean cache line marks it as dirty because the cache now contains write data that has not propagated
downstream. The line can be explicitly returned to the clean state using a clean maintenance operation (0x1 or 0x3), but
this is not required. Typically, the cache automatically propagates dirty cache lines downstream when it needs to
reallocate them.
Evictions happen when a cached read or write needs to allocate a cache line that is already in the clean or dirty state.
The eviction transitions the line momentarily to the invalid state, ready for allocation. For clean cache lines, this happens
instantaneously. For dirty cache lines, the cache must first propagate the cache line contents downstream before it can
safely enter the invalid state.
Cache lines enter the pinned state using a pin maintenance operation (0x7) and exit only by an invalidate maintenance
operation (0x0 or 0x2).
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 343
 NOTE
The pin maintenance operation only marks the line as pinned; it does not perform any copying of data. When pinning
lines that exist in external memory devices, you must first pin the line, then copy the downstream data into the
pinned line by reading from the uncached XIP window.
## 4.4.1.3. Cache-as-SRAM
When you disabled the cache of RP2040, the cache would map the entire cache memory at 0x15000000. RP2350 replaces
this with the ability to pin individual cache lines. You can use this in the following ways:
• Pin the entire cache at some address range to use the entire cache as SRAM
• Pin one full cache way to make half of the cache available for cache-as-SRAM use (the remaining cache way still
functions as usual)
• Pin an address range that that maps critical flash contents
 NOTE
Pinned cache lines are not accessible when the cache is disabled via the CTRL register (CTRL.EN_SECURE or
CTRL.EN_NONSECURE depending on security level of the bus access).
Because the QMI only occupies the lower half of the 64 MB XIP address space, you can pin cache lines outside of the
QMI address range (e.g. at the top of the XIP space) to avoid interfering with any QMI accesses. As a general rule, the
more cache you pin, the lower the cache hit rate for other accesses.
Cache lines are pinned using the pin maintenance operation (0x7), which performs the following steps:
1. An implicit invalidate-by-address operation (0x2) using the full address of the maintenance operation
◦
This ensures that each address is allocated in only one cache way (required for correct cache operation)
1. Select the cache line to be pinned, using bit 13 to select the cache way, and bits 12:3 to select the cache set (as
with 0x0/0x1 invalidate/clean by set/way commands)
1. Write the address to the cache line’s tag entry
2. Change the cache line’s state to pinned (as per the state diagram in Section 4.4.1.2)
3. Update the cache line’s tag with the full address of the maintenance operation
After a pin operation, cached reads and writes to the specified address always hit the cache until that cache line is
either invalidated or pinned to a different address.
 NOTE
Pinning two addresses that are equal modulo cache size pins the same cache line twice. It does not pin two different
cache lines. The second pin will overwrite the first.
When a cached access hits a pinned cache line, it behaves the same as a dirty line. The cache reads and writes as if
allocated in the cache by normal means.
Cache eviction policy is random, and the cache only makes one attempt to select an eviction way. If the cache selects
to evict a pinned line, the eviction fails, and the access is demoted to an uncached access. As a result, a cache with one
way pinned does not behave exactly the same as a direct-mapped 8 kB cache, but average-case performance is similar.
Cache line states are stored in the cache tag memory stored in the XIP memory power domain. This memory contents
do not change on reset, so pinned lines remain pinned across resets. If the XIP memory power domain is not powered
down, memory contents do not change across power cycles of the switched core reset domain. The bootrom clears the
tag memory upon entering the flash boot or NSBOOT (USB boot) path, but watchdog scratch vector reboots can boot
directly into pinned XIP cache lines.
RP2350 Datasheet
4.4. External flash and PSRAM (XIP) 344
4.4.2. QSPI Memory Interface (QMI)
Uncached accesses and cache misses require access to external memory. The QSPI memory interface (QMI) provides
this access, as documented in Section 12.14. The QMI supports:
• Up to two external QSPI devices, with separate chip selects and shared clock/data pins
◦
Banked configuration registers, including different SCK frequencies and QSPI opcodes
• Memory-mapped reads and writes (writes must be enabled via CTRL.WRITABLE_M0/CTRL.WRITABLE_M1)
• Serial/dual/quad-SPI transfer formats
• SCK speeds as high as clk_sys
• 8/16/32-bit accesses for uncached accesses, and 64-bit accesses for cache line fills
• Automatic chaining of sequentially addressed accesses into a single QSPI transfer
• Address translation (4 × 4 MB windows per QSPI device)
◦
Flash storage addresses can differ from runtime addresses, e.g. for multiple OTA upgrade image slots
◦
Allows code and data segments, or Secure and Non-secure images, to be mapped separately
• Direct-mode FIFO interface for programming and configuring external QSPI devices
XIP accesses via the two cache AHB ports, and from the DMA streaming hardware, arbitrate for access to the QMI. A
separate APB port configures the QMI.
The QMI is a new memory interface designed for RP2350, replacing the SSI peripheral on RP2040.
## 4.4.3. Streaming DMA interface
As the flash is generally much larger than on-chip SRAM, it’s often useful to stream chunks of data into memory from
flash. It’s convenient to have the DMA stream this data in the background while software in the foreground does other
things. It’s even more convenient if code can continue to execute from flash whilst this takes place.
This doesn’t interact well with standard XIP operation because QMI serial transfers force lengthy bus stalls on the DMA.
These stalls are tolerable for a processor because an in-order processor tends to have nothing better to do while
waiting for an instruction fetch to retire, and because typical code execution tends to have much higher cache hit rates
than bulk streaming of infrequently accessed data. In contrast, stalling the DMA prevents any other active DMA
channels from making progress during this time, slowing overall DMA throughput.
The STREAM_ADDR and STREAM_CTR registers are used to program a linear sequence of flash reads. The XIP
subsystem performs these reads in the background in a best-effort fashion. To minimise impact on code executed from
flash whilst the stream is ongoing, the streaming hardware has lower priority access to the QMI than regular XIP
accesses, and there is a brief cooldown (9 cycles) between the last XIP cache miss and resuming streaming. This
avoids increases in initial access latency on XIP cache misses.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/flash/xip_stream/flash_xip_stream.c Lines 45 - 48
45 while (!(xip_ctrl_hw->stat & XIP_STAT_FIFO_EMPTY))
46 (void) xip_ctrl_hw->stream_fifo;
47 xip_ctrl_hw->stream_addr = (uint32_t) &random_test_data[0];
48 xip_ctrl_hw->stream_ctr = count_of(random_test_data);
The streamed data is pushed to a small FIFO, which generates DREQ signals that tell the DMA to collect the streamed
data. As the DMA does not initiate a read until after reading the data from flash, the DMA does not stall when accessing
the data. The DMA can then retrieve this data through the auxiliary AHB port, which provides direct single-cycle access
to the streaming data FIFO.
On RP2350, you can also use the auxiliary AHB port to access the QMI direct-mode FIFOs. This is faster than accessing
RP2350 Datasheet
4.4. External flash and PSRAM (XIP) 345
the FIFOs through the QMI APB configuration port. When QMI access chaining is enabled, the streaming XIP DMA is
close to the maximum theoretical QSPI throughput, but the direct-mode FIFOs are available on AHB for situations that
require 100% of the theoretical throughput.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/flash/xip_stream/flash_xip_stream.c Lines 58 - 70
58 const uint dma_chan = 0;
59 dma_channel_config cfg = dma_channel_get_default_config(dma_chan);
60 channel_config_set_read_increment(&cfg, false);
61 channel_config_set_write_increment(&cfg, true);
62 channel_config_set_dreq(&cfg, DREQ_XIP_STREAM);
63 dma_channel_configure(
64 dma_chan,
65 &cfg,
66 (void *) buf, // Write addr
67 (const void *) XIP_AUX_BASE, // Read addr
68 count_of(random_test_data), // Transfer count
69 true // Start immediately!
70 );
## 4.4.4. Performance counters
The XIP subsystem provides two performance counters. These are 32 bits in size, saturate upon reaching 0xffffffff,
and are cleared by writing any value. They count:
1. The total number of XIP accesses, to any alias
2. The number of XIP accesses that resulted in a cache hit
This provides a way to profile the cache hit rate for common use cases.
4.4.5. List of XIP_CTRL registers
The XIP control registers start at a base address of 0x400c8000 (defined as XIP_CTRL_BASE in SDK).
Table 439. List of XIP
registers
Offset Name Info
0x00 CTRL Cache control register. Read-only from a Non-secure context.
0x08 STAT
0x0c CTR_HIT Cache Hit counter
0x10 CTR_ACC Cache Access counter
0x14 STREAM_ADDR FIFO stream address
0x18 STREAM_CTR FIFO stream control
0x1c STREAM_FIFO FIFO stream data
XIP: CTRL Register
Offset: 0x00
Description
Cache control register. Read-only from a Non-secure context.
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 346
Table 440. CTRL
Register
Bits Description Type Reset
31:12 Reserved. - -
11 WRITABLE_M1: If 1, enable writes to XIP memory window 1 (addresses
0x11000000 through 0x11ffffff, and their uncached mirrors). If 0, this region is
read-only.
XIP memory is read-only by default. This bit must be set to enable writes if a
RAM device is attached on QSPI chip select 1.
The default read-only behaviour avoids two issues with writing to a read-only
QSPI device (e.g. flash). First, a write will initially appear to succeed due to
caching, but the data will eventually be lost when the written line is evicted,
causing unpredictable behaviour.
Second, when a written line is evicted, it will cause a write command to be
issued to the flash, which can break the flash out of its continuous read mode.
After this point, flash reads will return garbage. This is a security concern, as it
allows Non-secure software to break Secure flash reads if it has permission to
write to any flash address.
Note the read-only behaviour is implemented by downgrading writes to reads,
so writes will still cause allocation of an address, but have no other effect.
RW 0x0
10 WRITABLE_M0: If 1, enable writes to XIP memory window 0 (addresses
0x10000000 through 0x10ffffff, and their uncached mirrors). If 0, this region is
read-only.
XIP memory is read-only by default. This bit must be set to enable writes if a
RAM device is attached on QSPI chip select 0.
The default read-only behaviour avoids two issues with writing to a read-only
QSPI device (e.g. flash). First, a write will initially appear to succeed due to
caching, but the data will eventually be lost when the written line is evicted,
causing unpredictable behaviour.
Second, when a written line is evicted, it will cause a write command to be
issued to the flash, which can break the flash out of its continuous read mode.
After this point, flash reads will return garbage. This is a security concern, as it
allows Non-secure software to break Secure flash reads if it has permission to
write to any flash address.
Note the read-only behaviour is implemented by downgrading writes to reads,
so writes will still cause allocation of an address, but have no other effect.
RW 0x0
9 SPLIT_WAYS: When 1, route all cached+Secure accesses to way 0 of the
cache, and route all cached+Non-secure accesses to way 1 of the cache.
This partitions the cache into two half-sized direct-mapped regions, such that
Non-secure code can not observe cache line state changes caused by Secure
execution.
A full cache flush is required when changing the value of SPLIT_WAYS. The
flush should be performed whilst SPLIT_WAYS is 0, so that both cache ways
are accessible for invalidation.
RW 0x0
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 347
Bits Description Type Reset
8 MAINT_NONSEC: When 0, Non-secure accesses to the cache maintenance
address window (addr[27] == 1, addr[26] == 0) will generate a bus error. When
1, Non-secure accesses can perform cache maintenance operations by writing
to the cache maintenance address window.
Cache maintenance operations may be used to corrupt Secure data by
invalidating cache lines inappropriately, or map Secure content into a Nonsecure region by pinning cache lines. Therefore this bit should generally be set
to 0, unless Secure code is not using the cache.
Care should also be taken to clear the cache data memory and tag memory
before granting maintenance operations to Non-secure code.
RW 0x0
7 NO_UNTRANSLATED_NONSEC: When 1, Non-secure accesses to the
uncached, untranslated window (addr[27:26] == 3) will generate a bus error.
RW 0x1
6 NO_UNTRANSLATED_SEC: When 1, Secure accesses to the uncached,
untranslated window (addr[27:26] == 3) will generate a bus error.
RW 0x0
5 NO_UNCACHED_NONSEC: When 1, Non-secure accesses to the uncached
window (addr[27:26] == 1) will generate a bus error. This may reduce the
number of SAU/MPU/PMP regions required to protect flash contents.
Note this does not disable access to the uncached, untranslated
window — see NO_UNTRANSLATED_SEC.
RW 0x0
4 NO_UNCACHED_SEC: When 1, Secure accesses to the uncached window
(addr[27:26] == 1) will generate a bus error. This may reduce the number of
SAU/MPU/PMP regions required to protect flash contents.
Note this does not disable access to the uncached, untranslated
window — see NO_UNTRANSLATED_SEC.
RW 0x0
3 POWER_DOWN: When 1, the cache memories are powered down. They retain
state, but can not be accessed. This reduces static power dissipation. Writing
1 to this bit forces CTRL_EN_SECURE and CTRL_EN_NONSECURE to 0, i.e. the
cache cannot be enabled when powered down.
RW 0x0
2 Reserved. - -
1 EN_NONSECURE: When 1, enable the cache for Non-secure accesses. When
enabled, Non-secure XIP accesses to the cached (addr[26] == 0) window will
query the cache, and QSPI accesses are performed only if the requested data
is not present. When disabled, Secure access ignore the cache contents, and
always access the QSPI interface.
Accesses to the uncached (addr[26] == 1) window will never query the cache,
irrespective of this bit.
RW 0x1
RP2350 Datasheet
## 4.4. External flash and PSRAM (XIP) 348
Bits Description Type Reset
0 EN_SECURE: When 1, enable the cache for Secure accesses. When enabled,
Secure XIP accesses to the cached (addr[26] == 0) window will query the
cache, and QSPI accesses are performed only if the requested data is not
present. When disabled, Secure access ignore the cache contents, and always
access the QSPI interface.
Accesses to the uncached (addr[26] == 1) window will never query the cache,
irrespective of this bit.
There is no cache-as-SRAM address window. Cache lines are allocated for
SRAM-like use by individually pinning them, and keeping the cache enabled.
RW 0x1
XIP: STAT Register
Offset: 0x08
Table 441. STAT
Register
Bits Description Type Reset
31:3 Reserved. - -
2 FIFO_FULL: When 1, indicates the XIP streaming FIFO is completely full.
The streaming FIFO is 2 entries deep, so the full and empty
flag allow its level to be ascertained.
RO 0x0
1 FIFO_EMPTY: When 1, indicates the XIP streaming FIFO is completely empty. RO 0x1
0 Reserved. - -
XIP: CTR_HIT Register
Offset: 0x0c
Description
Cache Hit counter
Table 442. CTR_HIT
Register
Bits Description Type Reset
31:0 A 32 bit saturating counter that increments upon each cache hit,
i.e. when an XIP access is serviced directly from cached data.
Write any value to clear.
WC 0x00000000
XIP: CTR_ACC Register
Offset: 0x10
Description
Cache Access counter
Table 443. CTR_ACC
Register
Bits Description Type Reset
31:0 A 32 bit saturating counter that increments upon each XIP access,
whether the cache is hit or not. This includes noncacheable accesses.
Write any value to clear.
WC 0x00000000
XIP: STREAM_ADDR Register
Offset: 0x14
RP2350 Datasheet
4.4. External flash and PSRAM (XIP) 349
Description
FIFO stream address
Table 444.
STREAM_ADDR
Register
Bits Description Type Reset
31:2 The address of the next word to be streamed from flash to the streaming
FIFO.
Increments automatically after each flash access.
Write the initial access address here before starting a streaming read.
RW 0x00000000
1:0 Reserved. - -
XIP: STREAM_CTR Register
Offset: 0x18
Description
FIFO stream control
Table 445.
STREAM_CTR Register
Bits Description Type Reset
31:22 Reserved. - -
21:0 Write a nonzero value to start a streaming read. This will then
progress in the background, using flash idle cycles to transfer
a linear data block from flash to the streaming FIFO.
Decrements automatically (1 at a time) as the stream
progresses, and halts on reaching 0.
Write 0 to halt an in-progress stream, and discard any in-flight
read, so that a new stream can immediately be started (after
draining the FIFO and reinitialising STREAM_ADDR)
RW 0x000000
XIP: STREAM_FIFO Register
Offset: 0x1c
Description
FIFO stream data
Table 446.
STREAM_FIFO
Register
Bits Description Type Reset
31:0 Streamed data is buffered here, for retrieval by the system DMA.
This FIFO can also be accessed via the XIP_AUX slave, to avoid exposing
the DMA to bus stalls caused by other XIP traffic.
RF 0x00000000
4.4.6. List of XIP_AUX registers
The XIP_AUX port provides fast AHB access to the streaming FIFO and the QMI Direct Mode FIFOs, to reduce the cost of
DMA access to these FIFOs.
Table 447. List of
XIP_AUX registers
Offset Name Info
0x0 STREAM Read the XIP stream FIFO (fast bus access to
XIP_CTRL_STREAM_FIFO)
0x4 QMI_DIRECT_TX Write to the QMI direct-mode TX FIFO (fast bus access to
QMI_DIRECT_TX)
RP2350 Datasheet
4.4. External flash and PSRAM (XIP) 350
Offset Name Info
0x8 QMI_DIRECT_RX Read from the QMI direct-mode RX FIFO (fast bus access to
QMI_DIRECT_RX)
XIP_AUX: STREAM Register
Offset: 0x0
Table 448. STREAM
Register
Bits Description Type Reset
31:0 Read the XIP stream FIFO (fast bus access to XIP_CTRL_STREAM_FIFO) RF 0x00000000
XIP_AUX: QMI_DIRECT_TX Register
Offset: 0x4
Description
Write to the QMI direct-mode TX FIFO (fast bus access to QMI_DIRECT_TX)
Table 449.
QMI_DIRECT_TX
Register
Bits Description Type Reset
31:21 Reserved. - -
20 NOPUSH: Inhibit the RX FIFO push that would correspond to this TX FIFO
entry.
Useful to avoid garbage appearing in the RX FIFO when pushing the command
at the beginning of a SPI transfer.
WF 0x0
19 OE: Output enable (active-high). For single width (SPI), this field is ignored, and
SD0 is always set to output, with SD1 always set to input.
For dual and quad width (DSPI/QSPI), this sets whether the relevant SDx pads
are set to output whilst transferring this FIFO record. In this case the
command/address should have OE set, and the data transfer should have OE
set or clear depending on the direction of the transfer.
WF 0x0
18 DWIDTH: Data width. If 0, hardware will transmit the 8 LSBs of the DIRECT_TX
DATA field, and return an 8-bit value in the 8 LSBs of DIRECT_RX. If 1, the full
16-bit width is used. 8-bit and 16-bit transfers can be mixed freely.
WF 0x0
17:16 IWIDTH: Configure whether this FIFO record is transferred with
single/dual/quad interface width (0/1/2). Different widths can be mixed freely.
WF 0x0
Enumerated values:
0x0 → S: Single width
0x1 → D: Dual width
0x2 → Q: Quad width
15:0 DATA: Data pushed here will be clocked out falling edges of SCK (or before
the very first rising edge of SCK, if this is the first pulse). For each byte clocked
out, the interface will simultaneously sample one byte, on rising edges of SCK,
and push this to the DIRECT_RX FIFO.
For 16-bit data, the least-significant byte is transmitted first.
WF 0x0000
XIP_AUX: QMI_DIRECT_RX Register
Offset: 0x8
RP2350 Datasheet
4.4. External flash and PSRAM (XIP) 351
Description
Read from the QMI direct-mode RX FIFO (fast bus access to QMI_DIRECT_RX)
Table 450.
QMI_DIRECT_RX
Register
Bits Description Type Reset
31:16 Reserved. - -
15:0 With each byte clocked out on the serial interface, one byte will simultaneously
be clocked in, and will appear in this FIFO. The serial interface will stall when
this FIFO is full, to avoid dropping data.
When 16-bit data is pushed into the TX FIFO, the corresponding RX FIFO push
will also contain 16 bits of data. The least-significant byte is the first one
received.
RF 0x0000
4.5. OTP
RP2350 contains 8 kB of one-time-programmable storage (OTP), which stores:
• Manufacturing information such as unique device ID
• Boot configuration such as non-default crystal oscillator frequency
• Public key fingerprint(s) for boot signature enforcement
• Symmetric keys for decryption of external flash contents into SRAM
• User-defined contents, including bootable program images (Section 5.10.7)
The OTP storage is structured as 4096 × 24-bit rows. Each row contains 16 bits of data and 8 bits of parity information,
providing 8 kB of data storage. OTP bit cells are initially 0 and can be programmed to 1. However, they cannot be cleared
back to 0 under any circumstance. This ensures that security-critical flags, such as debug disables, are physically
impossible to clear once set. However, you must also take care to program the correct values.
For more information about the OTP subsystem, see Chapter 13.





## .4. Function select
To allocate a function to a GPIO, write to the FUNCSEL field in the CTRL register corresponding to the pin. For a list of GPIOs
and corresponding registers, see Table 645. For an example, see GPIO0_CTRL. The descriptions for the functions listed
in this table can be found in Table 646.
Each GPIO can only select one function at a time. Each peripheral input (e.g. UART0 RX) should only be selected by one
GPIO at a time. If you connect the same peripheral input to multiple GPIOs, the peripheral sees the logical OR of these
GPIO inputs.
RP2350 Datasheet
## .4. Function select 589
Table 645. General
Purpose Input/Output
(GPIO) Bank 0
Functions
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
0 SPI0 RX UART0 TX I2C0 SDA PWM0 A SIO PIO0 PIO1 PIO2 QMI CS1n USB OVCUR DET
1 SPI0 CSn UART0 RX I2C0 SCL PWM0 B SIO PIO0 PIO1 PIO2 TRACECLK USB VBUS DET
2 SPI0 SCK UART0 CTS I2C1 SDA PWM1 A SIO PIO0 PIO1 PIO2 TRACEDATA0 USB VBUS EN UART0 TX
3 SPI0 TX UART0 RTS I2C1 SCL PWM1 B SIO PIO0 PIO1 PIO2 TRACEDATA1 USB OVCUR DET UART0 RX
4 SPI0 RX UART1 TX I2C0 SDA PWM2 A SIO PIO0 PIO1 PIO2 TRACEDATA2 USB VBUS DET
5 SPI0 CSn UART1 RX I2C0 SCL PWM2 B SIO PIO0 PIO1 PIO2 TRACEDATA3 USB VBUS EN
6 SPI0 SCK UART1 CTS I2C1 SDA PWM3 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 TX
7 SPI0 TX UART1 RTS I2C1 SCL PWM3 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 RX
8 SPI1 RX UART1 TX I2C0 SDA PWM4 A SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS EN
9 SPI1 CSn UART1 RX I2C0 SCL PWM4 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
10 SPI1 SCK UART1 CTS I2C1 SDA PWM5 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 TX
11 SPI1 TX UART1 RTS I2C1 SCL PWM5 B SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 RX
12 HSTX SPI1 RX UART0 TX I2C0 SDA PWM6 A SIO PIO0 PIO1 PIO2 CLOCK GPIN0 USB OVCUR DET
13 HSTX SPI1 CSn UART0 RX I2C0 SCL PWM6 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT0 USB VBUS DET
14 HSTX SPI1 SCK UART0 CTS I2C1 SDA PWM7 A SIO PIO0 PIO1 PIO2 CLOCK GPIN1 USB VBUS EN UART0 TX
15 HSTX SPI1 TX UART0 RTS I2C1 SCL PWM7 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT1 USB OVCUR DET UART0 RX
16 HSTX SPI0 RX UART0 TX I2C0 SDA PWM0 A SIO PIO0 PIO1 PIO2 USB VBUS DET
17 HSTX SPI0 CSn UART0 RX I2C0 SCL PWM0 B SIO PIO0 PIO1 PIO2 USB VBUS EN
18 HSTX SPI0 SCK UART0 CTS I2C1 SDA PWM1 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART0 TX
19 HSTX SPI0 TX UART0 RTS I2C1 SCL PWM1 B SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS DET UART0 RX
20 SPI0 RX UART1 TX I2C0 SDA PWM2 A SIO PIO0 PIO1 PIO2 CLOCK GPIN0 USB VBUS EN
21 SPI0 CSn UART1 RX I2C0 SCL PWM2 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT0 USB OVCUR DET
22 SPI0 SCK UART1 CTS I2C1 SDA PWM3 A SIO PIO0 PIO1 PIO2 CLOCK GPIN1 USB VBUS DET UART1 TX
RP2350 Datasheet
## .4. Function select 590
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
23 SPI0 TX UART1 RTS I2C1 SCL PWM3 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT1 USB VBUS EN UART1 RX
24 SPI1 RX UART1 TX I2C0 SDA PWM4 A SIO PIO0 PIO1 PIO2 CLOCK GPOUT2 USB OVCUR DET
25 SPI1 CSn UART1 RX I2C0 SCL PWM4 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT3 USB VBUS DET
26 SPI1 SCK UART1 CTS I2C1 SDA PWM5 A SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 TX
27 SPI1 TX UART1 RTS I2C1 SCL PWM5 B SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 RX
28 SPI1 RX UART0 TX I2C0 SDA PWM6 A SIO PIO0 PIO1 PIO2 USB VBUS DET
29 SPI1 CSn UART0 RX I2C0 SCL PWM6 B SIO PIO0 PIO1 PIO2 USB VBUS EN
GPIOs 30 through 47 are QFN-80 only:
30 SPI1 SCK UART0 CTS I2C1 SDA PWM7 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART0 TX
31 SPI1 TX UART0 RTS I2C1 SCL PWM7 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 RX
32 SPI0 RX UART0 TX I2C0 SDA PWM8 A SIO PIO0 PIO1 PIO2 USB VBUS EN
33 SPI0 CSn UART0 RX I2C0 SCL PWM8 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
34 SPI0 SCK UART0 CTS I2C1 SDA PWM9 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 TX
35 SPI0 TX UART0 RTS I2C1 SCL PWM9 B SIO PIO0 PIO1 PIO2 USB VBUS EN UART0 RX
36 SPI0 RX UART1 TX I2C0 SDA PWM10 A SIO PIO0 PIO1 PIO2 USB OVCUR DET
37 SPI0 CSn UART1 RX I2C0 SCL PWM10 B SIO PIO0 PIO1 PIO2 USB VBUS DET
38 SPI0 SCK UART1 CTS I2C1 SDA PWM11 A SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 TX
39 SPI0 TX UART1 RTS I2C1 SCL PWM11 B SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 RX
40 SPI1 RX UART1 TX I2C0 SDA PWM8 A SIO PIO0 PIO1 PIO2 USB VBUS DET
41 SPI1 CSn UART1 RX I2C0 SCL PWM8 B SIO PIO0 PIO1 PIO2 USB VBUS EN
42 SPI1 SCK UART1 CTS I2C1 SDA PWM9 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 TX
43 SPI1 TX UART1 RTS I2C1 SCL PWM9 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 RX
44 SPI1 RX UART0 TX I2C0 SDA PWM10 A SIO PIO0 PIO1 PIO2 USB VBUS EN
RP2350 Datasheet
## .4. Function select 591
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
45 SPI1 CSn UART0 RX I2C0 SCL PWM10 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
46 SPI1 SCK UART0 CTS I2C1 SDA PWM11 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 TX
47 SPI1 TX UART0 RTS I2C1 SCL PWM11 B SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS EN UART0 RX
RP2350 Datasheet
## .4. Function select 592
Table 646. GPIO User
Bank function
descriptions
Function Name Description
SPIx Connect one of the internal PL022 SPI peripherals to GPIO.
UARTx Connect one of the internal PL011 UART peripherals to GPIO.
I2Cx Connect one of the internal DW I2C peripherals to GPIO.
PWMx A/B Connect a PWM slice to GPIO. There are twelve PWM slices, each with two output
channels (A/B). The B pin can also be used as an input, for frequency and duty cycle
measurement.
SIO Software control of GPIO from the Single-cycle IO (SIO) block. The SIO function (F5)
must be selected for the processors to drive a GPIO, but the input is always connected,
so software can check the state of GPIOs at any time.
PIOx Connect one of the programmable IO blocks (PIO) to GPIO. PIO can implement a wide
variety of interfaces, and has its own internal pin mapping hardware, allowing flexible
placement of digital interfaces on Bank 0 GPIOs. The PIO function (F6, F7, F8) must be
selected for PIO to drive a GPIO, but the input is always connected, so the PIOs can
always see the state of all pins.
HSTX Connect the high-speed transmit peripheral (HSTX) to GPIO.
CLOCK GPINx General purpose clock inputs. Can be routed to a number of internal clock domains on
RP2350, e.g. to provide a 1Hz clock for the AON Timer, or can be connected to an
internal frequency counter.
CLOCK GPOUTx General purpose clock outputs. Can drive a number of internal clocks (including PLL
outputs) onto GPIOs, with optional integer divide.
TRACECLK, TRACEDATAx CoreSight execution trace output from Cortex-M33 processors (Arm-only).
USB OVCUR DET/VBUS
DET/VBUS EN
USB power control signals to/from the internal USB controller.
QMI CS1n Auxiliary chip select for QSPI bus, to allow execute-in-place from an additional flash or
PSRAM device.
Bank 1 function select operates identically to Bank 0, but its registers are in a different register block, starting with
USBPHY_DP_CTRL.
Table 647. GPIO Bank
1 Functions
Pin F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
USB DP UART1 TX I2C0 SDA SIO
USB DM UART1 RX I2C0 SCL SIO
QSPI SCK QMI SCK UART1 CTS I2C1 SDA SIO UART1 TX
QSPI CSn QMI CS0n UART1 RTS I2C1 SCL SIO UART1 RX
QSPI SD0 QMI SD0 UART0 TX I2C0 SDA SIO
QSPI SD1 QMI SD1 UART0 RX I2C0 SCL SIO
QSPI SD2 QMI SD2 UART0 CTS I2C1 SDA SIO UART0 TX
QSPI SD3 QMI SD3 UART0 RTS I2C1 SCL SIO UART0 RX
Table 648. GPIO bank
1 function
descriptions
Function Name Description
UARTx Connect one of the internal PL011 UART peripherals to GPIO.
I2Cx Connect one of the internal DW I2C peripherals to GPIO.
RP2350 Datasheet
## .4. Function select 593
Function Name Description
SIO Software control of GPIO, from the single-cycle IO (SIO) block. The SIO function (F5) must be selected
for the processors to drive a GPIO, but the input is always connected, so software can check the state
of GPIOs at any time.
QMI QSPI memory interface peripheral, used for execute-in-place from external QSPI flash or PSRAM
memory devices.
The six QSPI Bank GPIO pins are typically used by the XIP peripheral to communicate with an external flash device.
However, there are two scenarios where the pins can be used as software-controlled GPIOs:
• If a SPI or Dual-SPI flash device is used for execute-in-place, then the SD2 and SD3 pins are not used for flash
access, and can be used for other GPIO functions on the circuit board.
• If RP2350 is used in a flashless configuration (USB and OTP boot only), then all six pins can be used for softwarecontrolled GPIO functions.
## .5. Interrupts
An interrupt can be generated for every GPIO pin in four scenarios:
• Level High: the GPIO pin is a logical 1
• Level Low: the GPIO pin is a logical 0
• Edge High: the GPIO has transitioned from a logical 0 to a logical 1
• Edge Low: the GPIO has transitioned from a logical 1 to a logical 0
The level interrupts are not latched. This means that if the pin is a logical 1 and the level high interrupt is active, it will
become inactive as soon as the pin changes to a logical 0. The edge interrupts are stored in the INTR register and can be
cleared by writing to the INTR register.
There are enable, status, and force registers for three interrupt destinations: proc 0, proc 1, and dormant_wake. For proc
0 the registers are enable (PROC0_INTE0), status (PROC0_INTS0), and force (PROC0_INTF0). Dormant wake is used to
wake the ROSC or XOSC up from dormant mode. See Section 6.5.6.2 for more information on dormant mode.
There is an interrupt output for each combination of IO bank, IRQ destination, and security domain. In total there are
twelve such outputs:
• IO Bank 0 to dormant wake (Secure and Non-secure)
• IO Bank 0 to proc 0 (Secure and Non-secure)
• IO Bank 0 to proc 1 (Secure and Non-secure)
• IO QSPI to dormant wake (Secure and Non-secure)
• IO QSPI to proc 0 (Secure and Non-secure)
• IO QSPI to proc 1 (Secure and Non-secure)
Each interrupt output has its own array of enable registers (INTE) that configures which GPIO events cause the interrupt
to assert. The interrupt asserts when at least one enabled event occurs, and de-asserts when all enabled events have
been acknowledged via the relevant INTR register.
This means the user can watch for several GPIO events at once.
Summary registers can be used to quickly check for pending GPIO interrupts. See IRQSUMMARY_PROC0_NONSECURE0
for an example.
RP2350 Datasheet
## .5. Interrupts 594
## .6. Pads
 CAUTION
Under certain conditions, pull-down does not function as expected. For more information, see RP2350-E## .
Each GPIO is connected off-chip via a pad. Pads are the electrical interface between the chip’s internal logic and
external circuitry. They translate signal voltage levels, support higher currents and offer some protection against
electrostatic discharge (ESD) events. You can adjust pad electrical behaviour to meet the requirements of external
circuitry in the following ways:
• Output drive strength can be set to 2mA, 4mA, 8mA or 12mA.
• Output slew rate can be set to slow or fast.
• Input hysteresis (Schmitt trigger mode) can be enabled.
• A pull-up or pull-down can be enabled, to set the output signal level when the output driver is disabled.
• The input buffer can be disabled, to reduce current consumption when the pad is unused, unconnected or
connected to an analogue signal.
An example pad is shown in Figure 42.
PAD
GPIO
Muxing
Slew Rate
Output Enable
Output Data
Drive Strength
Input Enable
Input Data
Schmitt Trigger
Pull Up / Pull Down
2
2
Figure 42. Diagram of
a single IO pad.
The pad’s Output Enable, Output Data and Input Data ports connect, via the IO mux, to the function controlling the pad.
All other ports are controlled from the pad control register. You can use this register to disable the pad’s output driver by
overriding the Output Enable signal from the function controlling the pad. See GPIO0 for an example of a pad control
register.
Both the output signal level and acceptable input signal level at the pad are determined by the digital IO supply (IOVDD).
IOVDD can be any nominal voltage between 1.8V and 3.3V, but to meet specification when powered at 1.8V, the pad
input thresholds must be adjusted by writing a 1 to the pad VOLTAGE_SELECT registers. By default, the pad input thresholds
are valid for an IOVDD voltage between 2.5V and 3.3V. Using a voltage of 1.8V with the default input thresholds is a safe
operating mode, but it will result in input thresholds that don’t meet specification.
 WARNING
Using IOVDD voltages greater than 1.8V, with the input thresholds set for 1.8V may result in damage to the chip.
Pad input threshold are adjusted on a per bank basis, with separate VOLTAGE_SELECT registers for the pads associated with
the User IO bank (IO Bank 0) and the QSPI IO bank. However, both banks share the same digital IO supply (IOVDD), so
both register should always be set to the same value.
Pad register details are available in Section ## .11.3, “Pad Control - User Bank” and Section ## .11.4, “Pad Control - QSPI
Bank”.
RP2350 Datasheet
## .6. Pads 595
## .6.1. Bus keeper mode
For each pad, only the pull-up or the pull-down resistor can be enabled at any given time. It is impossible to enable both
simultaneously. Instead, if you set both the GPIO0.PDE and GPIO0.PUE bits simultaneously then you enable bus keeper
mode, where the pad is:
• Pulled up when its input is high.
• Pulled down when its input is low.
When the output buffer is disabled, and the pad is not driven by any external source, this mode weakly retains the pad’s
current logical state. The pad does not float to mid-rail.
Bus keeper mode relies on control logic in the switched core domain, so does not function when the core is powered
down. Rather, powering down the core when bus keeper mode is enabled latches the current output controls (pull-up or
pull-down) in the pad isolation latches, as described in Section ## .7.
## .7. Pad isolation latches
RP2350 features extended low-power states that allow all internal logic, with the exception of POWMAN and some
CoreSight debug logic, to fully power down under software control. This includes powering down all peripherals, the IO
muxing, and the pad control registers, which brings with it the risk that pad signals may experience unwanted
transitions when entering and exiting low-power states.
To ensure that pad states are well-defined at all times, all signals passing from the switched core power domain to the
pads pass through isolation latches. In normal operation, the latches are transparent, so the pads are controlled fully by
logic inside the switched core power domain, such as UARTs or the processors. However, when the ISO bit for each pad
is set (e.g. GPIO0.ISO) or the switched core domain is powered down, the control signals currently presented to that pad
are latched until the isolation is disabled. This includes the output enable state, output high/low level, and pull-up/pulldown resistor enable. The input signal from the pad back into the switched core domain is not isolated.
Consequently, when switched core logic is powered down, all Bank 0 and Bank 1 pads maintain the output state they
held immediately before the power down, unless overridden by always-on logic in POWMAN. When the switched core
power domain powers back up, all the GPIO ISO bits reset to 1, so the pre-power down state continues to be maintained
until user software starts up and clears the ISO bit to indicate it is ready to use the pad again. Pads whose IO muxing
has not yet been set up can be left isolated indefinitely, and will maintain their pre-power down state.
when software has finished setting up the IO muxing for a given pad, and the peripheral that is to be muxed in, the ISO
bit should be cleared. At this point the isolation latches will become transparent again: output signals passing through
the IO muxing block are now reflected in the pad output state, so peripherals can communicate with the outside world.
This process allows the switched core domain to be power cycled without causing any transitions on the pad outputs
that may interfere with the operation of external hardware connected to the pads.
 NOTE
Non-SDK applications ported from RP2040 must clear the ISO bit before using a GPIO, as this feature was not
present on RP2040. The SDK automatically clears the ISO bit when gpio_set_function() is called.
The isolation latches themselves are reset by the always-on power domain reset, namely any one of:
• Power-on reset
• Brownout reset
• RUN pin being asserted low
• SW-DP CDBGRSTREQ
• RP-AP rescue reset
The latches reset to the reset value of the signal being isolated. For example, on Bank 0 GPIOs, the input enable control
RP2350 Datasheet
## .7. Pad isolation latches 596
(GPIO0.IE) resets to 0 (input-disabled), so the isolation latches for these signals also take a reset value of 0. Resetting
the isolation latch forces the pad to assume its reset state even if it is currently isolated.
The ISO control bits (e.g. GPIO0.ISO) are reset by the top-level switched core domain isolation signal, which is asserted
by POWMAN before powering down the switched core domain and de-asserted after it is powered up. This means that
entering and exiting a sleep state where the switched core domain is unpowered leaves all GPIOs isolated after power
up; you can then re-engage them individually. The ISO control bits are not reset by the PADS register block reset driven
by the RESETS control registers: resetting the PADS register block returns non-isolated pads to their reset state, but has
no effect on isolated pads.
## .8. Processor GPIO controls (SIO)
The single-cycle IO subsystem (Section 3.1) contains memory-mapped GPIO registers. The processors can use these to
perform input/output operations on GPIOs:
• The GPIO_OUT and GPIO_HI_OUT registers set the output level: 1 = high, 0 = low
• The GPIO_OE and GPIO_HI_OE registers set the output enable: 1 = output, 0 = input
• The GPIO_IN and GPIO_HI_IN registers read the GPIO inputs
These registers are all 32 bits in size. The low registers (e.g. GPIO_OUT) connect to GPIOs 0 through 31, and the high
registers (e.g. GPIO_HI_OUT) connect to GPIOs 32 through 47, the QSPI pads, and the USB DM/DP pads.
For the output and output enable registers to take effect, the SIO function must be selected on each GPIO (function 5).
However, the GPIO input registers read back the GPIO input values even when the SIO function is not selected, so the
processor can always check the input state of any pin.
The SIO GPIO registers are shared between the two processors and between the Secure and Non-secure security
domains. This avoids programming errors introduced by selecting multiple GPIO functions for access from different
contexts.
Non-secure code’s view of the SIO registers is restricted by the Non-secure GPIO mask defined in GPIO_NSMASK0 and
GPIO_NSMASK1. Non-secure writes to Secure GPIOs are ignored. Non-secure reads of Secure GPIOs return 0.
These registers are documented in more detail in the SIO GPIO register section (Section 3.1.3).
The DMA cannot access registers in the SIO subsystem. The recommended method to DMA to GPIOs is a PIO program
that continuously transfers TX FIFO data to the GPIO outputs, which provides more consistent timing than DMA directly
into GPIO registers.
## .## . GPIO coprocessor port
Coprocessor port 0 on each Cortex-M33 processor connects to a GPIO coprocessor interface. These coprocessor
instructions provide fast access to the SIO GPIO registers from Arm software:
• The equivalent of any SIO GPIO register access is a single instruction, without having to materialise a 32-bit
register address beforehand
• An indexed write operation on any single GPIO is a single instruction
• 64 bits can be read/written in a single instruction
This reduces the timing impact of GPIO accesses on surrounding software, for example when GPIO tracing has been
added to interrupt handlers diagnose complex timing issues.
Both Secure and Non-secure code may access the coprocessor. Non-secure code sees a restricted view of the GPIO
registers, defined by ACCESSCTRL GPIO_NSMASK0/1.
The GPIO coprocessor instruction set is documented in Section 3.6.1.
RP2350 Datasheet
## .8. Processor GPIO controls (SIO) 597
## .10. Software examples
## .10.1. Select an IO function
An IO pin can perform many different functions and must be configured before use. For example, you may want it to be
a UART_TX pin, or a PWM output. The SDK provides gpio_set_function for this purpose. Many SDK examples call
gpio_set_function early on to enable printing to a UART.
The SDK starts by defining a structure to represent the registers of IO Bank 0, the User IO bank. Each IO has a status
register, followed by a control register. For N IOs, the SDK instantiates the structure containing a status and control
register as io[N] to repeat it N times.
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2350/hardware_structs/include/hardware/structs/io_bank0.h Lines 179 - 445
179 typedef struct {
180 io_bank0_status_ctrl_hw_t io[48];
181
182 uint32_t _pad0[32];
183
184 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC0_SECURE0
  applies similarly to other array indexes)
185 _REG_(IO_BANK0_IRQSUMMARY_PROC0_SECURE0_OFFSET) // IO_BANK0_IRQSUMMARY_PROC0_SECURE0
186 // 0x80000000 [31] GPIO31 (0)
187 // 0x40000000 [30] GPIO30 (0)
188 // 0x20000000 [29] GPIO29 (0)
189 // 0x10000000 [28] GPIO28 (0)
190 // 0x08000000 [27] GPIO27 (0)
191 // 0x04000000 [26] GPIO26 (0)
192 // 0x02000000 [25] GPIO25 (0)
193 // 0x01000000 [24] GPIO24 (0)
194 // 0x00800000 [23] GPIO23 (0)
195 // 0x00400000 [22] GPIO22 (0)
196 // 0x00200000 [21] GPIO21 (0)
197 // 0x00100000 [20] GPIO20 (0)
198 // 0x00080000 [19] GPIO19 (0)
199 // 0x00040000 [18] GPIO18 (0)
200 // 0x00020000 [17] GPIO17 (0)
201 // 0x00010000 [16] GPIO16 (0)
202 // 0x00008000 [15] GPIO15 (0)
203 // 0x00004000 [14] GPIO14 (0)
204 // 0x00002000 [13] GPIO13 (0)
205 // 0x00001000 [12] GPIO12 (0)
206 // 0x00000800 [11] GPIO11 (0)
207 // 0x00000400 [10] GPIO10 (0)
208 // 0x00000200 [9] GPIO9 (0)
209 // 0x00000100 [8] GPIO8 (0)
210 // 0x00000080 [7] GPIO7 (0)
211 // 0x00000040 [6] GPIO6 (0)
212 // 0x00000020 [5] GPIO5 (0)
213 // 0x00000010 [4] GPIO4 (0)
214 // 0x00000008 [3] GPIO3 (0)
215 // 0x00000004 [2] GPIO2 (0)
216 // 0x00000002 [1] GPIO1 (0)
217 // 0x00000001 [0] GPIO0 (0)
218 io_ro_32 irqsummary_proc0_secure[2];
219
220 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0
  applies similarly to other array indexes)
221 _REG_(IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0
222 // 0x80000000 [31] GPIO31 (0)
RP2350 Datasheet
## 9.10. Software examples 598
223 // 0x40000000 [30] GPIO30 (0)
224 // 0x20000000 [29] GPIO29 (0)
225 // 0x10000000 [28] GPIO28 (0)
226 // 0x08000000 [27] GPIO27 (0)
227 // 0x04000000 [26] GPIO26 (0)
228 // 0x02000000 [25] GPIO25 (0)
229 // 0x01000000 [24] GPIO24 (0)
230 // 0x00800000 [23] GPIO23 (0)
231 // 0x00400000 [22] GPIO22 (0)
232 // 0x00200000 [21] GPIO21 (0)
233 // 0x00100000 [20] GPIO20 (0)
234 // 0x00080000 [19] GPIO19 (0)
235 // 0x00040000 [18] GPIO18 (0)
236 // 0x00020000 [17] GPIO17 (0)
237 // 0x00010000 [16] GPIO16 (0)
238 // 0x00008000 [15] GPIO15 (0)
239 // 0x00004000 [14] GPIO14 (0)
240 // 0x00002000 [13] GPIO13 (0)
241 // 0x00001000 [12] GPIO12 (0)
242 // 0x00000800 [11] GPIO11 (0)
243 // 0x00000400 [10] GPIO10 (0)
244 // 0x00000200 [9] GPIO9 (0)
245 // 0x00000100 [8] GPIO8 (0)
246 // 0x00000080 [7] GPIO7 (0)
247 // 0x00000040 [6] GPIO6 (0)
248 // 0x00000020 [5] GPIO5 (0)
249 // 0x00000010 [4] GPIO4 (0)
250 // 0x00000008 [3] GPIO3 (0)
251 // 0x00000004 [2] GPIO2 (0)
252 // 0x00000002 [1] GPIO1 (0)
253 // 0x00000001 [0] GPIO0 (0)
254 io_ro_32 irqsummary_proc0_nonsecure[2];
255
256 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC1_SECURE0
  applies similarly to other array indexes)
257 _REG_(IO_BANK0_IRQSUMMARY_PROC1_SECURE0_OFFSET) // IO_BANK0_IRQSUMMARY_PROC1_SECURE0
258 // 0x80000000 [31] GPIO31 (0)
259 // 0x40000000 [30] GPIO30 (0)
260 // 0x20000000 [29] GPIO29 (0)
261 // 0x10000000 [28] GPIO28 (0)
262 // 0x08000000 [27] GPIO27 (0)
263 // 0x04000000 [26] GPIO26 (0)
264 // 0x02000000 [25] GPIO25 (0)
265 // 0x01000000 [24] GPIO24 (0)
266 // 0x00800000 [23] GPIO23 (0)
267 // 0x00400000 [22] GPIO22 (0)
268 // 0x00200000 [21] GPIO21 (0)
269 // 0x00100000 [20] GPIO20 (0)
270 // 0x00080000 [19] GPIO19 (0)
271 // 0x00040000 [18] GPIO18 (0)
272 // 0x00020000 [17] GPIO17 (0)
273 // 0x00010000 [16] GPIO16 (0)
274 // 0x00008000 [15] GPIO15 (0)
275 // 0x00004000 [14] GPIO14 (0)
276 // 0x00002000 [13] GPIO13 (0)
277 // 0x00001000 [12] GPIO12 (0)
278 // 0x00000800 [11] GPIO11 (0)
279 // 0x00000400 [10] GPIO10 (0)
280 // 0x00000200 [9] GPIO9 (0)
281 // 0x00000100 [8] GPIO8 (0)
282 // 0x00000080 [7] GPIO7 (0)
283 // 0x00000040 [6] GPIO6 (0)
284 // 0x00000020 [5] GPIO5 (0)
285 // 0x00000010 [4] GPIO4 (0)
RP2350 Datasheet
## 9.10. Software examples 599
286 // 0x00000008 [3] GPIO3 (0)
287 // 0x00000004 [2] GPIO2 (0)
288 // 0x00000002 [1] GPIO1 (0)
289 // 0x00000001 [0] GPIO0 (0)
290 io_ro_32 irqsummary_proc1_secure[2];
291
292 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0
  applies similarly to other array indexes)
293 _REG_(IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0
294 // 0x80000000 [31] GPIO31 (0)
295 // 0x40000000 [30] GPIO30 (0)
296 // 0x20000000 [29] GPIO29 (0)
297 // 0x10000000 [28] GPIO28 (0)
298 // 0x08000000 [27] GPIO27 (0)
299 // 0x04000000 [26] GPIO26 (0)
300 // 0x02000000 [25] GPIO25 (0)
301 // 0x01000000 [24] GPIO24 (0)
302 // 0x00800000 [23] GPIO23 (0)
303 // 0x00400000 [22] GPIO22 (0)
304 // 0x00200000 [21] GPIO21 (0)
305 // 0x00100000 [20] GPIO20 (0)
306 // 0x00080000 [19] GPIO19 (0)
307 // 0x00040000 [18] GPIO18 (0)
308 // 0x00020000 [17] GPIO17 (0)
309 // 0x00010000 [16] GPIO16 (0)
310 // 0x00008000 [15] GPIO15 (0)
311 // 0x00004000 [14] GPIO14 (0)
312 // 0x00002000 [13] GPIO13 (0)
313 // 0x00001000 [12] GPIO12 (0)
314 // 0x00000800 [11] GPIO11 (0)
315 // 0x00000400 [10] GPIO10 (0)
316 // 0x00000200 [9] GPIO9 (0)
317 // 0x00000100 [8] GPIO8 (0)
318 // 0x00000080 [7] GPIO7 (0)
319 // 0x00000040 [6] GPIO6 (0)
320 // 0x00000020 [5] GPIO5 (0)
321 // 0x00000010 [4] GPIO4 (0)
322 // 0x00000008 [3] GPIO3 (0)
323 // 0x00000004 [2] GPIO2 (0)
324 // 0x00000002 [1] GPIO1 (0)
325 // 0x00000001 [0] GPIO0 (0)
326 io_ro_32 irqsummary_proc1_nonsecure[2];
327
328 // (Description copied from array index 0 register
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0 applies similarly to other array indexes)
329 _REG_(IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0
330 // 0x80000000 [31] GPIO31 (0)
331 // 0x40000000 [30] GPIO30 (0)
332 // 0x20000000 [29] GPIO29 (0)
333 // 0x10000000 [28] GPIO28 (0)
334 // 0x08000000 [27] GPIO27 (0)
335 // 0x04000000 [26] GPIO26 (0)
336 // 0x02000000 [25] GPIO25 (0)
337 // 0x01000000 [24] GPIO24 (0)
338 // 0x00800000 [23] GPIO23 (0)
339 // 0x00400000 [22] GPIO22 (0)
340 // 0x00200000 [21] GPIO21 (0)
341 // 0x00100000 [20] GPIO20 (0)
342 // 0x00080000 [19] GPIO19 (0)
343 // 0x00040000 [18] GPIO18 (0)
344 // 0x00020000 [17] GPIO17 (0)
345 // 0x00010000 [16] GPIO16 (0)
RP2350 Datasheet
## 9.10. Software examples 600
346 // 0x00008000 [15] GPIO15 (0)
347 // 0x00004000 [14] GPIO14 (0)
348 // 0x00002000 [13] GPIO13 (0)
349 // 0x00001000 [12] GPIO12 (0)
350 // 0x00000800 [11] GPIO11 (0)
351 // 0x00000400 [10] GPIO10 (0)
352 // 0x00000200 [9] GPIO9 (0)
353 // 0x00000100 [8] GPIO8 (0)
354 // 0x00000080 [7] GPIO7 (0)
355 // 0x00000040 [6] GPIO6 (0)
356 // 0x00000020 [5] GPIO5 (0)
357 // 0x00000010 [4] GPIO4 (0)
358 // 0x00000008 [3] GPIO3 (0)
359 // 0x00000004 [2] GPIO2 (0)
360 // 0x00000002 [1] GPIO1 (0)
361 // 0x00000001 [0] GPIO0 (0)
362 io_ro_32 irqsummary_dormant_wake_secure[2];
363
364 // (Description copied from array index 0 register
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0 applies similarly to other array indexes)
365 _REG_(IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0
366 // 0x80000000 [31] GPIO31 (0)
367 // 0x40000000 [30] GPIO30 (0)
368 // 0x20000000 [29] GPIO29 (0)
369 // 0x10000000 [28] GPIO28 (0)
370 // 0x08000000 [27] GPIO27 (0)
371 // 0x04000000 [26] GPIO26 (0)
372 // 0x02000000 [25] GPIO25 (0)
373 // 0x01000000 [24] GPIO24 (0)
374 // 0x00800000 [23] GPIO23 (0)
375 // 0x00400000 [22] GPIO22 (0)
376 // 0x00200000 [21] GPIO21 (0)
377 // 0x00100000 [20] GPIO20 (0)
378 // 0x00080000 [19] GPIO19 (0)
379 // 0x00040000 [18] GPIO18 (0)
380 // 0x00020000 [17] GPIO17 (0)
381 // 0x00010000 [16] GPIO16 (0)
382 // 0x00008000 [15] GPIO15 (0)
383 // 0x00004000 [14] GPIO14 (0)
384 // 0x00002000 [13] GPIO13 (0)
385 // 0x00001000 [12] GPIO12 (0)
386 // 0x00000800 [11] GPIO11 (0)
387 // 0x00000400 [10] GPIO10 (0)
388 // 0x00000200 [9] GPIO9 (0)
389 // 0x00000100 [8] GPIO8 (0)
390 // 0x00000080 [7] GPIO7 (0)
391 // 0x00000040 [6] GPIO6 (0)
392 // 0x00000020 [5] GPIO5 (0)
393 // 0x00000010 [4] GPIO4 (0)
394 // 0x00000008 [3] GPIO3 (0)
395 // 0x00000004 [2] GPIO2 (0)
396 // 0x00000002 [1] GPIO1 (0)
397 // 0x00000001 [0] GPIO0 (0)
398 io_ro_32 irqsummary_dormant_wake_nonsecure[2];
399
400 // (Description copied from array index 0 register IO_BANK0_INTR0 applies similarly to
  other array indexes)
401 _REG_(IO_BANK0_INTR0_OFFSET) // IO_BANK0_INTR0
402 // Raw Interrupts
403 // 0x80000000 [31] GPIO7_EDGE_HIGH (0)
404 // 0x40000000 [30] GPIO7_EDGE_LOW (0)
405 // 0x20000000 [29] GPIO7_LEVEL_HIGH (0)
406 // 0x10000000 [28] GPIO7_LEVEL_LOW (0)
RP2350 Datasheet
## 9.10. Software examples 601
407 // 0x08000000 [27] GPIO6_EDGE_HIGH (0)
408 // 0x04000000 [26] GPIO6_EDGE_LOW (0)
409 // 0x02000000 [25] GPIO6_LEVEL_HIGH (0)
410 // 0x01000000 [24] GPIO6_LEVEL_LOW (0)
411 // 0x00800000 [23] GPIO5_EDGE_HIGH (0)
412 // 0x00400000 [22] GPIO5_EDGE_LOW (0)
413 // 0x00200000 [21] GPIO5_LEVEL_HIGH (0)
414 // 0x00100000 [20] GPIO5_LEVEL_LOW (0)
415 // 0x00080000 [19] GPIO4_EDGE_HIGH (0)
416 // 0x00040000 [18] GPIO4_EDGE_LOW (0)
417 // 0x00020000 [17] GPIO4_LEVEL_HIGH (0)
418 // 0x00010000 [16] GPIO4_LEVEL_LOW (0)
419 // 0x00008000 [15] GPIO3_EDGE_HIGH (0)
420 // 0x00004000 [14] GPIO3_EDGE_LOW (0)
421 // 0x00002000 [13] GPIO3_LEVEL_HIGH (0)
422 // 0x00001000 [12] GPIO3_LEVEL_LOW (0)
423 // 0x00000800 [11] GPIO2_EDGE_HIGH (0)
424 // 0x00000400 [10] GPIO2_EDGE_LOW (0)
425 // 0x00000200 [9] GPIO2_LEVEL_HIGH (0)
426 // 0x00000100 [8] GPIO2_LEVEL_LOW (0)
427 // 0x00000080 [7] GPIO1_EDGE_HIGH (0)
428 // 0x00000040 [6] GPIO1_EDGE_LOW (0)
429 // 0x00000020 [5] GPIO1_LEVEL_HIGH (0)
430 // 0x00000010 [4] GPIO1_LEVEL_LOW (0)
431 // 0x00000008 [3] GPIO0_EDGE_HIGH (0)
432 // 0x00000004 [2] GPIO0_EDGE_LOW (0)
433 // 0x00000002 [1] GPIO0_LEVEL_HIGH (0)
434 // 0x00000001 [0] GPIO0_LEVEL_LOW (0)
435 io_rw_32 intr[6];
436
437 union {
438 struct {
439 io_bank0_irq_ctrl_hw_t proc0_irq_ctrl;
440 io_bank0_irq_ctrl_hw_t proc1_irq_ctrl;
441 io_bank0_irq_ctrl_hw_t dormant_wake_irq_ctrl;
442 };
443 io_bank0_irq_ctrl_hw_t irq_ctrl[3];
444 };
445 } io_bank0_hw_t;
A similar structure is defined for the pad control registers for IO bank 1. By default, all pads come out of reset ready to
use, with input enabled and output disable set to 0. Regardless, gpio_set_function in the SDK sets the input enable and
clears the output disable to engage the pad’s IO buffers and connect internal signals to the outside world. Finally, the
desired function select is written to the IO control register (see GPIO0_CTRL for an example of an IO control register).
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 36 - 53
36 // Select function for this GPIO, and ensure input/output are enabled at the pad.
37 // This also clears the input/output/irq override bits.
38 void gpio_set_function(uint gpio, gpio_function_t fn) {
39 check_gpio_param(gpio);
40 invalid_params_if(HARDWARE_GPIO, ((uint32_t)fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) &
  ~IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS);
41 // Set input enable on, output disable off
42 hw_write_masked(&pads_bank0_hw->io[gpio],
43 PADS_BANK0_GPIO0_IE_BITS,
44 PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
45 );
46 // Zero all fields apart from fsel; we want this IO to do what the peripheral tells it.
47 // This doesn't affect e.g. pullup/pulldown, as these are in pad controls.
48 io_bank0_hw->io[gpio].ctrl = fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
49 // Remove pad isolation now that the correct peripheral is in control of the pad
RP2350 Datasheet
## 9.10. Software examples 602
50 hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
51 }
## 9.10.2. Enable a GPIO interrupt
The SDK provides a method of being interrupted when a GPIO pin changes state:
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 186 - 196
186 void gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled) {
187 // either this call disables the interrupt or callback should already be set.
188 // this protects against enabling the interrupt without callback set
189 assert(!enabled || irq_has_handler(IO_IRQ_BANK0));
190
191 // Separate mask/force/status per-core, so check which core called, and
192 // set the relevant IRQ controls.
193 io_bank0_irq_ctrl_hw_t *irq_ctrl_base = get_core_num() ?
194 &io_bank0_hw->proc1_irq_ctrl : &io_bank0_hw-
  >proc0_irq_ctrl;
195 _gpio_set_irq_enabled(gpio, events, enabled, irq_ctrl_base);
196 }
gpio_set_irq_enabled uses a lower level function _gpio_set_irq_enabled:
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 173 - 184
173 static void _gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled,
  io_bank0_irq_ctrl_hw_t *irq_ctrl_base) {
174 // Clear stale events which might cause immediate spurious handler entry
175 gpio_acknowledge_irq(gpio, events);
176
177 io_rw_32 *en_reg = &irq_ctrl_base->inte[gpio / 8];
178 events <<= 4 * (gpio % 8);
179
180 if (enabled)
181 hw_set_bits(en_reg, events);
182 else
183 hw_clear_bits(en_reg, events);
184 }
The user provides a pointer to a callback function that is called when the GPIO event happens. An example application
that uses this system is hello_gpio_irq:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/gpio/hello_gpio_irq/hello_gpio_irq.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdio.h>
 8 #include "pico/stdlib.h"
 9 #include "hardware/gpio.h"
10
11 #define GPIO_WATCH_PIN 2
12
RP2350 Datasheet
## 9.10. Software examples 603
13 static char event_str[128];
14
15 void gpio_event_string(char *buf, uint32_t events);
16
17 void gpio_callback(uint gpio, uint32_t events) {
18 // Put the GPIO event(s) that just happened into event_str
19 // so we can print it
20 gpio_event_string(event_str, events);
21 printf("GPIO %d %s\n", gpio, event_str);
22 }
23
24 int main() {
25 stdio_init_all();
26
27 printf("Hello GPIO IRQ\n");
28 gpio_init(GPIO_WATCH_PIN);
29 gpio_set_irq_enabled_with_callback(GPIO_WATCH_PIN, GPIO_IRQ_EDGE_RISE |
  GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
30
31 // Wait forever
32 while (1);
33 }
34
35
36 static const char *gpio_irq_str[] = {
37 "LEVEL_LOW", // 0x1
38 "LEVEL_HIGH", // 0x2
39 "EDGE_FALL", // 0x4
40 "EDGE_RISE" // 0x8
41 };
42
43 void gpio_event_string(char *buf, uint32_t events) {
44 for (uint i = 0; i < 4; i++) {
45 uint mask = (1 << i);
46 if (events & mask) {
47 // Copy this event string into the user string
48 const char *event_str = gpio_irq_str[i];
49 while (*event_str != '\0') {
50 *buf++ = *event_str++;
51 }
52 events &= ~mask;
53
54 // If more events add ", "
55 if (events) {
56 *buf++ = ',';
57 *buf++ = ' ';
58 }
59 }
60 }
61 *buf++ = '\0';
62 }
## 9.11. List of registers
## 9.11.1. IO - User Bank
The User Bank IO registers start at a base address of 0x40028000 (defined as IO_BANK0_BASE in SDK).
RP2350 Datasheet
## 9.11. List of registers 604
Table 64## 9. List of
IO_BANK0 registers
Offset Name Info
0x000 GPIO0_STATUS
0x004 GPIO0_CTRL
0x008 GPIO1_STATUS
0x00c GPIO1_CTRL
0x010 GPIO2_STATUS
0x014 GPIO2_CTRL
0x018 GPIO3_STATUS
0x01c GPIO3_CTRL
0x020 GPIO4_STATUS
0x024 GPIO4_CTRL
0x028 GPIO5_STATUS
0x02c GPIO5_CTRL
0x030 GPIO6_STATUS
0x034 GPIO6_CTRL
0x038 GPIO7_STATUS
0x03c GPIO7_CTRL
0x040 GPIO8_STATUS
0x044 GPIO8_CTRL
0x048 GPIO9_STATUS
0x04c GPIO9_CTRL
0x050 GPIO10_STATUS
0x054 GPIO10_CTRL
0x058 GPIO11_STATUS
0x05c GPIO11_CTRL
0x060 GPIO12_STATUS
0x064 GPIO12_CTRL
0x068 GPIO13_STATUS
0x06c GPIO13_CTRL
0x070 GPIO14_STATUS
0x074 GPIO14_CTRL
0x078 GPIO15_STATUS
0x07c GPIO15_CTRL
0x080 GPIO16_STATUS
0x084 GPIO16_CTRL
0x088 GPIO17_STATUS
0x08c GPIO17_CTRL
RP2350 Datasheet
## 9.11. List of registers 605
Offset Name Info
0x090 GPIO18_STATUS
0x094 GPIO18_CTRL
0x098 GPIO19_STATUS
0x09c GPIO19_CTRL
0x0a0 GPIO20_STATUS
0x0a4 GPIO20_CTRL
0x0a8 GPIO21_STATUS
0x0ac GPIO21_CTRL
0x0b0 GPIO22_STATUS
0x0b4 GPIO22_CTRL
0x0b8 GPIO23_STATUS
0x0bc GPIO23_CTRL
0x0c0 GPIO24_STATUS
0x0c4 GPIO24_CTRL
0x0c8 GPIO25_STATUS
0x0cc GPIO25_CTRL
0x0d0 GPIO26_STATUS
0x0d4 GPIO26_CTRL
0x0d8 GPIO27_STATUS
0x0dc GPIO27_CTRL
0x0e0 GPIO28_STATUS
0x0e4 GPIO28_CTRL
0x0e8 GPIO29_STATUS
0x0ec GPIO29_CTRL
0x0f0 GPIO30_STATUS
0x0f4 GPIO30_CTRL
0x0f8 GPIO31_STATUS
0x0fc GPIO31_CTRL
0x100 GPIO32_STATUS
0x104 GPIO32_CTRL
0x108 GPIO33_STATUS
0x10c GPIO33_CTRL
0x110 GPIO34_STATUS
0x114 GPIO34_CTRL
0x118 GPIO35_STATUS
0x11c GPIO35_CTRL
RP2350 Datasheet
## 9.11. List of registers 606
Offset Name Info
0x120 GPIO36_STATUS
0x124 GPIO36_CTRL
0x128 GPIO37_STATUS
0x12c GPIO37_CTRL
0x130 GPIO38_STATUS
0x134 GPIO38_CTRL
0x138 GPIO39_STATUS
0x13c GPIO39_CTRL
0x140 GPIO40_STATUS
0x144 GPIO40_CTRL
0x148 GPIO41_STATUS
0x14c GPIO41_CTRL
0x150 GPIO42_STATUS
0x154 GPIO42_CTRL
0x158 GPIO43_STATUS
0x15c GPIO43_CTRL
0x160 GPIO44_STATUS
0x164 GPIO44_CTRL
0x168 GPIO45_STATUS
0x16c GPIO45_CTRL
0x170 GPIO46_STATUS
0x174 GPIO46_CTRL
0x178 GPIO47_STATUS
0x17c GPIO47_CTRL
0x200 IRQSUMMARY_PROC0_SECURE0
0x204 IRQSUMMARY_PROC0_SECURE1
0x208 IRQSUMMARY_PROC0_NONSECURE0
0x20c IRQSUMMARY_PROC0_NONSECURE1
0x210 IRQSUMMARY_PROC1_SECURE0
0x214 IRQSUMMARY_PROC1_SECURE1
0x218 IRQSUMMARY_PROC1_NONSECURE0
0x21c IRQSUMMARY_PROC1_NONSECURE1
0x220 IRQSUMMARY_COMA_WAKE_SECURE
0
0x224 IRQSUMMARY_COMA_WAKE_SECURE
1
RP2350 Datasheet
## 9.11. List of registers 607
Offset Name Info
0x228 IRQSUMMARY_COMA_WAKE_NONSE
CURE0
0x22c IRQSUMMARY_COMA_WAKE_NONSE
CURE1
0x230 INTR0 Raw Interrupts
0x234 INTR1 Raw Interrupts
0x238 INTR2 Raw Interrupts
0x23c INTR3 Raw Interrupts
0x240 INTR4 Raw Interrupts
0x244 INTR5 Raw Interrupts
0x248 PROC0_INTE0 Interrupt Enable for proc0
0x24c PROC0_INTE1 Interrupt Enable for proc0
0x250 PROC0_INTE2 Interrupt Enable for proc0
0x254 PROC0_INTE3 Interrupt Enable for proc0
0x258 PROC0_INTE4 Interrupt Enable for proc0
0x25c PROC0_INTE5 Interrupt Enable for proc0
0x260 PROC0_INTF0 Interrupt Force for proc0
0x264 PROC0_INTF1 Interrupt Force for proc0
0x268 PROC0_INTF2 Interrupt Force for proc0
0x26c PROC0_INTF3 Interrupt Force for proc0
0x270 PROC0_INTF4 Interrupt Force for proc0
0x274 PROC0_INTF5 Interrupt Force for proc0
0x278 PROC0_INTS0 Interrupt status after masking & forcing for proc0
0x27c PROC0_INTS1 Interrupt status after masking & forcing for proc0
0x280 PROC0_INTS2 Interrupt status after masking & forcing for proc0
0x284 PROC0_INTS3 Interrupt status after masking & forcing for proc0
0x288 PROC0_INTS4 Interrupt status after masking & forcing for proc0
0x28c PROC0_INTS5 Interrupt status after masking & forcing for proc0
0x290 PROC1_INTE0 Interrupt Enable for proc1
0x294 PROC1_INTE1 Interrupt Enable for proc1
0x298 PROC1_INTE2 Interrupt Enable for proc1
0x29c PROC1_INTE3 Interrupt Enable for proc1
0x2a0 PROC1_INTE4 Interrupt Enable for proc1
0x2a4 PROC1_INTE5 Interrupt Enable for proc1
0x2a8 PROC1_INTF0 Interrupt Force for proc1
0x2ac PROC1_INTF1 Interrupt Force for proc1
RP2350 Datasheet
## 9.11. List of registers 608
Offset Name Info
0x2b0 PROC1_INTF2 Interrupt Force for proc1
0x2b4 PROC1_INTF3 Interrupt Force for proc1
0x2b8 PROC1_INTF4 Interrupt Force for proc1
0x2bc PROC1_INTF5 Interrupt Force for proc1
0x2c0 PROC1_INTS0 Interrupt status after masking & forcing for proc1
0x2c4 PROC1_INTS1 Interrupt status after masking & forcing for proc1
0x2c8 PROC1_INTS2 Interrupt status after masking & forcing for proc1
0x2cc PROC1_INTS3 Interrupt status after masking & forcing for proc1
0x2d0 PROC1_INTS4 Interrupt status after masking & forcing for proc1
0x2d4 PROC1_INTS5 Interrupt status after masking & forcing for proc1
0x2d8 DORMANT_WAKE_INTE0 Interrupt Enable for dormant_wake
0x2dc DORMANT_WAKE_INTE1 Interrupt Enable for dormant_wake
0x2e0 DORMANT_WAKE_INTE2 Interrupt Enable for dormant_wake
0x2e4 DORMANT_WAKE_INTE3 Interrupt Enable for dormant_wake
0x2e8 DORMANT_WAKE_INTE4 Interrupt Enable for dormant_wake
0x2ec DORMANT_WAKE_INTE5 Interrupt Enable for dormant_wake
0x2f0 DORMANT_WAKE_INTF0 Interrupt Force for dormant_wake
0x2f4 DORMANT_WAKE_INTF1 Interrupt Force for dormant_wake
0x2f8 DORMANT_WAKE_INTF2 Interrupt Force for dormant_wake
0x2fc DORMANT_WAKE_INTF3 Interrupt Force for dormant_wake
0x300 DORMANT_WAKE_INTF4 Interrupt Force for dormant_wake
0x304 DORMANT_WAKE_INTF5 Interrupt Force for dormant_wake
0x308 DORMANT_WAKE_INTS0 Interrupt status after masking & forcing for dormant_wake
0x30c DORMANT_WAKE_INTS1 Interrupt status after masking & forcing for dormant_wake
0x310 DORMANT_WAKE_INTS2 Interrupt status after masking & forcing for dormant_wake
0x314 DORMANT_WAKE_INTS3 Interrupt status after masking & forcing for dormant_wake
0x318 DORMANT_WAKE_INTS4 Interrupt status after masking & forcing for dormant_wake
0x31c DORMANT_WAKE_INTS5 Interrupt status after masking & forcing for dormant_wake
IO_BANK0: GPIO0_STATUS Register
Offset: 0x000
Table 650.
GPIO0_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
RP2350 Datasheet
## 9.11. List of registers 609
Bits Description Type Reset
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO0_CTRL Register
Offset: 0x004
Table 651.
GPIO0_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
RP2350 Datasheet
## 9.11. List of registers 610
Bits Description Type Reset
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TCK
0x01 → SPI0_RX
0x02 → UART0_TX
0x03 → I2C0_SDA
0x04 → PWM_A_0
0x05 → SIO_0
0x06 → PIO0_0
0x07 → PIO1_0
0x08 → PIO2_0
0x09 → XIP_SS_N_1
0x0a → USB_MUXING_OVERCURR_DETECT
0x1f → NULL
IO_BANK0: GPIO1_STATUS Register
Offset: 0x008
Table 652.
GPIO1_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO1_CTRL Register
Offset: 0x00c
Table 653.
GPIO1_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
RP2350 Datasheet
## 9.11. List of registers 611
Bits Description Type Reset
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TMS
0x01 → SPI0_SS_N
0x02 → UART0_RX
0x03 → I2C0_SCL
0x04 → PWM_B_0
RP2350 Datasheet
## 9.11. List of registers 612
Bits Description Type Reset
0x05 → SIO_1
0x06 → PIO0_1
0x07 → PIO1_1
0x08 → PIO2_1
0x09 → CORESIGHT_TRACECLK
0x0a → USB_MUXING_VBUS_DETECT
0x1f → NULL
IO_BANK0: GPIO2_STATUS Register
Offset: 0x010
Table 654.
GPIO2_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO2_CTRL Register
Offset: 0x014
Table 655.
GPIO2_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
RP2350 Datasheet
## 9.11. List of registers 613
Bits Description Type Reset
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TDI
0x01 → SPI0_SCLK
0x02 → UART0_CTS
0x03 → I2C1_SDA
0x04 → PWM_A_1
0x05 → SIO_2
0x06 → PIO0_2
0x07 → PIO1_2
0x08 → PIO2_2
0x09 → CORESIGHT_TRACEDATA_0
0x0a → USB_MUXING_VBUS_EN
0x0b → UART0_TX
0x1f → NULL
IO_BANK0: GPIO3_STATUS Register
Offset: 0x018
RP2350 Datasheet
## 9.11. List of registers 614
Table 656.
GPIO3_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO3_CTRL Register
Offset: 0x01c
Table 657.
GPIO3_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0







# 12.7. USB
12.7.1. Overview
 NOTE
Prerequisite knowledge required
This section requires knowledge of the USB protocol. If you aren’t yet familiar with the USB protocol, we recommend
the archive of the very useful USB Made Simple website. For formal definitions of the terminology used in this
section, see the USB 2.0 Specification.
RP2350 contains a USB 2.0 controller that can operate as either:
• a Full Speed (FS) device (12 Mb/s)
• a host that can communicate with both Low Speed (LS) (1.5 Mb/s) and Full Speed devices, including multiple
downstream devices connected to a USB hub
There is an integrated USB 1.1 PHY which interfaces the USB controller with the DP and DM pins of the chip. You may use
this as 3.3 V GPIO when the USB controller is not in use.
RP2350 Datasheet
12.7. USB 1141
12.7.1.1. Features
The USB controller hardware handles the low level USB protocol. The programmer must configure the controller, provide
data buffers, and consume or provide data buffers in response to events on the bus. The controller interrupts the
processor when it needs attention. The USB controller has 4 kB of dual-port SRAM (DPSRAM) used for configuration
and data buffers.
12.7.1.1.1. Device Mode
In Device Mode, the USB controller has the following characteristics:
• USB 2.0-compatible Full Speed device (12 Mb/s)
• Supports up to 32 endpoints (Endpoints 0 → 15 in both in and out directions)
• Supports Control, Isochronous (ISO), Bulk, and Interrupt endpoint types
• Supports double buffering
• 3840 bytes of usable buffer space in DPSRAM. This is equivalent to 60 × 64-byte buffers
12.7.1.1.2. Host Mode
In Host Mode, the USB controller can:
• communicate with Full Speed (12 Mb/s) devices and Low Speed devices (1.5 Mb/s)
• communicate with multiple devices via a USB hub, including Low Speed devices connected to a Full Speed hub
• poll up to 15 interrupt endpoints in hardware (used by hubs to notify the host of connect/disconnect events, used
by mice to notify the host of movement, etc.)
12.7.1.1.3. USB DPRAM
The USB controller uses 4 kB of dual-port SRAM (DPSRAM) to exchange data and control information with the
controller. This is also referred to as dual-port RAM (DPRAM). One port is accessible from the system bus, clocked by
clk_sys. The other port is accessible from the controller, clocked by clk_usb. The DPRAM is mapped in the system
address space starting from 0x50100000, USBCTRL_DPRAM_BASE.
The USB DPRAM supports 32-bit, 16-bit and 8-bit reads and writes. Writes complete in one cycle. Reads complete in two
cycles.
You can store general user data in USB DPRAM space not required for USB controller operation. When the controller is
disabled, all 4 kB of DPRAM is available. Before accessing the DPRAM, you must take the USB controller out of reset.
Since the USB controller is in the peripheral address space, it is not accessible for processor instruction fetch.
Attempting to fetch instructions from USB DPRAM unconditionally returns a bus error response, no matter the
configuration of the processor SAU/MPU/PMP or the system ACCESSCTRL registers.
As peripheral addresses are marked Exempt in the IDAU (Section 10.2.2), the SAU configuration for this address range
is ignored. Accesses to USB DPRAM are controlled only by the processor MPU/PMP and the ACCESSCTRL USBCTRL
register.
12.7.2. Changes from RP2040
All changes from RP2040 are a superset of the RP2040 features. Existing software for the RP2040 USB controller will
continue to work with one exception: you must clear the MAIN_CTRL.PHY_ISO bit at startup and after power down
events. We recommend leaving the LINESTATE_TUNING register at its reset value. Software should not clear this
register.
RP2350 Datasheet
12.7. USB 1142
12.7.2.1. Errata fixes
RP2350 fixes all RP2040 USB errata. This includes fixes for the following RP2040B0 and B1 errata which are also fixed
by RP2040B2:
• RP2040-E2: USB device endpoint abort is not cleared
• RP2040-E5: USB device fails to exit RESET state on busy USB bus
For more information about RP2040B2, see the RP2040 datasheet.
RP2350 fixes the following RP2040B2 errata, which require software workarounds on RP2040B2:
• RP2040-E3: USB host: interrupt endpoint buffer done flag can be set with incorrect buffer select
• RP2040-E4: USB host writes to upper half of buffer status in single buffered mode
• RP2040-E15: USB Device controller will hang if certain bus errors occur during an IN transfer (see Section
12.7.2.2.4)
12.7.2.2. New features
12.7.2.2.1. General
• The USB PHY DP and DM can be used as regular GPIO pins. See the GPIO muxing Table 646 in Section 9.4..
• A MAIN_CTRL.PHY_ISO control isolates the PHY from the switched core power domain while the switched core
domain is powered down. The isolation control resets to 1, meaning the MAIN_CTRL.PHY_ISO bit needs to be
cleared before the PHY can be used. For more information on isolation, see Chapter 9.
• SIE_CTRL.PULLDOWN_EN defaults to a 1 to match the reset state of isolation latches in the USB PHY. Pulling the
DP and DM pins down by default saves power by preventing them from floating when unused.
• The USB_MUXING.TO_PHY bit defaults to a 1 to match the reset state of isolation latches.
• Added SM_STATE, which exposes the internal state of the controller’s modules.
12.7.2.2.2. Host
• You can now optionally stop a transaction if a NAK is received. This allows the USB host to stop a bulk transaction if
the device is not able to transfer data. Some devices using bulk endpoints, such as a UART, will return NAKs until a
character is received. Stopping the transaction in hardware rather than using software means the host can get a
NAK and guarantee no data has been dropped. RP2350 adds two register bits and an interrupt to support this:
◦
The NAK_POLL.STOP_EPX_ON_NAK control, which enables and disables the feature.
◦
The NAK_POLL.EPX_STOPPED_ON_NAK status bit, which also has an associated interrupt
INTS.EPX_STOPPED_ON_NAK.
• RP2350 increases inter-packet and turnaround timeouts to accommodate worst-case hub delays. This issue, only
seen with long chains of USB hubs, was never seen in practice. Timings in the host state machine have been
corrected to match USB spec. This fix is enabled by LINESTATE_TUNING.MULTI_HUB_FIX.
12.7.2.2.3. Device
• Added wake from suspend fix: Any bus activity (defined as K or SE0) should cause a wake from suspend, not just a
qualified period of resume signalling. This fix is enabled by default and can be disabled with
LINESTATE_TUNING.DEV_LS_WAKE_FIX (LS means line state in this instance, not low speed).
• Added DPSRAM double read feature to ensure data consistency. This avoids the need to set the AVAILABLE bit in the
buffer control register separate to the rest of the buffer information. This feature is enabled by default and
controlled by LINESTATE_TUNING.DEV_BUFF_CONTROL_DOUBLE_READ_FIX.
RP2350 Datasheet
12.7. USB 1143
• Added ability to stop DEVICE OUT FROM HOST when a short packet is received. For EP0 this is controlled by
SIE_CTRL.EP0_STOP_ON_SHORT_PACKET. This is done by stopping the transaction and then not toggling the
buffer if in double buffered mode. Also added short_packet interrupt to notify software that a short packet has been
received (INTS.RX_SHORT_PACKET)
12.7.2.2.4. Device error handling
• Added DEV_RX_ERR_QUIESCE feature: the device endpoint error count replicates the host’s internal Cerr count so
software can detect if the host has probably halted the endpoint after three consecutive errors. The various stages
of RX decode generate their own error signals that propagate to the top level. These error signals arrive at different
times, so two error interrupts generate for every failed transfer. Added an optional override for this behaviour by
forcing the device RX controller to idle after the first instance of an error during a transfer. This fix is enabled with
LINESTATE_TUNING.DEV_RX_ERR_QUIESCE.
• Added SIE_RX_CHATTER_SE0_FIX: the existing error recovery implementation waits for 8 FS idle bit-times before
signalling a framing error and returning to idle. This works OK for random bus errors, but when a hub terminates a
downstream packet, the hub forces a bit-stuff error followed by EOP. A valid token from the host may immediately
follow this, but the device controller may ignore it due to the enforced delay. Optionally waits for either a valid EOP
or 8 idle bit times before signalling a framing error. To enable the fix, use
LINESTATE_TUNING.SIE_RX_CHATTER_SE0_FIX.
• Fix RP2040-E15: the receive state machine doesn’t always handle cases where the bitstream deserialiser can abort
a transfer. If decoding terminates due to bitstuff errors during the middle phases of a packet, the device controller
can lock up. Unconditionally disables RX if the deserialiser has flagged a bitstuff error and subsequently signalled
framing error after linestate returns to idle. To enable this fix, use LINESTATE_TUNING.SIE_RX_BITSTUFF_FIX.
• Device state machine watchdog: added a watchdog so that if the device state machine gets stuck for a certain
amount of time it can be forced to idle. This is to handle any other error cases not anticipated by the above fixes.
To enable the watchdog, use DEV_SM_WATCHDOG.
12.7.3. Architecture
12.7.3.1. Clock speed
This controller requires clk_usb to be running at 48MHz.
 NOTE
clk_sys must also be running at > 48MHz. See RP2350-E12.
12.7.3.2. Overview
RP2350 Datasheet
12.7. USB 1144
Figure 124. A
simplified overview of
the USB controller
architecture.
The USB controller is an area-efficient design that muxes a device controller or host controller onto a common set of
components. Each component is detailed below.
12.7.3.3. USB PHY
The USB PHY provides the electrical interface between the USB DP and DM pins and the digital logic of the controller. The
DP and DM pins are a differential pair, meaning the values are always the inverse of each other, except to encode a
specific line state (e.g. SE0). The USB PHY drives the DP and DM pins to transmit data and performs a differential receive
of any incoming data. The USB PHY provides both single-ended and differential receive data to the line state detection
module.
The USB PHY has built in pull-up and pull-down resistors. When the controller acts as a Full Speed device, the DP pin is
pulled up to indicate to the host that a Full Speed device has been connected. In host mode, a weak pull-down is applied
to DP and DM so that the lines are pulled to a logical zero until the device pulls up DP for Full Speed or DM for Low Speed.
12.7.3.4. Line state detection
The USB 2.0 Specification defines several line states (Bus Reset, Connected, Suspend, Resume, Data 1, Data 0, etc.) that
need to be detected. The line state detection module has several state machines to detect these states and signal
events to the other hardware components. There is no shared clock signal in USB, so the RX data must be sampled by
an internal clock. The maximum data rate of USB Full Speed is 12 Mb/s. The RX data is sampled at 48MHz, giving 4
clock cycles to capture and filter the bus state. The line state detection module distributes the filtered RX data to the
Serial RX Engine.
12.7.3.5. Serial RX engine
The serial receive (RX) engine decodes receive data captured by the line state detection module. It produces the
following information:
• The PID of the incoming data packet
• The device address for the incoming data
• The device endpoint for the incoming data
• Data bytes
The serial receive engine also detects errors in RX data by performing a CRC check on the incoming data. Any errors are
signalled to the other hardware blocks and can raise an interrupt.
RP2350 Datasheet
12.7. USB 1145
 NOTE
If you disconnect the USB cable during packet transfer in either host or device mode, the hardware will raise errors.
Software must account for this scenario if you enable error interrupts.
12.7.3.6. Serial TX engine
The serial transmit (TX) engine is a mirror of the serial receive engine. It is connected to the currently active controller
(either device or host). It creates TOKEN and DATA packets, calculates the CRC, and transmits them on the bus.
12.7.3.7. DPSRAM
The USB controller uses 4 kB (4096 bytes) of Dual Port SRAM (DPSRAM) to store control registers and data buffers. The
DPSRAM is accessible as a 32-bit wide memory at address 0 of the USB controller (0x50100000).
The DPSRAM has the following characteristics, which differ from most registers on RP2350:
• Supports 8-bit, 16-bit, and 32-bit accesses (typically, RP2350 registers only support 32-bit accesses)
• Does not support set/clear aliases. (typically, RP2350 registers support these)
Data Buffers are typically 64 bytes long, as this is the maximum normal packet size for most Full Speed packets.
Isochronous endpoints support a maximum buffer size of 1023 bytes. For other packet types, the maximum size is 64
bytes per buffer.
12.7.3.7.1. Concurrent access
The DPSRAM in the USB controller is asynchronous. The dual port part of the name indicates that both the processor
and the USB controller have ports to read and write, and these two ports are in different clock domains. As a result, the
processor and USB controller can access the same memory address at the same time. One could write and one could
read simultaneously. This could result in inconsistent data reads. You can avoid this scenario by following the rules
outlined in this section.
The AVAILABLE bit in the buffer control register indicates who has ownership of a buffer. Set this bit to 1 from the
processor to give the controller ownership of the buffer. When it has finished using the buffer, the controller sets the bit
back to 0. Set the AVAILABLE bit separately from the rest of the data in the buffer control register so that the rest of the
data in the buffer control register is accurate when the AVAILABLE bit is set.
This is necessary because the processor clock clk_sys can run several times faster than the clk_usb clock. Therefore
clk_sys can update the data during a USB controller read on a slower clock. The correct process is:
1. Write buffer information (length, etc.) to the buffer control register.
2. nop for some clk_sys cycles to ensure that at least one clk_usb cycle passes. Consider a scenario where clk_sys runs
at 125MHz and clk_usb runs at 48MHz. Because , you should issue 3 nop instructions between the writes
to guarantee that at least one clk_usb cycle has passed.
3. Set the AVAILABLE bit.
If clk_sys and clk_usb run at the same frequency, then it is not necessary to set the AVAILABLE bit separately.
RP2350 Datasheet
12.7. USB 1146
 NOTE
When the USB controller writes the status back to the DPSRAM, it does a 16-bit write to the lower 2 bytes for buffer 0
and the upper 2 bytes for buffer 1. When using double-buffered mode, always treat the buffer control register as two
16-bit registers when updating it in software.
12.7.3.7.2. Layout
Addresses 0x0 → 0xff are used for control registers containing configuration data. The remaining space, addresses
0x100 → 0xfff (3840 bytes) can be used for data buffers. The controller has control registers that start at address
0x10000.
The memory layout depends on the USB controller mode:
• In Device mode, the host can access multiple endpoints, so each endpoint must have endpoint control and buffer
control registers.
• In Host mode, the host software running on the processor decides which endpoints and devices to access. This
only requires one set of endpoint control and buffer control registers. As well as software-driven transfers, the host
controller can poll up to 15 interrupt endpoints and has a register for each of these interrupt endpoints.
Table 1192. DPSRAM
layout
Offset Device Function Host Function
0x0 Setup packet (8 bytes)
0x8 EP1 in control Interrupt endpoint control 1
0xc EP1 out control Spare
0x10 EP2 in control Interrupt endpoint control 2
0x14 EP2 out control Spare
0x18 EP3 in control Interrupt endpoint control 3
0x1c EP3 out control Spare
0x20 EP4 in control Interrupt endpoint control 4
0x24 EP4 out control Spare
0x28 EP5 in control Interrupt endpoint control 5
0x2c EP5 out control Spare
0x30 EP6 in control Interrupt endpoint control 6
0x34 EP6 out control Spare
0x38 EP7 in control Interrupt endpoint control 7
0x3c EP7 out control Spare
0x40 EP8 in control Interrupt endpoint control 8
0x44 EP8 out control Spare
0x48 EP9 in control Interrupt endpoint control 9
0x4c EP9 out control Spare
0x50 EP10 in control Interrupt endpoint control 10
0x54 EP10 out control Spare
0x58 EP11 in control Interrupt endpoint control 11
RP2350 Datasheet
12.7. USB 1147
Offset Device Function Host Function
0x5c EP11 out control Spare
0x60 EP12 in control Interrupt endpoint control 12
0x64 EP12 out control Spare
0x68 EP13 in control Interrupt endpoint control 13
0x6c EP13 out control Spare
0x70 EP14 in control Interrupt endpoint control 14
0x74 EP14 out control Spare
0x78 EP15 in control Interrupt endpoint control 15
0x7c EP15 out control Spare
0x80 EP0 in buffer control EPx buffer control
0x84 EP0 out buffer control Spare
0x88 EP1 in buffer control Interrupt endpoint buffer control 1
0x8c EP1 out buffer control Spare
0x90 EP2 in buffer control Interrupt endpoint buffer control 2
0x94 EP2 out buffer control Spare
0x98 EP3 in buffer control Interrupt endpoint buffer control 3
0x9c EP3 out buffer control Spare
0xa0 EP4 in buffer control Interrupt endpoint buffer control 4
0xa4 EP4 out buffer control Spare
0xa8 EP5 in buffer control Interrupt endpoint buffer control 5
0xac EP5 out buffer control Spare
0xb0 EP6 in buffer control Interrupt endpoint buffer control 6
0xb4 EP6 out buffer control Spare
0xb8 EP7 in buffer control Interrupt endpoint buffer control 7
0xbc EP7 out buffer control Spare
0xc0 EP8 in buffer control Interrupt endpoint buffer control 8
0xc4 EP8 out buffer control Spare
0xc8 EP9 in buffer control Interrupt endpoint buffer control 9
0xcc EP9 out buffer control Spare
0xd0 EP10 in buffer control Interrupt endpoint buffer control 10
0xd4 EP10 out buffer control Spare
0xd8 EP11 in buffer control Interrupt endpoint buffer control 11
0xdc EP11 out buffer control Spare
0xe0 EP12 in buffer control Interrupt endpoint buffer control 12
0xe4 EP12 out buffer control Spare
0xe8 EP13 in buffer control Interrupt endpoint buffer control 13
RP2350 Datasheet
12.7. USB 1148
Offset Device Function Host Function
0xec EP13 out buffer control Spare
0xf0 EP14 in buffer control Interrupt endpoint buffer control 14
0xf4 EP14 out buffer control Spare
0xf8 EP15 in buffer control Interrupt endpoint buffer control 15
0xfc EP15 out buffer control Spare
0x100 EP0 buffer 0 (shared between in and
out)
EPx control
0x140 Optional EP0 buffer 1 Spare
0x180 Data buffers
12.7.3.7.3. Endpoint control register
The endpoint control register is used to configure an endpoint. It defines:
• The endpoint type
• The base address of the endpoint’s data buffer (or data buffers if double-buffered)
• Which endpoint events trigger the controller interrupt output
A device must support Endpoint 0 so that it can reply to SETUP packets and be enumerated. As a result, there is no
endpoint control register for EP0. Its buffers begin at 0x100. All other endpoints can have either single or dual buffers and
are mapped at the base address programmed. As EP0 has no endpoint control register, the interrupt enable controls for
EP0 come from SIE_CTRL.
Table 1193. Endpoint
control register layout
Bit(s) Device Function Host Function
31 Endpoint enable
30 Single buffered (64 bytes) = 0, Double buffered (64 bytes × 2) = 1
29 Enable interrupt for every transferred buffer
28 Enable interrupt for every 2 transferred buffers (valid for double-buffered only)
27:26 Endpoint Type: Control = 0, Isochronous = 1, Bulk = 2, Interrupt = 3
25:18 N/A The interval the host controller should poll this
endpoint. Only applicable for interrupt
endpoints. Specified in ms - 1. For example: a
value of 9 would poll the endpoint every 10ms.
17 Interrupt on STALL
16 Interrupt on NAK
15:6 Address base offset in DPSRAM of data buffer(s)
 NOTE
The data buffer base address must be 64-byte aligned, since bits 0 through 5 are ignored.
12.7.3.7.4. Buffer control register
The buffer control register contains information about the state of the data buffers for that endpoint. It is shared
between the processor and the controller. If the endpoint is configured to be single-buffered, only the first half (bits 0
through 15) of the buffer are used.
If double buffering, the buffer select starts at buffer 0. From then on, the buffer select flips between buffer 0 and 1
RP2350 Datasheet
12.7. USB 1149
unless the reset buffer select bit is set (which resets the buffer select to buffer 0). The value of the buffer select is
internal to the controller and not accessible by the processor.
For host interrupt and isochronous packets on EPx, the buffer full bit will be set on completion even if the transfer was
unsuccessful. To determine the error, read the error bits in the SIE_STATUS register.
Table 1194. Buffer
control register layout
Bit(s) Function
31 Buffer 1 full. Should be set to 1 by the processor for an IN transaction and 0 for an OUT
transaction. The controller sets this to 1 for an OUT transaction because it has filled the buffer.
The controller sets it to 0 for an IN transaction because it has emptied the buffer. Only valid
when double buffering.
30 Last buffer of transfer for buffer 1. Only valid when double buffering.
29 Data PID for buffer 1 - DATA0 = 0, DATA1 = 1. Only valid when double buffering.
27:28 Double buffer offset for isochronous mode (0 = 128, 1 = 256, 2 = 512, 3 = 1024).
26 Buffer 1 available. Whether the buffer can be used by the controller for a transfer. The
processor sets this to 1 when the buffer is configured. The controller sets this to 0 after it has
sent the data to the host for an IN transaction, or filled the buffer with data from the host for an
OUT transaction. Only valid when double buffering.
25:16 Buffer 1 transfer length. Only valid when double buffering.
15 Buffer 0 full. Should be set to 1 by the processor for an IN transaction and 0 for an OUT
transaction. The controller sets this to 1 for an OUT transaction because it has filled the buffer.
The controller sets it to 0 for an IN transaction because it has emptied the buffer.
14 Last buffer of transfer for buffer 0.
13 Data PID for buffer 0 - DATA0 = 0, DATA1 = 1.
12 Reset buffer select to buffer 0 - cleared at end of transfer. For device only.
11 Send STALL for device, STALL received for host.
10 Buffer 0 available. Indicates whether the buffer can be used by the controller for a transfer.
The processor sets this to 1 when the buffer is configured. The controller sets this to 0 after it
has sent the data to the host for an IN transaction or filled the buffer with data from the host
for an OUT transaction.
9:0 Buffer 0 transfer length.
 WARNING
If you run clk_sys and clk_usb at different speeds, set the available and stall bits after the other data in the buffer
control register. Otherwise, the controller may initiate a transaction with data from a previous packet. The controller
could see the available bit set, but get the data PID or length from the previous packet.
12.7.3.8. Device controller
This section details how the device controller operates when it receives various packet types from the host.
12.7.3.8.1. SETUP
The device controller MUST always accept a SETUP packet from the host. DPSRAM dedicates its first 8 bytes to the setup
packet.
The USB 2.0 Specification states that receiving a setup packet also clears any stall bits on EP0. For this reason, the stall
RP2350 Datasheet
12.7. USB 1150
bits for EP0 are gated with two bits in the EP_STALL_ARM register. These bits are cleared when a setup packet is
received. This means that to send a stall on EP0, you must set both the stall bit in the buffer control register and the
appropriate bit in EP_STALL_ARM.
Barring any errors, the setup packet will be put into the setup packet buffer at DPSRAM offset 0x0. The device controller
will then reply with an ACK.
Finally, SIE_STATUS.SETUP_REC is set to indicate that a setup packet has been received. This will trigger an interrupt if
the programmer has enabled the SETUP_REC interrupt (see INTE).
12.7.3.8.2. IN
From the device’s point of view, an IN transfer means transferring data into the host. When an IN token is received from
the host, the request is handled as follows:
TOKEN phase:
1. If STALL is set in the buffer control register (and if EP0, the appropriate EP_STALL_ARM bit is set), send a STALL
response and go to idle.
2. If AVAILABLE and FULL bits are set in buffer control, go to the DATA phase.
3. If this is an isochronous endpoint, go to idle.
◦
Otherwise, send NAK and go to the DATA phase.
DATA phase:
1. Send data.
2. If this is an isochronous endpoint, go to idle.
◦
Otherwise, go to the ACK phase.
ACK phase:
1. Wait for ACK packet from host.
2. If there is a timeout, raise a timeout error.
3. If ACK is received, the packet is done, so go to STATUS phase.
STATUS phase:
1. If this was the last buffer in the transfer (i.e. if the LAST_BUFFER bit in the buffer control register was set), set
SIE_STATUS.TRANS_COMPLETE.
2. If the endpoint is double buffered, flip the buffer select to the other buffer.
3. Set a bit in BUFF_STATUS to indicate the buffer is done. When handling this event, the programmer should read
BUFF_CPU_SHOULD_HANDLE to see if it is buffer 0 or buffer 1 that is finished. If the endpoint is double-buffered,
both buffers could be done. The cleared BUFF_STATUS bit will be set again, and BUFF_CPU_SHOULD_HANDLE will
change in this instance.
4. Update status in the appropriate half of the buffer control register: length, pid, and last_buff are set. Everything else
is written to zero.
If the host receives a NAK, the host will retry again later.
12.7.3.8.3. OUT
When an OUT token is received from the host, the request is handled as follows:
TOKEN phase:
1. If this is not an Isochronous endpoint and the data PID does not match the buffer control register, raise
RP2350 Datasheet
12.7. USB 1151
SIE_STATUS.DATA_SEQ_ERROR (isochronous data is always sent with a DATA0 pid).
2. If the AVAILABLE bit is set and the FULL bit is clear, go to the DATA phase, unless the STALL bit is set in which case the
device controller will reply with a STALL.
DATA phase:
1. Store received data in buffer. If this is an isochronous endpoint, go to the STATUS phase. Otherwise, go to the ACK
phase.
ACK phase:
1. Send ACK. Go to the STATUS phase.
STATUS phase:
See IN STATUS phase: [usb-device-in-status-phase]. There is one difference: the FULL bit is set in the buffer control register
to indicate that data has been received. In the IN phase, the FULL bit is cleared to indicate that data has been sent.
12.7.3.8.4. Suspend and resume
The USB device controller supports suspend, resume, and device-initiated remote resume (triggered with
SIE_CTRL.RESUME). There is an interrupt / status bit in SIE_STATUS. It is not necessary to enable the suspend and
resume interrupts, since suspend and resume are irrelevant to most devices.
The device goes into suspend when it does not see any start of frame packets (transmitted every 1ms) from the host.
 NOTE
If you enable the suspend interrupt, it is likely you will see a suspend interrupt when the device first connects, but the
bus is idle. The bus can be idle for a few milliseconds before the host begins sending start of frame packets. If you
do not have a VBUS detect circuit connected, you will also see a suspend interrupt when the device disconnects.
Without VBUS detection, it is impossible to tell the difference between being disconnected and suspended.
12.7.3.9. Host controller
The host controller design is similar to the device controller. The host starts all transactions, so the host always deals
with transactions it has started. For this reason, there is only one set of endpoint control and endpoint buffer control
registers. The host controller also contains additional hardware to poll interrupt endpoints in the background when there
are no software controlled transactions taking place.
The host needs to send keep-alive packets to the device every 1ms to keep the device from suspending. Full Speed
mode uses a SOF (start of frame) packet. Low Speed mode uses an EOP (end of packet) instead. Set
SIE_CTRL.KEEP_ALIVE_EN and SIE_CTRL.SOF_EN to enable these packets.
Several bits in SIE_CTRL are used to begin a host transaction:
• SEND_SETUP - Send a setup packet. Typically used with RECEIVE_TRANS, so the setup packet will be sent followed by the
additional data transaction expected from the device.
• SEND_TRANS - This transfer is OUT from the host.
• RECEIVE_TRANS - This transfer is IN to the host.
• START_TRANS - Start the transfer (non-latching).
• STOP_TRANS - Stop the current transfer (non-latching).
• PREAMBLE_ENABLE - Used to send a packet to a Low Speed device on a Full Speed hub. Sends a PRE token packet
before every packet the host sends (i.e. PRE, TOKEN, PRE, DATA, pre, ACK).
• SOF_SYNC - Used to delay the transaction until after the next SOF. Useful for interrupt and isochronous endpoints. The
host controller prevents a transaction of 64 bytes from clashing with the SOF packets. For longer isochronous
RP2350 Datasheet
12.7. USB 1152
packets, software is responsible for preventing collisions. To prevent collisions in software, use SOF_SYNC and limit
the number of packets sent in one frame. If a transaction is set up with multiple packets, SOF_SYNC only applies to
the first packet.
The START_TRANS bit is synchronised separately from other control bits in the SIE_CTRL register because the processor
clock clk_sys can be asynchronous to the clk_usb clock. Always set the START_TRANS bit separately from the rest of the
data in the SIE_CTRL register. Always ensure that at least two clk_usb cycles pass between writing to START_TRANS and other
bits in SIE_CTRL. This ensures that the register contents are stable when the controller is prompted to start a transfer.
Consider a scenario where clk_sys runs at 125MHz and clk_usb runs at 48MHz. Because , you should
issue 6 nop instructions between the writes to guarantee that at least two clk_usb cycles have passed.
12.7.3.9.1. SETUP
The SETUP packet sent from the host always comes from the dedicated 8 bytes of space at offset 0x0 of the DPSRAM.
Like the device controller, there are no control registers associated with the setup packet. The parameters are hardcoded and loaded into the hardware when you write to START_TRANS with the SEND_SETUP bit set. Once the setup packet has
been sent, the host state machine waits for an ACK from the device. If there is a timeout, an RX_TIMEOUT error will be raised.
If the SEND_TRANS bit is set, the host state machine will move to the OUT phase. Typically, the SEND_SETUP packet is used with
the RECEIVE_TRANS bit, so the controller moves to the IN phase after sending a setup packet.
12.7.3.9.2. IN
An IN transfer is triggered with the RECEIVE_TRANS bit set when the START_TRANS bit is set. If the SEND_SETUP bit was set, this
may be preceded by a SETUP packet.
CONTROL phase:
1. Read the EPx control register located at 0x80 to get the following endpoint information:
◦
Is it double buffered?
◦ What interrupts are enabled?
◦
Base address of the data buffer (data buffers if in double-buffered mode)
◦ What is the endpoint type?
2. Read the EPx buffer control register at 0x100 to get endpoint buffer information, such as transfer length and data
PID.
3. Set the AVAILABLE bit (the host state machine checks for it).
4. Clear the FULL bit.
TOKEN phase:
1. Send the IN token packet to the device. The target device address and endpoint come from the ADDR_ENDP
register.
DATA phase:
1. Receive the first data packet from the device.
2. Raise RX timeout error if the device doesn’t reply.
3. If this is not an Isochronous endpoint and the data PID does not match the buffer control register, raise
SIE_STATUS.DATA_SEQ_ERROR (isochronous data is always sent with a DATA0 pid).
ACK phase:
1. Send ACK to device.
STATUS phase:
RP2350 Datasheet
12.7. USB 1153
1. Set the BUFF_STATUS bit and update the buffer control register.
2. Set FULL, DATA_PID, WR_LEN, and LAST_BUFF if applicable.
3. If this is the last buffer in the transfer, set TRANS_COMPLETE.
CONTROL phase (continued):
The host state machine performs IN transactions until LAST_BUFF is seen in the buffer_control register.
If the host is in double buffered mode, the host controller toggles between the BUF0 and BUF1 sections of the buffer
control register.
Otherwise, the controller reads the buffer control register for buffer 0, then waits for FULL to be clear and AVAILABLE to be
set before starting the next IN transaction, waiting in the CONTROL phase.
If the host receives a zero length packet, the device has no more data. The host state machine stops listening for more
data regardless of if the LAST_BUFF flag was set or not. To detect this from host software, check BUFF_DONE for a data
length of 0 in the buffer control register.
12.7.3.9.3. OUT
An OUT transfer is triggered with the SEND_TRANS bit set when the START_TRANS bit is set. This may be preceded by a SETUP
packet if the SEND_SETUP bit was set.
CONTROL phase:
1. Read the EPx control register to get endpoint information (same as Section 12.7.3.9.2).
2. Read the EPx buffer control register to get the transfer length and data PID. AVAILABLE and FULL must be set before
the transfer can start.
TOKEN phase
1. Send an OUT packet to the device. The target device address and endpoint come from the ADDR_ENDP register.
DATA phase:
1. Send the first data packet to the device. If the endpoint type is isochronous, there is no ACK phase, so the host
controller goes straight to status phase. If ACK is received, go to status phase. Otherwise:
◦
If the host receives no reply, raise SIE_STATUS.RX_TIMEOUT.
◦
If the host receives NAK, raise SIE_STATUS.NAK_REC and send the data packet again.
◦
If the host receives STALL, raise SIE_STATUS.STALL_REC and go to idle.
STATUS phase:
1. Set the BUFF_STATUS bit and update the buffer control register. FULL will be set to 0. TRANS_COMPLETE will be set if
this is the last buffer in the transfer.
CONTROL phase (continued):
1. If this isn’t the last buffer in the transfer, wait for FULL and AVAILABLE to be set in the EPx buffer control register again.
12.7.3.9.4. Interrupt endpoints
The host controller can poll interrupt endpoints on a maximum of 15 endpoints. To enable interrupt endpoints, the
programmer must:
• Pick the next free interrupt endpoint slot on the host controller (starting at 1, to a maximum of 15).
• Program the appropriate endpoint control register and buffer control register like you would with a normal IN or OUT
transfer. Because interrupt endpoints are single-buffered, the BUF1 part of the buffer control register is invalid.
• Set the address and endpoint of the device in the appropriate ADDR_ENDP register (ADDR_ENDP1 to ADDR_ENDP15).
RP2350 Datasheet
12.7. USB 1154
If the device is Low Speed but attached to a Full Speed hub, the preamble bit should be set. The endpoint direction
bit should also be set.
• Set the corresponding interrupt endpoint active bit (one of bits 1 through 15) in INT_EP_CTRL.
Typically, interrupt endpoints use an IN transfer. The host might poll a USB hub to see if the state of any of its ports have
changed. If there is no change, the hub replies with a NAK to the controller, and nothing happens. Similarly, a mouse
replies with a NAK unless the mouse has been moved since the last time the interrupt endpoint was polled.
Interrupt endpoints are polled by the controller once a SOF packet has been sent by the host controller.
The controller loops from 1 to 15 and attempts to poll any interrupt endpoint with the EP_ACTIVE bit set to 1 in
INT_EP_CTRL. The controller will then read the endpoint control register and the buffer control register to see if there is
an available buffer (i.e. FULL + AVAILABLE if an OUT transfer and NOT FULL + AVAILABLE for an IN transfer). If not, the controller
will move onto the next interrupt endpoint slot.
If there is an available buffer, the transfer is dealt with the same as a normal IN or OUT transfer and the BUFF_DONE flag in
BUFF_STATUS will be set when the interrupt endpoint has a valid buffer.
12.7.3.10. VBUS control
The USB controller can be connected to GPIO pins (see Chapter 9) for the following VBUS controls:
• VBUS enable, used to enable VBUS in host mode. Set in SIE_CTRL.
• VBUS detect, used to detect that VBUS is present in device mode. Set via a bit in SIE_STATUS. Can also raise a
VBUS_DETECT interrupt enabled in INTE.
• VBUS overcurrent, used to detect an overcurrent event. Applicable to both device and host. VBUS overcurrent is a
bit in SIE_STATUS.
It is not necessary to connect up any of these pins to GPIO. The host can permanently supply VBUS and detect a device
being connected when either the DP or DM pin is pulled high. VBUS detect can be forced in USB_PWR.
12.7.4. Programmer’s model
12.7.4.1. TinyUSB
The RP2350 TinyUSB port is the reference implementation for this USB controller. This port can be found in the
following files of the pico-sdk GitHub repository:
dcd_rp2040.c
hcd_rp2040.c
rp2040_usb.h
12.7.4.2. Standalone device example
A standalone USB device example, dev_lowlevel, makes it easier to understand how to interact with the USB controller
without needing to understand the TinyUSB abstractions. In addition to endpoint 0, the standalone device has two bulk
endpoints: EP1 OUT and EP2 IN. The device is designed to send whatever data it receives on EP1 to EP2. The example comes
with a small Python script that writes "Hello World" into EP1 and checks that it is correctly received on EP2.
The code included in this section explains setting up the USB device controller to receive. It also shows how software
responds to a setup packet received from the host.
RP2350 Datasheet
12.7. USB 1155
Figure 125. USB
analyser trace of the
dev_lowlevel USB
device example. The
control transfers are
the device
enumeration. The first
bulk OUT (out from the
host) transfer,
highlighted in blue, is
the host sending
"Hello World" to the
device. The second
bulk transfer IN (in to
the host), is the device
returning "Hello World"
to the host.
12.7.4.2.1. Device controller initialisation
The following code initialises the USB device:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 183 - 217
183 void usb_device_init() {
184 // Reset usb controller
185 reset_unreset_block_num_wait_blocking(RESET_USBCTRL);
186
187 // Clear any previous state in dpram just in case
188 memset(usb_dpram, 0, sizeof(*usb_dpram)); ①
189
190 // Enable USB interrupt at processor
191 irq_set_enabled(USBCTRL_IRQ, true);
192
193 // Mux the controller to the onboard usb phy
194 usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
195
196 // Force VBUS detect so the device thinks it is plugged into a host
197 usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
198
199 // Enable the USB controller in device mode.
200 usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;
201
202 // Enable an interrupt per EP0 transaction
203 usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS; ②
204
205 // Enable interrupts for when a buffer is done, when the bus is reset,
206 // and when a setup packet is received
207 usb_hw->inte = USB_INTS_BUFF_STATUS_BITS |
208 USB_INTS_BUS_RESET_BITS |
209 USB_INTS_SETUP_REQ_BITS;
210
211 // Set up endpoints (endpoint control registers)
212 // described by device configuration
213 usb_setup_endpoints();
214
215 // Present full speed device by enabling pull up on DP
RP2350 Datasheet
12.7. USB 1156
216 usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
217 }
12.7.4.2.2. Configuring the endpoint control registers for EP1 and EP2
The function usb_configure_endpoints loops through each endpoint defined in the device configuration (including EP0 in
and EP0 out, which don’t have an endpoint control register defined) and calls the usb_configure_endpoint function. This
sets up the endpoint control register for that endpoint:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 149 - 164
149 void usb_setup_endpoint(const struct usb_endpoint_configuration *ep) {
150 printf("Set up endpoint 0x%x with buffer address 0x%p\n", ep->descriptor-
  >bEndpointAddress, ep->data_buffer);
151
152 // EP0 doesn't have one so return if that is the case
153 if (!ep->endpoint_control) {
154 return;
155 }
156
157 // Get the data buffer as an offset of the USB controller's DPRAM
158 uint32_t dpram_offset = usb_buffer_offset(ep->data_buffer);
159 uint32_t reg = EP_CTRL_ENABLE_BITS
160 | EP_CTRL_INTERRUPT_PER_BUFFER
161 | (ep->descriptor->bmAttributes << EP_CTRL_BUFFER_TYPE_LSB)
162 | dpram_offset;
163 *ep->endpoint_control = reg;
164 }
12.7.4.2.3. Receiving a setup packet
An interrupt is raised when a setup packet is received, so the interrupt handler must handle this event:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 494 - 504
494 void isr_usbctrl(void) {
495 // USB interrupt handler
496 uint32_t status = usb_hw->ints;
497 uint32_t handled = 0;
498
499 // Setup packet received
500 if (status & USB_INTS_SETUP_REQ_BITS) {
501 handled |= USB_INTS_SETUP_REQ_BITS;
502 usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
503 usb_handle_setup_packet();
504 }
The controller writes the SETUP packet to the first 8 bytes of the DPSRAM, so the setup packet handler casts that area of
memory to struct usb_setup_packet *:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 383 - 427
383 void usb_handle_setup_packet(void) {
384 volatile struct usb_setup_packet *pkt = (volatile struct usb_setup_packet *) &usb_dpram
  ->setup_packet;
385 uint8_t req_direction = pkt->bmRequestType;
RP2350 Datasheet
12.7. USB 1157
386 uint8_t req = pkt->bRequest;
387
388 // Reset PID to 1 for EP0 IN
389 usb_get_endpoint_configuration(EP0_IN_ADDR)->next_pid = 1u;
390
391 if (req_direction == USB_DIR_OUT) {
392 if (req == USB_REQUEST_SET_ADDRESS) {
393 usb_set_device_address(pkt);
394 } else if (req == USB_REQUEST_SET_CONFIGURATION) {
395 usb_set_device_configuration(pkt);
396 } else {
397 usb_acknowledge_out_request();
398 printf("Other OUT request (0x%x)\r\n", pkt->bRequest);
399 }
400 } else if (req_direction == USB_DIR_IN) {
401 if (req == USB_REQUEST_GET_DESCRIPTOR) {
402 uint16_t descriptor_type = pkt->wValue >> 8;
403
404 switch (descriptor_type) {
405 case USB_DT_DEVICE:
406 usb_handle_device_descriptor(pkt);
407 printf("GET DEVICE DESCRIPTOR\r\n");
408 break;
409
410 case USB_DT_CONFIG:
411 usb_handle_config_descriptor(pkt);
412 printf("GET CONFIG DESCRIPTOR\r\n");
413 break;
414
415 case USB_DT_STRING:
416 usb_handle_string_descriptor(pkt);
417 printf("GET STRING DESCRIPTOR\r\n");
418 break;
419
420 default:
421 printf("Unhandled GET_DESCRIPTOR type 0x%x\r\n", descriptor_type);
422 }
423 } else {
424 printf("Other IN request (0x%x)\r\n", pkt->bRequest);
425 }
426 }
427 }
12.7.4.2.4. Replying to a setup packet on EP0 IN
The host first requests the device descriptor. The following code handles that setup request:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 266 - 273
266 void usb_handle_device_descriptor(volatile struct usb_setup_packet *pkt) {
267 const struct usb_device_descriptor *d = dev_config.device_descriptor;
268 // EP0 in
269 struct usb_endpoint_configuration *ep = usb_get_endpoint_configuration(EP0_IN_ADDR);
270 // Always respond with pid 1
271 ep->next_pid = 1;
272 usb_start_transfer(ep, (uint8_t *) d, MIN(sizeof(struct usb_device_descriptor), pkt-
  >wLength));
273 }
The usb_start_transfer function copies data to be sent into the appropriate hardware buffer and configures the buffer
RP2350 Datasheet
12.7. USB 1158
control register. Once the buffer control register has been written to, the device controller responds to the host with the
data. Before this point, the device replies with a NAK:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c Lines 238 - 260
238 void usb_start_transfer(struct usb_endpoint_configuration *ep, uint8_t *buf, uint16_t len) {
239 // We are asserting that the length is <= 64 bytes for simplicity of the example.
240 // For multi packet transfers see the tinyusb port.
241 assert(len <= 64);
242
243 printf("Start transfer of len %d on ep addr 0x%x\n", len, ep->descriptor-
  >bEndpointAddress);
244
245 // Prepare buffer control register value
246 uint32_t val = len | USB_BUF_CTRL_AVAIL;
247
248 if (ep_is_tx(ep)) {
249 // Need to copy the data from the user buffer to the usb memory
250 memcpy((void *) ep->data_buffer, (void *) buf, len);
251 // Mark as full
252 val |= USB_BUF_CTRL_FULL;
253 }
254
255 // Set pid and flip for next transfer
256 val |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
257 ep->next_pid ^= 1u;
258
259 *ep->buffer_control = val;
260 }
12.7.5. List of registers
The USB registers start at a base address of 0x50110000 (defined as USBCTRL_REGS_BASE in SDK).
Table 1195. List of
USB registers
Offset Name Info
0x000 ADDR_ENDP Device address and endpoint control
0x004 ADDR_ENDP1 Interrupt endpoint 1. Only valid for HOST mode.
0x008 ADDR_ENDP2 Interrupt endpoint 2. Only valid for HOST mode.
0x00c ADDR_ENDP3 Interrupt endpoint 3. Only valid for HOST mode.
0x010 ADDR_ENDP4 Interrupt endpoint 4. Only valid for HOST mode.
0x014 ADDR_ENDP5 Interrupt endpoint 5. Only valid for HOST mode.
0x018 ADDR_ENDP6 Interrupt endpoint 6. Only valid for HOST mode.
0x01c ADDR_ENDP7 Interrupt endpoint 7. Only valid for HOST mode.
0x020 ADDR_ENDP8 Interrupt endpoint 8. Only valid for HOST mode.
0x024 ADDR_ENDP9 Interrupt endpoint 9. Only valid for HOST mode.
0x028 ADDR_ENDP10 Interrupt endpoint 10. Only valid for HOST mode.
0x02c ADDR_ENDP11 Interrupt endpoint 11. Only valid for HOST mode.
0x030 ADDR_ENDP12 Interrupt endpoint 12. Only valid for HOST mode.
0x034 ADDR_ENDP13 Interrupt endpoint 13. Only valid for HOST mode.
RP2350 Datasheet
12.7. USB 1159
Offset Name Info
0x038 ADDR_ENDP14 Interrupt endpoint 14. Only valid for HOST mode.
0x03c ADDR_ENDP15 Interrupt endpoint 15. Only valid for HOST mode.
0x040 MAIN_CTRL Main control register
0x044 SOF_WR Set the SOF (Start of Frame) frame number in the host controller.
The SOF packet is sent every 1ms and the host will increment the
frame number by 1 each time.
0x048 SOF_RD Read the last SOF (Start of Frame) frame number seen. In device
mode the last SOF received from the host. In host mode the last
SOF sent by the host.
0x04c SIE_CTRL SIE control register
0x050 SIE_STATUS SIE status register
0x054 INT_EP_CTRL interrupt endpoint control register
0x058 BUFF_STATUS Buffer status register. A bit set here indicates that a buffer has
completed on the endpoint (if the buffer interrupt is enabled). It
is possible for 2 buffers to be completed, so clearing the buffer
status bit may instantly re set it on the next clock cycle.
0x05c BUFF_CPU_SHOULD_HANDLE Which of the double buffers should be handled. Only valid if
using an interrupt per buffer (i.e. not per 2 buffers). Not valid for
host interrupt endpoint polling because they are only single
buffered.
0x060 EP_ABORT Device only: Can be set to ignore the buffer control register for
this endpoint in case you would like to revoke a buffer. A NAK
will be sent for every access to the endpoint until this bit is
cleared. A corresponding bit in EP_ABORT_DONE is set when it is safe
to modify the buffer control register.
0x064 EP_ABORT_DONE Device only: Used in conjunction with EP_ABORT. Set once an
endpoint is idle so the programmer knows it is safe to modify the
buffer control register.
0x068 EP_STALL_ARM Device: this bit must be set in conjunction with the STALL bit in the
buffer control register to send a STALL on EP0. The device
controller clears these bits when a SETUP packet is received
because the USB spec requires that a STALL condition is cleared
when a SETUP packet is received.
0x06c NAK_POLL Used by the host controller. Sets the wait time in microseconds
before trying again if the device replies with a NAK.
0x070 EP_STATUS_STALL_NAK Device: bits are set when the IRQ_ON_NAK or IRQ_ON_STALL bits are
set. For EP0 this comes from SIE_CTRL. For all other endpoints it
comes from the endpoint control register.
0x074 USB_MUXING Where to connect the USB controller. Should be to_phy by
default.
0x078 USB_PWR Overrides for the power signals in the event that the VBUS
signals are not hooked up to GPIO. Set the value of the override
and then the override enable to switch over to the override value.
RP2350 Datasheet
12.7. USB 1160
Offset Name Info
0x07c USBPHY_DIRECT This register allows for direct control of the USB phy. Use in
conjunction with usbphy_direct_override register to enable each
override bit.
0x080 USBPHY_DIRECT_OVERRIDE Override enable for each control in usbphy_direct
0x084 USBPHY_TRIM Used to adjust trim values of USB phy pull down resistors.
0x088 LINESTATE_TUNING Used for debug only.
0x08c INTR Raw Interrupts
0x090 INTE Interrupt Enable
0x094 INTF Interrupt Force
0x098 INTS Interrupt status after masking & forcing
0x100 SOF_TIMESTAMP_RAW Device only. Raw value of free-running PHY clock counter
@48MHz. Used to calculate time between SOF events.
0x104 SOF_TIMESTAMP_LAST Device only. Value of free-running PHY clock counter @48MHz
when last SOF event occured.
0x108 SM_STATE
0x10c EP_TX_ERROR TX error count for each endpoint. Write to each field to reset the
counter to 0.
0x110 EP_RX_ERROR RX error count for each endpoint. Write to each field to reset the
counter to 0.
0x114 DEV_SM_WATCHDOG Watchdog that forces the device state machine to idle and raises
an interrupt if the device stays in a state that isn’t idle for the
configured limit. The counter is reset on every state transition.
Set limit while enable is low and then set the enable.








# 12.1. UART
Arm documentation
Excerpted from the PrimeCell UART (PL011) Technical Reference Manual. Used with permission.
RP2350 has 2 identical instances of a UART peripheral, based on the Arm Primecell UART (PL011) (Revision r1p5).
Each instance supports the following features:
• Separate 32×8 TX and 32×12 RX FIFOs
• Programmable baud rate generator, clocked by clk_peri (see Figure 33)
• Standard asynchronous communication bits (start, stop, parity) added on transmit and removed on receive
• Line break detection
• Programmable serial interface (5, 6, 7, or 8 bits)
• 1 or 2 stop bits
• Programmable hardware flow control
Each UART can be connected to a number of GPIO pins as defined in the GPIO muxing table in Section 9.4. Connections
to the GPIO muxing use a prefix including the UART instance name uart0_ or uart1_, and include the following:
• Transmit data tx (referred to as UARTTXD in the following sections)
• Received data rx (referred to as UARTRXD in the following sections)
• Output flow control rts (referred to as nUARTRTS in the following sections)
• Input flow control cts (referred to as nUARTCTS in the following sections)
The modem mode and IrDA mode of the PL011 are not supported.
The UARTCLK is driven from clk_peri, and PCLK is driven from the system clock clk_sys (see Figure 33).
12.1.1. Overview
The UART performs:
• Serial-to-parallel conversion on data received from a peripheral device
• Parallel-to-serial conversion on data transmitted to the peripheral device
The CPU reads and writes data and control/status information through the AMBA APB interface. The transmit and
receive paths are buffered with internal FIFO memories that store up to 32 bytes independently in both transmit and
receive modes.
The UART:
• Includes a programmable baud rate generator that generates a common transmit and receive internal clock from
the UART internal reference clock input, UARTCLK
• Offers similar functionality to the industry-standard 16C650 UART device
• Supports a maximum baud rate of UARTCLK / 16 in UART mode (7.8 Mbaud at 125MHz)
RP2350 Datasheet
12.1. UART 961
The UART operation and baud rate values are controlled by the Line Control Register (UARTLCR_H) and the baud rate
divisor registers: Integer Baud Rate Register (UARTIBRD), and Fractional Baud Rate Register (UARTFBRD).
The UART can generate:
• Individually maskable interrupts from the receive (including timeout), transmit, modem status and error conditions
• A single combined interrupt so that the output is asserted if any of the individual interrupts are asserted and
unmasked
• DMA request signals for interfacing with a Direct Memory Access (DMA) controller
If a framing, parity, or break error occurs during reception, the appropriate error bit is set and stored in the FIFO. If an
overrun condition occurs, the overrun register bit is set immediately and FIFO data is prevented from being overwritten.
You can program the FIFOs to be 1-byte deep providing a conventional double-buffered UART interface.
There is a programmable hardware flow control feature that uses the nUARTCTS input and the nUARTRTS output to
automatically control the serial data flow.
12.1.2. Functional description
Figure 63. UART block
diagram. Test logic is
not shown for clarity.
12.1.2.1. AMBA APB interface
The AMBA APB interface generates read and write decodes for accesses to status/control registers, and the transmit
and receive FIFOs.
12.1.2.2. Register block
The register block stores data written, or to be read across the AMBA APB interface.
RP2350 Datasheet
12.1. UART 962
12.1.2.3. Baud rate generator
The baud rate generator contains free-running counters that generate the internal clocks: Baud16 and IrLPBaud16
signals. Baud16 provides timing information for UART transmit and receive control. Baud16 is a stream of pulses with a
width of one UARTCLK clock period and a frequency of 16 times the baud rate.
12.1.2.4. Transmit FIFO
The transmit FIFO is an 8-bit wide, 32 location deep, FIFO memory buffer. CPU data written across the APB interface is
stored in the FIFO until read out by the transmit logic. When disabled, the transmit FIFO acts like a one byte holding
register.
12.1.2.5. Receive FIFO
The receive FIFO is a 12-bit wide, 32 location deep, FIFO memory buffer. Received data and corresponding error bits are
stored in the receive FIFO by the receive logic until read out by the CPU across the APB interface. When disabled, the
receive FIFO acts like a one byte holding register.
12.1.2.6. Transmit logic
The transmit logic performs parallel-to-serial conversion on the data read from the transmit FIFO. Control logic outputs
the serial bit stream in the following order:
1. Start bit
2. Data bits (Least Significant Bit (LSB) first)
3. Parity bit
4. Stop bits according to the programmed configuration in control registers
12.1.2.7. Receive logic
The receive logic performs serial-to-parallel conversion on the received bit stream after a valid start pulse has been
detected. Receive logic includes overrun, parity, frame error checking, and line break detection; you can find the output
of these checks in the status that accompanies the data written to the receive FIFO.
12.1.2.8. Interrupt generation logic
The UART generates individual maskable active HIGH interrupts to the processor interrupt controllers. To generate
combined interrupts, the UART outputs an OR function of the individual interrupt requests.
For more information, see Section 12.1.6.
12.1.2.9. DMA interface
The UART provides an interface to connect to the DMA controller as a UART DMA; for more information, see Section
12.1.5.
RP2350 Datasheet
12.1. UART 963
12.1.2.10. Synchronizing registers and logic
The UART supports both asynchronous and synchronous operation of the clocks, PCLK and UARTCLK. The UART
implements always-on synchronisation registers and handshaking logic. This has a minimal impact on performance and
area. The UART performs control signal synchronisation on both directions of data flow (from the PCLK to the UARTCLK
domain, and from the UARTCLK to the PCLK domain).
12.1.3. Operation
12.1.3.1. Clock signals
The frequency selected for UARTCLK must accommodate the required range of baud rates:
• FUARTCLK (min) ≥ 16 × baud_rate (max)
• FUARTCLK (max) ≤ 16 × 65535 × baud_rate (min)
For example, for a range of baud rates from 110 baud to 460800 baud the UARTCLK frequency must be between
7.3728MHz to 115.34MHz.
To use all baud rates, the UARTCLK frequency must fall within the required error limits.
There is also a constraint on the ratio of clock frequencies for PCLK to UARTCLK. The frequency of UARTCLK must be no more
than 5/3 times faster than the frequency of PCLK:
• FUARTCLK ≤ 5/3 × FPCLK
For example, in UART mode, to generate 921600 baud when UARTCLK is 14.7456MHz, PCLK must be greater than or equal
to 8.85276MHz. This ensures that the UART has sufficient time to write the received data to the receive FIFO.
12.1.3.2. UART operation
Control data is written to the UART Line Control Register, UARTLCR. This register is 30 bits wide internally, but provides
external access through the APB interface by writes to the following registers:
• UARTLCR_H, which defines the following:
◦
transmission parameters
◦
word length
◦
buffer mode
◦
number of transmitted stop bits
◦
parity mode
◦
break generation
• UARTIBRD, which defines the integer baud rate divider
• UARTFBRD, which defines the fractional baud rate divider
12.1.3.2.1. Fractional baud rate divider
The baud rate divisor is a 22-bit number consisting of a 16-bit integer and a 6-bit fractional part. The baud rate generator
uses the baud rate divisor to determine the bit period. The fractional baud rate divider enables the use of any clock with
a frequency greater than 3.6864MHz to act as UARTCLK, while it is still possible to generate all the standard baud rates.
The 16-bit integer is written to the Integer Baud Rate Register, UARTIBRD. The 6-bit fractional part is written to the
Fractional Baud Rate Register, UARTFBRD. The Baud Rate Divisor has the following relationship to UARTCLK:
RP2350 Datasheet
12.1. UART 964
Baud Rate Divisor = UARTCLK/(16×Baud Rate) = where is the integer part and is the
fractional part separated by a decimal point as shown in Figure 64.
Figure 64. Baud rate
divisor.
To calculate the 6-bit number ( ), multiply the fractional part of the required baud rate divisor by 64 ( , where is the
width of the UARTFBRD register) and add 0.5 to account for rounding errors:
The UART generates an internal clock enable signal, Baud16. This is a stream of UARTCLK-wide pulses with an average
frequency of 16 times the required baud rate. Divide this signal by 16 to give the transmit clock. A low number in the
baud rate divisor produces a short bit period, and a high number in the baud rate divisor produces a long bit period.
12.1.3.2.2. Data transmission or reception
The UART uses two 32-byte FIFOs to store data received and transmitted. The receive FIFO has an extra four bits per
character for status information. For transmission, data is written into the transmit FIFO. If the UART is enabled, it
causes a data frame to start transmitting with the parameters indicated in the Line Control Register, UARTLCR_H. Data
continues to be transmitted until there is no data left in the transmit FIFO. The BUSY signal goes HIGH immediately after
data writes to the transmit FIFO (that is, the FIFO is non-empty) and remains asserted HIGH while data transmits. BUSY
is negated only when the transmit FIFO is empty, and the last character has been transmitted from the shift register,
including the stop bits. BUSY can be asserted HIGH even though the UART might no longer be enabled.
For each sample of data, three readings are taken and the majority value is kept. In the following paragraphs, the middle
sampling point is defined, and one sample is taken either side of it.
When the receiver is idle (UARTRXD continuously 1, in the marking state) and a LOW is detected on the data input (a start
bit has been received), the receive counter, with the clock enabled by Baud16, begins running and data is sampled on
the eighth cycle of that counter in UART mode, or the fourth cycle of the counter in SIR mode to allow for the shorter
logic 0 pulses (half way through a bit period).
The start bit is valid if UARTRXD is still LOW on the eighth cycle of Baud16, otherwise a false start bit is detected and it is
ignored.
If the start bit was valid, successive data bits are sampled on every 16th cycle of Baud16 (that is, one bit period later)
according to the programmed length of the data characters. The parity bit is then checked if parity mode was enabled.
Lastly, a valid stop bit is confirmed if UARTRXD is HIGH, otherwise a framing error has occurred. When a full word is
received, the data is stored in the receive FIFO, with any error bits associated with that word
12.1.3.2.3. Error bits
The receive FIFO stores three error bits in bits 8 (framing), 9 (parity), and 10 (break), each associated with a particular
character. An additional error bit, stored in bit 11 of the receive FIFO, indicates an overrun error.
12.1.3.2.4. Overrun bit
The overrun bit is not associated with the character in the receive FIFO. The overrun error is set when the FIFO is full and
the next character is completely received in the shift register. The data in the shift register is overwritten, but it is not
written into the FIFO. When an empty location becomes available in the FIFO, another character is received and the state
of the overrun bit is copied into the receive FIFO along with the received character. The overrun state is then cleared.
Table 1025 lists the bit functions of the receive FIFO.
RP2350 Datasheet
12.1. UART 965
Table 1025. Receive
FIFO bit functions
FIFO bit Function
11 Overrun indicator
10 Break error
9 Parity error
8 Framing error
7:0 Received data
12.1.3.2.5. Disabling the FIFOs
The bottom entry of the transmit and receive sides of the UART both have the equivalent of a 1-byte holding register.
You can manipulate flags to disable the FIFOs, allowing you to use the bottom entry of the FIFOs as a 1-byte register.
However, this doesn’t physically disable the FIFOs. When using the FIFOs as a 1-byte register, a write to the data register
bypasses the holding register unless the transmit shift register is already in use.
12.1.3.2.6. System and diagnostic loopback testing
To perform loopback testing for UART data, set the Loop Back Enable (LBE) bit to 1 in the Control Register, UARTCR.
Data transmitted on UARTTXD is received on the UARTRXD input.
12.1.3.3. UART character frame
Figure 65. UART
character frame.
12.1.4. UART hardware flow control
The fully-selectable hardware flow control feature enables you to control the serial data flow with the nUARTRTS output
and nUARTCTS input signals. Figure 66 shows how to communicate between two devices using hardware flow control:
Figure 66. Hardware
flow control between
two similar devices.
When the RTS flow control is enabled, nUARTRTS is asserted until the receive FIFO is filled up to the programmed
watermark level. When the CTS flow control is enabled, the transmitter can only transmit data when nUARTCTS is asserted.
The hardware flow control is selectable using the RTSEn and CTSEn bits in the Control Register, UARTCR. Table 1026 shows
how to configure UARTCR register bits to enable RTS and/or CTS.
RP2350 Datasheet
12.1. UART 966
Table 1026. Control
bits to enable and
disable hardware flow
control.
UARTCR register bits
CTSEn RTSEn Description
1 1 Both RTS and CTS flow control
enabled
1 0 Only CTS flow control enabled
0 1 Only RTS flow control enabled
0 0 Both RTS and CTS flow control
disabled
 NOTE
When RTS flow control is enabled, the software cannot use the RTSEn bit in the Control Register (UARTCR) to control
the status of nUARTRTS.
12.1.4.1. RTS flow control
The RTS flow control logic is linked to the programmable receive FIFO watermark levels.
When RTS flow control is disabled, the receive FIFO receives data until full, or no more data is transmitted to it.
When RTS flow control is enabled, the nUARTRTS is asserted until the receive FIFO fills up to the watermark level. When the
receive FIFO reaches the watermark level, the nUARTRTS signal is de-asserted. This indicates that the FIFO has no more
room to receive data. The transmission of data is expected to cease after the current character has been transmitted.
When the receive FIFO drains below the watermark level, the nUARTRTS signal is reasserted.
12.1.4.2. CTS flow control
The CTS flow control logic is linked to the nUARTCTS signal.
When CTS flow control is disabled, the transmitter transmits data until the transmit FIFO is empty.
When CTS flow control is enabled, the transmitter checks the nUARTCTS signal before transmitting each byte. It only
transmits the byte if the nUARTCTS signal is asserted. As long as the transmit FIFO is not empty and nUARTCTS is asserted,
data continues to transmit. If the transmit FIFO is empty and the nUARTCTS signal is asserted, no data is transmitted. If the
nUARTCTS signal is de-asserted during transmission, the transmitter finishes transmitting the current character before
stopping.
12.1.5. UART DMA interface
The UART provides an interface to connect to a DMA controller. The DMA operation of the UART is controlled using the
DMA Control Register, UARTDMACR. The DMA interface includes the following signals:
For receive:
UARTRXDMASREQ
Single character DMA transfer request, asserted by the UART. For receive, one character consists of up to 12 bits.
This signal is asserted when the receive FIFO contains at least one character.
UARTRXDMABREQ
Burst DMA transfer request, asserted by the UART. This signal is asserted when the receive FIFO contains more
characters than the programmed watermark level. You can program the watermark level for each FIFO using the
Interrupt FIFO Level Select Register (UARTIFLS).
RP2350 Datasheet
12.1. UART 967
UARTRXDMACLR
DMA request clear, asserted by a DMA controller to clear the receive request signals. If DMA burst transfer is
requested, the clear signal is asserted during the transfer of the last data in the burst.
For transmit:
UARTTXDMASREQ
Single character DMA transfer request, asserted by the UART. For transmit, one character consists of up to eight
bits. This signal is asserted when there is at least one empty location in the transmit FIFO.
UARTTXDMABREQ
Burst DMA transfer request, asserted by the UART. This signal is asserted when the transmit FIFO contains less
characters than the watermark level. You can program the watermark level for each FIFO using the Interrupt FIFO
Level Select Register (UARTIFLS).
UARTTXDMACLR
DMA request clear, asserted by a DMA controller to clear the transmit request signals. If DMA burst transfer is
requested, the clear signal is asserted during the transfer of the last data in the burst.
The burst transfer and single transfer request signals are not mutually exclusive: they can both be asserted at the same
time. When the receive FIFO exceeds the watermark level, the burst transfer request and the single transfer request
signals are both asserted. When the receive FIFO is below than the watermark level, only the single transfer request
signal is asserted. This is useful in situations where the number of characters left to be received in the stream is less
than a burst.
Consider a scenario where the watermark level is set to four, but 19 characters are left to be received. The DMA
controller then transfers four bursts of four characters and three single transfers to complete the stream.
 NOTE
For the remaining three characters, the UART cannot assert the burst request.
Each request signal remains asserted until the relevant DMACLR signal is asserted. After the request clear signal is deasserted, a request signal can become active again, depending on the conditions described previously. All request
signals are de-asserted if the UART is disabled or the relevant DMA enable bit, TXDMAE or RXDMAE, in the DMA Control
Register, UARTDMACR, is cleared.
If you disable the FIFOs in the UART, it operates in character mode. Character mode limits FIFO transfers to a single
character at a time, so only the DMA single transfer mode can operate. In character mode, only the UARTRXDMASREQ and
UARTTXDMASREQ request signals can be asserted. For information about disabling the FIFOs, see the Line Control Register,
UARTLCR_H.
When the UART is in the FIFO enabled mode, data transfers can use either single or burst transfers depending on the
programmed watermark level and the amount of data in the FIFO. Table 1027 lists the trigger points for UARTRXDMABREQ
and UARTTXDMABREQ, depending on the watermark level, for the transmit and receive FIFOs.
Table 1027. DMA
trigger points for the
transmit and receive
FIFOs.
Watermark level Burst length
Transmit (number of empty
locations)
Receive (number of filled locations)
1/8 28 4
1/4 24 8
1/2 16 16
3/4 8 24
7/8 4 28
In addition, the DMAONERR bit in the DMA Control Register, UARTDMACR, supports the use of the receive error interrupt,
RP2350 Datasheet
12.1. UART 968
UARTEINTR. It enables the DMA receive request outputs, UARTRXDMASREQ or UARTRXDMABREQ, to be masked out when the UART
error interrupt, UARTEINTR, is asserted. The DMA receive request outputs remain inactive until the UARTEINTR is cleared. The
DMA transmit request outputs are unaffected.
Figure 67. DMA
transfer waveforms.
Figure 67 shows the timing diagram for both a single transfer request and a burst transfer request with the appropriate
DMACLR signal. The signals are all synchronous to PCLK. For the sake of clarity it is assumed that there is no
synchronization of the request signals in the DMA controller.
12.1.6. Interrupts
There are eleven maskable interrupts generated in the UART. On RP2350, only the combined interrupt output, UARTINTR, is
connected.
To enable or disable individual interrupts, change the mask bits in the Interrupt Mask Set/Clear Register, UARTIMSC. Set
the appropriate mask bit HIGH to enable the interrupt.
The transmit and receive dataflow interrupts UARTRXINTR and UARTTXINTR have been separated from the status interrupts.
This enables you to use UARTRXINTR and UARTTXINTR to read or write data in response to FIFO trigger levels.
The error interrupt, UARTEINTR, can be triggered when there is an error in the reception of data. A number of error
conditions are possible.
The modem status interrupt, UARTMSINTR, is a combined interrupt of all the individual modem status signals.
The status of the individual interrupt sources can be read either from the Raw Interrupt Status Register, UARTRIS, or from
the Masked Interrupt Status Register, UARTMIS.
12.1.6.1. UARTMSINTR
The modem status interrupt is asserted if any of the modem status signals (nUARTCTS, nUARTDCD, nUARTDSR, and nUARTRI)
change. To clear the modem status interrupt, write a 1 to the bits corresponding to the modem status signals that
generated the interrupt in the Interrupt Clear Register (UARTICR).
12.1.6.2. UARTRXINTR
The receive interrupt changes state when one of the following events occurs:
• The FIFOs are enabled and the receive FIFO reaches the programmed trigger level. This asserts the receive
interrupt HIGH. To clear the receive interrupt, read data from the receive FIFO until it drops below the trigger level.
• The FIFOs are disabled (have a depth of one location) and data is received, thereby filling the receive FIFO. This
asserts the receive interrupt HIGH. To clear the receive interrupt, perform a single read from the receive FIFO.
In both cases, you can also clear the interrupt manually.
12.1.6.3. UARTTXINTR
The transmit interrupt changes state when one of the following events occurs:
• The FIFOs are enabled and the transmit FIFO is equal to or lower than the programmed trigger level. This asserts
the transmit interrupt HIGH. To clear the transmit interrupt, write data to the transmit FIFO until it exceeds the
RP2350 Datasheet
12.1. UART 969
trigger level.
• The FIFOs are disabled (have a depth of one location) and there is no data present in the transmit FIFO. This
asserts the transmit interrupt HIGH. To clear the transmit interrupt, perform a single write to the transmit FIFO.
In both cases, you can also clear the interrupt manually.
To update the transmit FIFO, write data to the transmit FIFO before or after enabling the UART and the interrupts.
 NOTE
The transmit interrupt is based on a transition through a level, rather than on the level itself. When the interrupt and
the UART is enabled before any data is written to the transmit FIFO, the interrupt is not set. The interrupt is only set
after written data leaves the single location of the transmit FIFO and it becomes empty.
12.1.6.4. UARTRTINTR
The receive timeout interrupt is asserted when the receive FIFO is not empty and no more data is received during a 32-
bit period.
The receive timeout interrupt is cleared in the following scenarios:
• the FIFO becomes empty through reading all the data or by reading the holding register
• a 1 is written to the corresponding bit of the Interrupt Clear Register, UARTICR
12.1.6.5. UARTEINTR
The error interrupt is asserted when an error occurs in the reception of data by the UART. The interrupt can be caused
by a number of different error conditions:
• framing
• parity
• break
• overrun
To determine the cause of the interrupt, read the Raw Interrupt Status Register (UARTRIS) or the Masked Interrupt Status
Register (UARTMIS). To clear the interrupt, write to the relevant bits of the Interrupt Clear Register, UARTICR (bits 7 to 10 are
the error clear bits).
12.1.6.6. UARTINTR
The interrupts are also combined into a single output, that is an OR function of the individual masked sources. You can
connect this output to a system interrupt controller to provide another level of masking on a individual peripheral basis.
The combined UART interrupt is asserted if any of the individual interrupts are asserted and enabled.
12.1.7. Programmer’s model
The SDK provides a uart_init function to configure the UART with a particular baud rate. Once the UART is initialised,
the user must configure a GPIO pin as UART_TX and UART_RX. See Section 9.10.1 for more information on selecting a GPIO
function.
To initialise the UART, the uart_init function takes the following steps:
1. De-asserts the reset
RP2350 Datasheet
12.1. UART 970
2. Enables clk_peri
3. Sets enable bits in the control register
4. Enables the FIFOs
5. Sets the baud rate divisors
6. Sets the format
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_uart/uart.c Lines 42 - 92
42 uint uart_init(uart_inst_t *uart, uint baudrate) {
43 invalid_params_if(HARDWARE_UART, uart != uart0 && uart != uart1);
44
45 if (uart_clock_get_hz(uart) == 0) {
46 return 0;
47 }
48
49 uart_reset(uart);
50 uart_unreset(uart);
51
52 uart_set_translate_crlf(uart, PICO_UART_DEFAULT_CRLF);
53
54 // Any LCR writes need to take place before enabling the UART
55 uint baud = uart_set_baudrate(uart, baudrate);
56
57 // inline the uart_set_format() call, as we don't need the CR disable/re-enable
58 // protection, and also many people will never call it again, so having
59 // the generic function is not useful, and much bigger than this inlined
60 // code which is only a handful of instructions.
61 //
62 // The UART_UARTLCR_H_FEN_BITS setting is combined as well as it is the same register
63 #ifdef 0
64 uart_set_format(uart, 8, 1, UART_PARITY_NONE);
65 // Enable FIFOs (must be before setting UARTEN, as this is an LCR access)
66 hw_set_bits(&uart_get_hw(uart)->lcr_h, UART_UARTLCR_H_FEN_BITS);
67 #else
68 uint data_bits = 8;
69 uint stop_bits = 1;
70 uint parity = UART_PARITY_NONE;
71 hw_write_masked(&uart_get_hw(uart)->lcr_h,
72 ((data_bits - 5u) << UART_UARTLCR_H_WLEN_LSB) |
73 ((stop_bits - 1u) << UART_UARTLCR_H_STP2_LSB) |
74 (bool_to_bit(parity != UART_PARITY_NONE) << UART_UARTLCR_H_PEN_LSB) |
75 (bool_to_bit(parity == UART_PARITY_EVEN) << UART_UARTLCR_H_EPS_LSB) |
76 UART_UARTLCR_H_FEN_BITS,
77 UART_UARTLCR_H_WLEN_BITS | UART_UARTLCR_H_STP2_BITS |
78 UART_UARTLCR_H_PEN_BITS | UART_UARTLCR_H_EPS_BITS |
79 UART_UARTLCR_H_FEN_BITS);
80 #endif
81
82 // Enable the UART, both TX and RX
83 uart_get_hw(uart)->cr = UART_UARTCR_UARTEN_BITS | UART_UARTCR_TXE_BITS |
  UART_UARTCR_RXE_BITS;
84 // Always enable DREQ signals -- no harm in this if DMA is not listening
85 uart_get_hw(uart)->dmacr = UART_UARTDMACR_TXDMAE_BITS | UART_UARTDMACR_RXDMAE_BITS;
86
87 return baud;
88 }
RP2350 Datasheet
12.1. UART 971
12.1.7.1. Baud rate calculation
The UART baud rate is derived from dividing clk_peri.
If the required baud rate is 115200 and UARTCLK = 125MHz then:
Baud Rate Divisor = (125 × 106
)/(16 × 115200) ~= 67.817
Therefore, BRDI = 67 and BRDF = 0.817,
Therefore, fractional part, m = integer((0.817 × 64) + 0.5) = 52
Generated baud rate divider = 67 + 52/64 = 67.8125
Generated baud rate = (125 × 106
)/(16 × 67.8125) ~= 115207
Error = (abs(115200 - 115207) / 115200) × 100 ~= 0.006%
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_uart/uart.c Lines 155 - 180
155 uint uart_set_baudrate(uart_inst_t *uart, uint baudrate) {
156 invalid_params_if(HARDWARE_UART, baudrate == 0);
157 uint32_t baud_rate_div = (8 * uart_clock_get_hz(uart) / baudrate) + 1;
158 uint32_t baud_ibrd = baud_rate_div >> 7;
159 uint32_t baud_fbrd;
160
161 if (baud_ibrd == 0) {
162 baud_ibrd = 1;
163 baud_fbrd = 0;
164 } else if (baud_ibrd >= 65535) {
165 baud_ibrd = 65535;
166 baud_fbrd = 0;
167 } else {
168 baud_fbrd = (baud_rate_div & 0x7f) >> 1;
169 }
170
171 uart_get_hw(uart)->ibrd = baud_ibrd;
172 uart_get_hw(uart)->fbrd = baud_fbrd;
173
174 // PL011 needs a (dummy) LCR_H write to latch in the divisors.
175 // We don't want to actually change LCR_H contents here.
176 uart_write_lcr_bits_masked(uart, 0, 0);
177
178 // See datasheet
179 return (4 * uart_clock_get_hz(uart)) / (64 * baud_ibrd + baud_fbrd);
180 }
12.1.8. List of registers
The UART0 and UART1 registers start at base addresses of 0x40070000 and 0x40078000 respectively (defined as
UART0_BASE and UART1_BASE in SDK).
Table 1028. List of
UART registers
Offset Name Info
0x000 UARTDR Data Register, UARTDR
0x004 UARTRSR Receive Status Register/Error Clear Register,
UARTRSR/UARTECR
0x018 UARTFR Flag Register, UARTFR
0x020 UARTILPR IrDA Low-Power Counter Register, UARTILPR
RP2350 Datasheet
12.1. UART 972
Offset Name Info
0x024 UARTIBRD Integer Baud Rate Register, UARTIBRD
0x028 UARTFBRD Fractional Baud Rate Register, UARTFBRD
0x02c UARTLCR_H Line Control Register, UARTLCR_H
0x030 UARTCR Control Register, UARTCR
0x034 UARTIFLS Interrupt FIFO Level Select Register, UARTIFLS
0x038 UARTIMSC Interrupt Mask Set/Clear Register, UARTIMSC
0x03c UARTRIS Raw Interrupt Status Register, UARTRIS
0x040 UARTMIS Masked Interrupt Status Register, UARTMIS
0x044 UARTICR Interrupt Clear Register, UARTICR
0x048 UARTDMACR DMA Control Register, UARTDMACR
0xfe0 UARTPERIPHID0 UARTPeriphID0 Register
0xfe4 UARTPERIPHID1 UARTPeriphID1 Register
0xfe8 UARTPERIPHID2 UARTPeriphID2 Register
0xfec UARTPERIPHID3 UARTPeriphID3 Register
0xff0 UARTPCELLID0 UARTPCellID0 Register
0xff4 UARTPCELLID1 UARTPCellID1 Register
0xff8 UARTPCELLID2 UARTPCellID2 Register
0xffc UARTPCELLID3 UARTPCellID3 Register
UART: UARTDR Register
Offset: 0x000
Description
Data Register, UARTDR
Table 1029. UARTDR
Register
Bits Description Type Reset
31:12 Reserved. - -
11 OE: Overrun error. This bit is set to 1 if data is received and the receive FIFO is
already full. This is cleared to 0 once there is an empty space in the FIFO and a
new character can be written to it.
RO -
10 BE: Break error. This bit is set to 1 if a break condition was detected, indicating
that the received data input was held LOW for longer than a full-word
transmission time (defined as start, data, parity and stop bits). In FIFO mode,
this error is associated with the character at the top of the FIFO. When a break
occurs, only one 0 character is loaded into the FIFO. The next character is only
enabled after the receive data input goes to a 1 (marking state), and the next
valid start bit is received.
RO -
9 PE: Parity error. When set to 1, it indicates that the parity of the received data
character does not match the parity that the EPS and SPS bits in the Line
Control Register, UARTLCR_H. In FIFO mode, this error is associated with the
character at the top of the FIFO.
RO -
RP2350 Datasheet
12.1. UART 973
Bits Description Type Reset
8 FE: Framing error. When set to 1, it indicates that the received character did
not have a valid stop bit (a valid stop bit is 1). In FIFO mode, this error is
associated with the character at the top of the FIFO.
RO -
7:0 DATA: Receive (read) data character. Transmit (write) data character. RWF -
UART: UARTRSR Register
Offset: 0x004
Description
Receive Status Register/Error Clear Register, UARTRSR/UARTECR
Table 1030. UARTRSR
Register
Bits Description Type Reset
31:4 Reserved. - -
3 OE: Overrun error. This bit is set to 1 if data is received and the FIFO is already
full. This bit is cleared to 0 by a write to UARTECR. The FIFO contents remain
valid because no more data is written when the FIFO is full, only the contents
of the shift register are overwritten. The CPU must now read the data, to
empty the FIFO.
WC 0x0
2 BE: Break error. This bit is set to 1 if a break condition was detected, indicating
that the received data input was held LOW for longer than a full-word
transmission time (defined as start, data, parity, and stop bits). This bit is
cleared to 0 after a write to UARTECR. In FIFO mode, this error is associated
with the character at the top of the FIFO. When a break occurs, only one 0
character is loaded into the FIFO. The next character is only enabled after the
receive data input goes to a 1 (marking state) and the next valid start bit is
received.
WC 0x0
1 PE: Parity error. When set to 1, it indicates that the parity of the received data
character does not match the parity that the EPS and SPS bits in the Line
Control Register, UARTLCR_H. This bit is cleared to 0 by a write to UARTECR.
In FIFO mode, this error is associated with the character at the top of the FIFO.
WC 0x0
0 FE: Framing error. When set to 1, it indicates that the received character did
not have a valid stop bit (a valid stop bit is 1). This bit is cleared to 0 by a write
to UARTECR. In FIFO mode, this error is associated with the character at the
top of the FIFO.
WC 0x0
UART: UARTFR Register
Offset: 0x018
Description
Flag Register, UARTFR
Table 1031. UARTFR
Register
Bits Description Type Reset
31:9 Reserved. - -
8 RI: Ring indicator. This bit is the complement of the UART ring indicator,
nUARTRI, modem status input. That is, the bit is 1 when nUARTRI is LOW.
RO -
RP2350 Datasheet
12.1. UART 974
Bits Description Type Reset
7 TXFE: Transmit FIFO empty. The meaning of this bit depends on the state of
the FEN bit in the Line Control Register, UARTLCR_H. If the FIFO is disabled,
this bit is set when the transmit holding register is empty. If the FIFO is
enabled, the TXFE bit is set when the transmit FIFO is empty. This bit does not
indicate if there is data in the transmit shift register.
RO 0x1
6 RXFF: Receive FIFO full. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the receive holding register is full. If the FIFO is enabled, the RXFF bit is set
when the receive FIFO is full.
RO 0x0
5 TXFF: Transmit FIFO full. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the transmit holding register is full. If the FIFO is enabled, the TXFF bit is set
when the transmit FIFO is full.
RO 0x0
4 RXFE: Receive FIFO empty. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the receive holding register is empty. If the FIFO is enabled, the RXFE bit is set
when the receive FIFO is empty.
RO 0x1
3 BUSY: UART busy. If this bit is set to 1, the UART is busy transmitting data.
This bit remains set until the complete byte, including all the stop bits, has
been sent from the shift register. This bit is set as soon as the transmit FIFO
becomes non-empty, regardless of whether the UART is enabled or not.
RO 0x0
2 DCD: Data carrier detect. This bit is the complement of the UART data carrier
detect, nUARTDCD, modem status input. That is, the bit is 1 when nUARTDCD
is LOW.
RO -
1 DSR: Data set ready. This bit is the complement of the UART data set ready,
nUARTDSR, modem status input. That is, the bit is 1 when nUARTDSR is LOW.
RO -
0 CTS: Clear to send. This bit is the complement of the UART clear to send,
nUARTCTS, modem status input. That is, the bit is 1 when nUARTCTS is LOW.
RO -
UART: UARTILPR Register
Offset: 0x020
Description
IrDA Low-Power Counter Register, UARTILPR
Table 1032. UARTILPR
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 ILPDVSR: 8-bit low-power divisor value. These bits are cleared to 0 at reset. RW 0x00
UART: UARTIBRD Register
Offset: 0x024
Description
Integer Baud Rate Register, UARTIBRD
Table 1033. UARTIBRD
Register
Bits Description Type Reset
31:16 Reserved. - -
RP2350 Datasheet
12.1. UART 975
Bits Description Type Reset
15:0 BAUD_DIVINT: The integer baud rate divisor. These bits are cleared to 0 on
reset.
RW 0x0000
UART: UARTFBRD Register
Offset: 0x028
Description
Fractional Baud Rate Register, UARTFBRD
Table 1034.
UARTFBRD Register
Bits Description Type Reset
31:6 Reserved. - -
5:0 BAUD_DIVFRAC: The fractional baud rate divisor. These bits are cleared to 0
on reset.
RW 0x00
UART: UARTLCR_H Register
Offset: 0x02c
Description
Line Control Register, UARTLCR_H
Table 1035.
UARTLCR_H Register
Bits Description Type Reset
31:8 Reserved. - -
7 SPS: Stick parity select. 0 = stick parity is disabled 1 = either: * if the EPS bit is
0 then the parity bit is transmitted and checked as a 1 * if the EPS bit is 1 then
the parity bit is transmitted and checked as a 0. This bit has no effect when
the PEN bit disables parity checking and generation.
RW 0x0
6:5 WLEN: Word length. These bits indicate the number of data bits transmitted or
received in a frame as follows: b11 = 8 bits b10 = 7 bits b01 = 6 bits b00 = 5
bits.
RW 0x0
4 FEN: Enable FIFOs: 0 = FIFOs are disabled (character mode) that is, the FIFOs
become 1-byte-deep holding registers 1 = transmit and receive FIFO buffers
are enabled (FIFO mode).
RW 0x0
3 STP2: Two stop bits select. If this bit is set to 1, two stop bits are transmitted
at the end of the frame. The receive logic does not check for two stop bits
being received.
RW 0x0
2 EPS: Even parity select. Controls the type of parity the UART uses during
transmission and reception: 0 = odd parity. The UART generates or checks for
an odd number of 1s in the data and parity bits. 1 = even parity. The UART
generates or checks for an even number of 1s in the data and parity bits. This
bit has no effect when the PEN bit disables parity checking and generation.
RW 0x0
1 PEN: Parity enable: 0 = parity is disabled and no parity bit added to the data
frame 1 = parity checking and generation is enabled.
RW 0x0
0 BRK: Send break. If this bit is set to 1, a low-level is continually output on the
UARTTXD output, after completing transmission of the current character. For
the proper execution of the break command, the software must set this bit for
at least two complete frames. For normal use, this bit must be cleared to 0.
RW 0x0
UART: UARTCR Register
RP2350 Datasheet
12.1. UART 976
Offset: 0x030
Description
Control Register, UARTCR
Table 1036. UARTCR
Register
Bits Description Type Reset
31:16 Reserved. - -
15 CTSEN: CTS hardware flow control enable. If this bit is set to 1, CTS hardware
flow control is enabled. Data is only transmitted when the nUARTCTS signal is
asserted.
RW 0x0
14 RTSEN: RTS hardware flow control enable. If this bit is set to 1, RTS hardware
flow control is enabled. Data is only requested when there is space in the
receive FIFO for it to be received.
RW 0x0
13 OUT2: This bit is the complement of the UART Out2 (nUARTOut2) modem
status output. That is, when the bit is programmed to a 1, the output is 0. For
DTE this can be used as Ring Indicator (RI).
RW 0x0
12 OUT1: This bit is the complement of the UART Out1 (nUARTOut1) modem
status output. That is, when the bit is programmed to a 1 the output is 0. For
DTE this can be used as Data Carrier Detect (DCD).
RW 0x0
11 RTS: Request to send. This bit is the complement of the UART request to
send, nUARTRTS, modem status output. That is, when the bit is programmed
to a 1 then nUARTRTS is LOW.
RW 0x0
10 DTR: Data transmit ready. This bit is the complement of the UART data
transmit ready, nUARTDTR, modem status output. That is, when the bit is
programmed to a 1 then nUARTDTR is LOW.
RW 0x0
9 RXE: Receive enable. If this bit is set to 1, the receive section of the UART is
enabled. Data reception occurs for either UART signals or SIR signals
depending on the setting of the SIREN bit. When the UART is disabled in the
middle of reception, it completes the current character before stopping.
RW 0x1
8 TXE: Transmit enable. If this bit is set to 1, the transmit section of the UART is
enabled. Data transmission occurs for either UART signals, or SIR signals
depending on the setting of the SIREN bit. When the UART is disabled in the
middle of transmission, it completes the current character before stopping.
RW 0x1
7 LBE: Loopback enable. If this bit is set to 1 and the SIREN bit is set to 1 and
the SIRTEST bit in the Test Control Register, UARTTCR is set to 1, then the
nSIROUT path is inverted, and fed through to the SIRIN path. The SIRTEST bit
in the test register must be set to 1 to override the normal half-duplex SIR
operation. This must be the requirement for accessing the test registers
during normal operation, and SIRTEST must be cleared to 0 when loopback
testing is finished. This feature reduces the amount of external coupling
required during system test. If this bit is set to 1, and the SIRTEST bit is set to
0, the UARTTXD path is fed through to the UARTRXD path. In either SIR mode
or UART mode, when this bit is set, the modem outputs are also fed through to
the modem inputs. This bit is cleared to 0 on reset, to disable loopback.
RW 0x0
6:3 Reserved. - -
RP2350 Datasheet
12.1. UART 977
Bits Description Type Reset
2 SIRLP: SIR low-power IrDA mode. This bit selects the IrDA encoding mode. If
this bit is cleared to 0, low-level bits are transmitted as an active high pulse
with a width of 3 / 16th of the bit period. If this bit is set to 1, low-level bits are
transmitted with a pulse width that is 3 times the period of the IrLPBaud16
input signal, regardless of the selected bit rate. Setting this bit uses less
power, but might reduce transmission distances.
RW 0x0
1 SIREN: SIR enable: 0 = IrDA SIR ENDEC is disabled. nSIROUT remains LOW (no
light pulse generated), and signal transitions on SIRIN have no effect. 1 = IrDA
SIR ENDEC is enabled. Data is transmitted and received on nSIROUT and
SIRIN. UARTTXD remains HIGH, in the marking state. Signal transitions on
UARTRXD or modem status inputs have no effect. This bit has no effect if the
UARTEN bit disables the UART.
RW 0x0
0 UARTEN: UART enable: 0 = UART is disabled. If the UART is disabled in the
middle of transmission or reception, it completes the current character before
stopping. 1 = the UART is enabled. Data transmission and reception occurs for
either UART signals or SIR signals depending on the setting of the SIREN bit.
RW 0x0
UART: UARTIFLS Register
Offset: 0x034
Description
Interrupt FIFO Level Select Register, UARTIFLS
Table 1037. UARTIFLS
Register
Bits Description Type Reset
31:6 Reserved. - -
5:3 RXIFLSEL: Receive interrupt FIFO level select. The trigger points for the receive
interrupt are as follows: b000 = Receive FIFO becomes >= 1 / 8 full b001 =
Receive FIFO becomes >= 1 / 4 full b010 = Receive FIFO becomes >= 1 / 2 full
b011 = Receive FIFO becomes >= 3 / 4 full b100 = Receive FIFO becomes >= 7
/ 8 full b101-b111 = reserved.
RW 0x2
2:0 TXIFLSEL: Transmit interrupt FIFO level select. The trigger points for the
transmit interrupt are as follows: b000 = Transmit FIFO becomes <= 1 / 8 full
b001 = Transmit FIFO becomes <= 1 / 4 full b010 = Transmit FIFO becomes <=
1 / 2 full b011 = Transmit FIFO becomes <= 3 / 4 full b100 = Transmit FIFO
becomes <= 7 / 8 full b101-b111 = reserved.
RW 0x2
UART: UARTIMSC Register
Offset: 0x038
Description
Interrupt Mask Set/Clear Register, UARTIMSC
Table 1038.
UARTIMSC Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEIM: Overrun error interrupt mask. A read returns the current mask for the
UARTOEINTR interrupt. On a write of 1, the mask of the UARTOEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
RP2350 Datasheet
12.1. UART 978
Bits Description Type Reset
9 BEIM: Break error interrupt mask. A read returns the current mask for the
UARTBEINTR interrupt. On a write of 1, the mask of the UARTBEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
8 PEIM: Parity error interrupt mask. A read returns the current mask for the
UARTPEINTR interrupt. On a write of 1, the mask of the UARTPEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
7 FEIM: Framing error interrupt mask. A read returns the current mask for the
UARTFEINTR interrupt. On a write of 1, the mask of the UARTFEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
6 RTIM: Receive timeout interrupt mask. A read returns the current mask for the
UARTRTINTR interrupt. On a write of 1, the mask of the UARTRTINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
5 TXIM: Transmit interrupt mask. A read returns the current mask for the
UARTTXINTR interrupt. On a write of 1, the mask of the UARTTXINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
4 RXIM: Receive interrupt mask. A read returns the current mask for the
UARTRXINTR interrupt. On a write of 1, the mask of the UARTRXINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
3 DSRMIM: nUARTDSR modem interrupt mask. A read returns the current mask
for the UARTDSRINTR interrupt. On a write of 1, the mask of the
UARTDSRINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
2 DCDMIM: nUARTDCD modem interrupt mask. A read returns the current mask
for the UARTDCDINTR interrupt. On a write of 1, the mask of the
UARTDCDINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
1 CTSMIM: nUARTCTS modem interrupt mask. A read returns the current mask
for the UARTCTSINTR interrupt. On a write of 1, the mask of the
UARTCTSINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
0 RIMIM: nUARTRI modem interrupt mask. A read returns the current mask for
the UARTRIINTR interrupt. On a write of 1, the mask of the UARTRIINTR
interrupt is set. A write of 0 clears the mask.
RW 0x0
UART: UARTRIS Register
Offset: 0x03c
Description
Raw Interrupt Status Register, UARTRIS
Table 1039. UARTRIS
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OERIS: Overrun error interrupt status. Returns the raw interrupt state of the
UARTOEINTR interrupt.
RO 0x0
9 BERIS: Break error interrupt status. Returns the raw interrupt state of the
UARTBEINTR interrupt.
RO 0x0
8 PERIS: Parity error interrupt status. Returns the raw interrupt state of the
UARTPEINTR interrupt.
RO 0x0
RP2350 Datasheet
12.1. UART 979
Bits Description Type Reset
7 FERIS: Framing error interrupt status. Returns the raw interrupt state of the
UARTFEINTR interrupt.
RO 0x0
6 RTRIS: Receive timeout interrupt status. Returns the raw interrupt state of the
UARTRTINTR interrupt. a
RO 0x0
5 TXRIS: Transmit interrupt status. Returns the raw interrupt state of the
UARTTXINTR interrupt.
RO 0x0
4 RXRIS: Receive interrupt status. Returns the raw interrupt state of the
UARTRXINTR interrupt.
RO 0x0
3 DSRRMIS: nUARTDSR modem interrupt status. Returns the raw interrupt state
of the UARTDSRINTR interrupt.
RO -
2 DCDRMIS: nUARTDCD modem interrupt status. Returns the raw interrupt state
of the UARTDCDINTR interrupt.
RO -
1 CTSRMIS: nUARTCTS modem interrupt status. Returns the raw interrupt state
of the UARTCTSINTR interrupt.
RO -
0 RIRMIS: nUARTRI modem interrupt status. Returns the raw interrupt state of
the UARTRIINTR interrupt.
RO -
UART: UARTMIS Register
Offset: 0x040
Description
Masked Interrupt Status Register, UARTMIS
Table 1040. UARTMIS
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEMIS: Overrun error masked interrupt status. Returns the masked interrupt
state of the UARTOEINTR interrupt.
RO 0x0
9 BEMIS: Break error masked interrupt status. Returns the masked interrupt
state of the UARTBEINTR interrupt.
RO 0x0
8 PEMIS: Parity error masked interrupt status. Returns the masked interrupt
state of the UARTPEINTR interrupt.
RO 0x0
7 FEMIS: Framing error masked interrupt status. Returns the masked interrupt
state of the UARTFEINTR interrupt.
RO 0x0
6 RTMIS: Receive timeout masked interrupt status. Returns the masked
interrupt state of the UARTRTINTR interrupt.
RO 0x0
5 TXMIS: Transmit masked interrupt status. Returns the masked interrupt state
of the UARTTXINTR interrupt.
RO 0x0
4 RXMIS: Receive masked interrupt status. Returns the masked interrupt state
of the UARTRXINTR interrupt.
RO 0x0
3 DSRMMIS: nUARTDSR modem masked interrupt status. Returns the masked
interrupt state of the UARTDSRINTR interrupt.
RO -
2 DCDMMIS: nUARTDCD modem masked interrupt status. Returns the masked
interrupt state of the UARTDCDINTR interrupt.
RO -
RP2350 Datasheet
12.1. UART 980
Bits Description Type Reset
1 CTSMMIS: nUARTCTS modem masked interrupt status. Returns the masked
interrupt state of the UARTCTSINTR interrupt.
RO -
0 RIMMIS: nUARTRI modem masked interrupt status. Returns the masked
interrupt state of the UARTRIINTR interrupt.
RO -
UART: UARTICR Register
Offset: 0x044
Description
Interrupt Clear Register, UARTICR
Table 1041. UARTICR
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEIC: Overrun error interrupt clear. Clears the UARTOEINTR interrupt. WC -
9 BEIC: Break error interrupt clear. Clears the UARTBEINTR interrupt. WC -
8 PEIC: Parity error interrupt clear. Clears the UARTPEINTR interrupt. WC -
7 FEIC: Framing error interrupt clear. Clears the UARTFEINTR interrupt. WC -
6 RTIC: Receive timeout interrupt clear. Clears the UARTRTINTR interrupt. WC -
5 TXIC: Transmit interrupt clear. Clears the UARTTXINTR interrupt. WC -
4 RXIC: Receive interrupt clear. Clears the UARTRXINTR interrupt. WC -
3 DSRMIC: nUARTDSR modem interrupt clear. Clears the UARTDSRINTR
interrupt.
WC -
2 DCDMIC: nUARTDCD modem interrupt clear. Clears the UARTDCDINTR
interrupt.
WC -
1 CTSMIC: nUARTCTS modem interrupt clear. Clears the UARTCTSINTR
interrupt.
WC -
0 RIMIC: nUARTRI modem interrupt clear. Clears the UARTRIINTR interrupt. WC -
UART: UARTDMACR Register
Offset: 0x048
Description
DMA Control Register, UARTDMACR
Table 1042.
UARTDMACR Register
Bits Description Type Reset
31:3 Reserved. - -
2 DMAONERR: DMA on error. If this bit is set to 1, the DMA receive request
outputs, UARTRXDMASREQ or UARTRXDMABREQ, are disabled when the
UART error interrupt is asserted.
RW 0x0
1 TXDMAE: Transmit DMA enable. If this bit is set to 1, DMA for the transmit
FIFO is enabled.
RW 0x0
0 RXDMAE: Receive DMA enable. If this bit is set to 1, DMA for the receive FIFO
is enabled.
RW 0x0
RP2350 Datasheet
12.1. UART 981
UART: UARTPERIPHID0 Register
Offset: 0xfe0
Description
UARTPeriphID0 Register
Table 1043.
UARTPERIPHID0
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 PARTNUMBER0: These bits read back as 0x11 RO 0x11
UART: UARTPERIPHID1 Register
Offset: 0xfe4
Description
UARTPeriphID1 Register
Table 1044.
UARTPERIPHID1
Register
Bits Description Type Reset
31:8 Reserved. - -
7:4 DESIGNER0: These bits read back as 0x1 RO 0x1
3:0 PARTNUMBER1: These bits read back as 0x0 RO 0x0
UART: UARTPERIPHID2 Register
Offset: 0xfe8
Description
UARTPeriphID2 Register
Table 1045.
UARTPERIPHID2
Register
Bits Description Type Reset
31:8 Reserved. - -
7:4 REVISION: This field depends on the revision of the UART: r1p0 0x0 r1p1 0x1
r1p3 0x2 r1p4 0x2 r1p5 0x3
RO 0x3
3:0 DESIGNER1: These bits read back as 0x4 RO 0x4
UART: UARTPERIPHID3 Register
Offset: 0xfec
Description
UARTPeriphID3 Register
Table 1046.
UARTPERIPHID3
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 CONFIGURATION: These bits read back as 0x00 RO 0x00
UART: UARTPCELLID0 Register
Offset: 0xff0
Description
UARTPCellID0 Register
RP2350 Datasheet
12.1. UART 982
Table 1047.
UARTPCELLID0
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID0: These bits read back as 0x0D RO 0x0d
UART: UARTPCELLID1 Register
Offset: 0xff4
Description
UARTPCellID1 Register
Table 1048.
UARTPCELLID1
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID1: These bits read back as 0xF0 RO 0xf0
UART: UARTPCELLID2 Register
Offset: 0xff8
Description
UARTPCellID2 Register
Table 1049.
UARTPCELLID2
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID2: These bits read back as 0x05 RO 0x05
UART: UARTPCELLID3 Register
Offset: 0xffc
Description
UARTPCellID3 Register
Table 1050.
UARTPCELLID3
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID3: These bits read back as 0xB1 RO 0xb112.1. UART
Arm documentation
Excerpted from the PrimeCell UART (PL011) Technical Reference Manual. Used with permission.
RP2350 has 2 identical instances of a UART peripheral, based on the Arm Primecell UART (PL011) (Revision r1p5).
Each instance supports the following features:
• Separate 32×8 TX and 32×12 RX FIFOs
• Programmable baud rate generator, clocked by clk_peri (see Figure 33)
• Standard asynchronous communication bits (start, stop, parity) added on transmit and removed on receive
• Line break detection
• Programmable serial interface (5, 6, 7, or 8 bits)
• 1 or 2 stop bits
• Programmable hardware flow control
Each UART can be connected to a number of GPIO pins as defined in the GPIO muxing table in Section 9.4. Connections
to the GPIO muxing use a prefix including the UART instance name uart0_ or uart1_, and include the following:
• Transmit data tx (referred to as UARTTXD in the following sections)
• Received data rx (referred to as UARTRXD in the following sections)
• Output flow control rts (referred to as nUARTRTS in the following sections)
• Input flow control cts (referred to as nUARTCTS in the following sections)
The modem mode and IrDA mode of the PL011 are not supported.
The UARTCLK is driven from clk_peri, and PCLK is driven from the system clock clk_sys (see Figure 33).
12.1.1. Overview
The UART performs:
• Serial-to-parallel conversion on data received from a peripheral device
• Parallel-to-serial conversion on data transmitted to the peripheral device
The CPU reads and writes data and control/status information through the AMBA APB interface. The transmit and
receive paths are buffered with internal FIFO memories that store up to 32 bytes independently in both transmit and
receive modes.
The UART:
• Includes a programmable baud rate generator that generates a common transmit and receive internal clock from
the UART internal reference clock input, UARTCLK
• Offers similar functionality to the industry-standard 16C650 UART device
• Supports a maximum baud rate of UARTCLK / 16 in UART mode (7.8 Mbaud at 125MHz)
RP2350 Datasheet
12.1. UART 961
The UART operation and baud rate values are controlled by the Line Control Register (UARTLCR_H) and the baud rate
divisor registers: Integer Baud Rate Register (UARTIBRD), and Fractional Baud Rate Register (UARTFBRD).
The UART can generate:
• Individually maskable interrupts from the receive (including timeout), transmit, modem status and error conditions
• A single combined interrupt so that the output is asserted if any of the individual interrupts are asserted and
unmasked
• DMA request signals for interfacing with a Direct Memory Access (DMA) controller
If a framing, parity, or break error occurs during reception, the appropriate error bit is set and stored in the FIFO. If an
overrun condition occurs, the overrun register bit is set immediately and FIFO data is prevented from being overwritten.
You can program the FIFOs to be 1-byte deep providing a conventional double-buffered UART interface.
There is a programmable hardware flow control feature that uses the nUARTCTS input and the nUARTRTS output to
automatically control the serial data flow.
12.1.2. Functional description
Figure 63. UART block
diagram. Test logic is
not shown for clarity.
12.1.2.1. AMBA APB interface
The AMBA APB interface generates read and write decodes for accesses to status/control registers, and the transmit
and receive FIFOs.
12.1.2.2. Register block
The register block stores data written, or to be read across the AMBA APB interface.
RP2350 Datasheet
12.1. UART 962
12.1.2.3. Baud rate generator
The baud rate generator contains free-running counters that generate the internal clocks: Baud16 and IrLPBaud16
signals. Baud16 provides timing information for UART transmit and receive control. Baud16 is a stream of pulses with a
width of one UARTCLK clock period and a frequency of 16 times the baud rate.
12.1.2.4. Transmit FIFO
The transmit FIFO is an 8-bit wide, 32 location deep, FIFO memory buffer. CPU data written across the APB interface is
stored in the FIFO until read out by the transmit logic. When disabled, the transmit FIFO acts like a one byte holding
register.
12.1.2.5. Receive FIFO
The receive FIFO is a 12-bit wide, 32 location deep, FIFO memory buffer. Received data and corresponding error bits are
stored in the receive FIFO by the receive logic until read out by the CPU across the APB interface. When disabled, the
receive FIFO acts like a one byte holding register.
12.1.2.6. Transmit logic
The transmit logic performs parallel-to-serial conversion on the data read from the transmit FIFO. Control logic outputs
the serial bit stream in the following order:
1. Start bit
2. Data bits (Least Significant Bit (LSB) first)
3. Parity bit
4. Stop bits according to the programmed configuration in control registers
12.1.2.7. Receive logic
The receive logic performs serial-to-parallel conversion on the received bit stream after a valid start pulse has been
detected. Receive logic includes overrun, parity, frame error checking, and line break detection; you can find the output
of these checks in the status that accompanies the data written to the receive FIFO.
12.1.2.8. Interrupt generation logic
The UART generates individual maskable active HIGH interrupts to the processor interrupt controllers. To generate
combined interrupts, the UART outputs an OR function of the individual interrupt requests.
For more information, see Section 12.1.6.
12.1.2.9. DMA interface
The UART provides an interface to connect to the DMA controller as a UART DMA; for more information, see Section
12.1.5.
RP2350 Datasheet
12.1. UART 963
12.1.2.10. Synchronizing registers and logic
The UART supports both asynchronous and synchronous operation of the clocks, PCLK and UARTCLK. The UART
implements always-on synchronisation registers and handshaking logic. This has a minimal impact on performance and
area. The UART performs control signal synchronisation on both directions of data flow (from the PCLK to the UARTCLK
domain, and from the UARTCLK to the PCLK domain).
12.1.3. Operation
12.1.3.1. Clock signals
The frequency selected for UARTCLK must accommodate the required range of baud rates:
• FUARTCLK (min) ≥ 16 × baud_rate (max)
• FUARTCLK (max) ≤ 16 × 65535 × baud_rate (min)
For example, for a range of baud rates from 110 baud to 460800 baud the UARTCLK frequency must be between
7.3728MHz to 115.34MHz.
To use all baud rates, the UARTCLK frequency must fall within the required error limits.
There is also a constraint on the ratio of clock frequencies for PCLK to UARTCLK. The frequency of UARTCLK must be no more
than 5/3 times faster than the frequency of PCLK:
• FUARTCLK ≤ 5/3 × FPCLK
For example, in UART mode, to generate 921600 baud when UARTCLK is 14.7456MHz, PCLK must be greater than or equal
to 8.85276MHz. This ensures that the UART has sufficient time to write the received data to the receive FIFO.
12.1.3.2. UART operation
Control data is written to the UART Line Control Register, UARTLCR. This register is 30 bits wide internally, but provides
external access through the APB interface by writes to the following registers:
• UARTLCR_H, which defines the following:
◦
transmission parameters
◦
word length
◦
buffer mode
◦
number of transmitted stop bits
◦
parity mode
◦
break generation
• UARTIBRD, which defines the integer baud rate divider
• UARTFBRD, which defines the fractional baud rate divider
12.1.3.2.1. Fractional baud rate divider
The baud rate divisor is a 22-bit number consisting of a 16-bit integer and a 6-bit fractional part. The baud rate generator
uses the baud rate divisor to determine the bit period. The fractional baud rate divider enables the use of any clock with
a frequency greater than 3.6864MHz to act as UARTCLK, while it is still possible to generate all the standard baud rates.
The 16-bit integer is written to the Integer Baud Rate Register, UARTIBRD. The 6-bit fractional part is written to the
Fractional Baud Rate Register, UARTFBRD. The Baud Rate Divisor has the following relationship to UARTCLK:
RP2350 Datasheet
12.1. UART 964
Baud Rate Divisor = UARTCLK/(16×Baud Rate) = where is the integer part and is the
fractional part separated by a decimal point as shown in Figure 64.
Figure 64. Baud rate
divisor.
To calculate the 6-bit number ( ), multiply the fractional part of the required baud rate divisor by 64 ( , where is the
width of the UARTFBRD register) and add 0.5 to account for rounding errors:
The UART generates an internal clock enable signal, Baud16. This is a stream of UARTCLK-wide pulses with an average
frequency of 16 times the required baud rate. Divide this signal by 16 to give the transmit clock. A low number in the
baud rate divisor produces a short bit period, and a high number in the baud rate divisor produces a long bit period.
12.1.3.2.2. Data transmission or reception
The UART uses two 32-byte FIFOs to store data received and transmitted. The receive FIFO has an extra four bits per
character for status information. For transmission, data is written into the transmit FIFO. If the UART is enabled, it
causes a data frame to start transmitting with the parameters indicated in the Line Control Register, UARTLCR_H. Data
continues to be transmitted until there is no data left in the transmit FIFO. The BUSY signal goes HIGH immediately after
data writes to the transmit FIFO (that is, the FIFO is non-empty) and remains asserted HIGH while data transmits. BUSY
is negated only when the transmit FIFO is empty, and the last character has been transmitted from the shift register,
including the stop bits. BUSY can be asserted HIGH even though the UART might no longer be enabled.
For each sample of data, three readings are taken and the majority value is kept. In the following paragraphs, the middle
sampling point is defined, and one sample is taken either side of it.
When the receiver is idle (UARTRXD continuously 1, in the marking state) and a LOW is detected on the data input (a start
bit has been received), the receive counter, with the clock enabled by Baud16, begins running and data is sampled on
the eighth cycle of that counter in UART mode, or the fourth cycle of the counter in SIR mode to allow for the shorter
logic 0 pulses (half way through a bit period).
The start bit is valid if UARTRXD is still LOW on the eighth cycle of Baud16, otherwise a false start bit is detected and it is
ignored.
If the start bit was valid, successive data bits are sampled on every 16th cycle of Baud16 (that is, one bit period later)
according to the programmed length of the data characters. The parity bit is then checked if parity mode was enabled.
Lastly, a valid stop bit is confirmed if UARTRXD is HIGH, otherwise a framing error has occurred. When a full word is
received, the data is stored in the receive FIFO, with any error bits associated with that word
12.1.3.2.3. Error bits
The receive FIFO stores three error bits in bits 8 (framing), 9 (parity), and 10 (break), each associated with a particular
character. An additional error bit, stored in bit 11 of the receive FIFO, indicates an overrun error.
12.1.3.2.4. Overrun bit
The overrun bit is not associated with the character in the receive FIFO. The overrun error is set when the FIFO is full and
the next character is completely received in the shift register. The data in the shift register is overwritten, but it is not
written into the FIFO. When an empty location becomes available in the FIFO, another character is received and the state
of the overrun bit is copied into the receive FIFO along with the received character. The overrun state is then cleared.
Table 1025 lists the bit functions of the receive FIFO.
RP2350 Datasheet
12.1. UART 965
Table 1025. Receive
FIFO bit functions
FIFO bit Function
11 Overrun indicator
10 Break error
9 Parity error
8 Framing error
7:0 Received data
12.1.3.2.5. Disabling the FIFOs
The bottom entry of the transmit and receive sides of the UART both have the equivalent of a 1-byte holding register.
You can manipulate flags to disable the FIFOs, allowing you to use the bottom entry of the FIFOs as a 1-byte register.
However, this doesn’t physically disable the FIFOs. When using the FIFOs as a 1-byte register, a write to the data register
bypasses the holding register unless the transmit shift register is already in use.
12.1.3.2.6. System and diagnostic loopback testing
To perform loopback testing for UART data, set the Loop Back Enable (LBE) bit to 1 in the Control Register, UARTCR.
Data transmitted on UARTTXD is received on the UARTRXD input.
12.1.3.3. UART character frame
Figure 65. UART
character frame.
12.1.4. UART hardware flow control
The fully-selectable hardware flow control feature enables you to control the serial data flow with the nUARTRTS output
and nUARTCTS input signals. Figure 66 shows how to communicate between two devices using hardware flow control:
Figure 66. Hardware
flow control between
two similar devices.
When the RTS flow control is enabled, nUARTRTS is asserted until the receive FIFO is filled up to the programmed
watermark level. When the CTS flow control is enabled, the transmitter can only transmit data when nUARTCTS is asserted.
The hardware flow control is selectable using the RTSEn and CTSEn bits in the Control Register, UARTCR. Table 1026 shows
how to configure UARTCR register bits to enable RTS and/or CTS.
RP2350 Datasheet
12.1. UART 966
Table 1026. Control
bits to enable and
disable hardware flow
control.
UARTCR register bits
CTSEn RTSEn Description
1 1 Both RTS and CTS flow control
enabled
1 0 Only CTS flow control enabled
0 1 Only RTS flow control enabled
0 0 Both RTS and CTS flow control
disabled
 NOTE
When RTS flow control is enabled, the software cannot use the RTSEn bit in the Control Register (UARTCR) to control
the status of nUARTRTS.
12.1.4.1. RTS flow control
The RTS flow control logic is linked to the programmable receive FIFO watermark levels.
When RTS flow control is disabled, the receive FIFO receives data until full, or no more data is transmitted to it.
When RTS flow control is enabled, the nUARTRTS is asserted until the receive FIFO fills up to the watermark level. When the
receive FIFO reaches the watermark level, the nUARTRTS signal is de-asserted. This indicates that the FIFO has no more
room to receive data. The transmission of data is expected to cease after the current character has been transmitted.
When the receive FIFO drains below the watermark level, the nUARTRTS signal is reasserted.
12.1.4.2. CTS flow control
The CTS flow control logic is linked to the nUARTCTS signal.
When CTS flow control is disabled, the transmitter transmits data until the transmit FIFO is empty.
When CTS flow control is enabled, the transmitter checks the nUARTCTS signal before transmitting each byte. It only
transmits the byte if the nUARTCTS signal is asserted. As long as the transmit FIFO is not empty and nUARTCTS is asserted,
data continues to transmit. If the transmit FIFO is empty and the nUARTCTS signal is asserted, no data is transmitted. If the
nUARTCTS signal is de-asserted during transmission, the transmitter finishes transmitting the current character before
stopping.
12.1.5. UART DMA interface
The UART provides an interface to connect to a DMA controller. The DMA operation of the UART is controlled using the
DMA Control Register, UARTDMACR. The DMA interface includes the following signals:
For receive:
UARTRXDMASREQ
Single character DMA transfer request, asserted by the UART. For receive, one character consists of up to 12 bits.
This signal is asserted when the receive FIFO contains at least one character.
UARTRXDMABREQ
Burst DMA transfer request, asserted by the UART. This signal is asserted when the receive FIFO contains more
characters than the programmed watermark level. You can program the watermark level for each FIFO using the
Interrupt FIFO Level Select Register (UARTIFLS).
RP2350 Datasheet
12.1. UART 967
UARTRXDMACLR
DMA request clear, asserted by a DMA controller to clear the receive request signals. If DMA burst transfer is
requested, the clear signal is asserted during the transfer of the last data in the burst.
For transmit:
UARTTXDMASREQ
Single character DMA transfer request, asserted by the UART. For transmit, one character consists of up to eight
bits. This signal is asserted when there is at least one empty location in the transmit FIFO.
UARTTXDMABREQ
Burst DMA transfer request, asserted by the UART. This signal is asserted when the transmit FIFO contains less
characters than the watermark level. You can program the watermark level for each FIFO using the Interrupt FIFO
Level Select Register (UARTIFLS).
UARTTXDMACLR
DMA request clear, asserted by a DMA controller to clear the transmit request signals. If DMA burst transfer is
requested, the clear signal is asserted during the transfer of the last data in the burst.
The burst transfer and single transfer request signals are not mutually exclusive: they can both be asserted at the same
time. When the receive FIFO exceeds the watermark level, the burst transfer request and the single transfer request
signals are both asserted. When the receive FIFO is below than the watermark level, only the single transfer request
signal is asserted. This is useful in situations where the number of characters left to be received in the stream is less
than a burst.
Consider a scenario where the watermark level is set to four, but 19 characters are left to be received. The DMA
controller then transfers four bursts of four characters and three single transfers to complete the stream.
 NOTE
For the remaining three characters, the UART cannot assert the burst request.
Each request signal remains asserted until the relevant DMACLR signal is asserted. After the request clear signal is deasserted, a request signal can become active again, depending on the conditions described previously. All request
signals are de-asserted if the UART is disabled or the relevant DMA enable bit, TXDMAE or RXDMAE, in the DMA Control
Register, UARTDMACR, is cleared.
If you disable the FIFOs in the UART, it operates in character mode. Character mode limits FIFO transfers to a single
character at a time, so only the DMA single transfer mode can operate. In character mode, only the UARTRXDMASREQ and
UARTTXDMASREQ request signals can be asserted. For information about disabling the FIFOs, see the Line Control Register,
UARTLCR_H.
When the UART is in the FIFO enabled mode, data transfers can use either single or burst transfers depending on the
programmed watermark level and the amount of data in the FIFO. Table 1027 lists the trigger points for UARTRXDMABREQ
and UARTTXDMABREQ, depending on the watermark level, for the transmit and receive FIFOs.
Table 1027. DMA
trigger points for the
transmit and receive
FIFOs.
Watermark level Burst length
Transmit (number of empty
locations)
Receive (number of filled locations)
1/8 28 4
1/4 24 8
1/2 16 16
3/4 8 24
7/8 4 28
In addition, the DMAONERR bit in the DMA Control Register, UARTDMACR, supports the use of the receive error interrupt,
RP2350 Datasheet
12.1. UART 968
UARTEINTR. It enables the DMA receive request outputs, UARTRXDMASREQ or UARTRXDMABREQ, to be masked out when the UART
error interrupt, UARTEINTR, is asserted. The DMA receive request outputs remain inactive until the UARTEINTR is cleared. The
DMA transmit request outputs are unaffected.
Figure 67. DMA
transfer waveforms.
Figure 67 shows the timing diagram for both a single transfer request and a burst transfer request with the appropriate
DMACLR signal. The signals are all synchronous to PCLK. For the sake of clarity it is assumed that there is no
synchronization of the request signals in the DMA controller.
12.1.6. Interrupts
There are eleven maskable interrupts generated in the UART. On RP2350, only the combined interrupt output, UARTINTR, is
connected.
To enable or disable individual interrupts, change the mask bits in the Interrupt Mask Set/Clear Register, UARTIMSC. Set
the appropriate mask bit HIGH to enable the interrupt.
The transmit and receive dataflow interrupts UARTRXINTR and UARTTXINTR have been separated from the status interrupts.
This enables you to use UARTRXINTR and UARTTXINTR to read or write data in response to FIFO trigger levels.
The error interrupt, UARTEINTR, can be triggered when there is an error in the reception of data. A number of error
conditions are possible.
The modem status interrupt, UARTMSINTR, is a combined interrupt of all the individual modem status signals.
The status of the individual interrupt sources can be read either from the Raw Interrupt Status Register, UARTRIS, or from
the Masked Interrupt Status Register, UARTMIS.
12.1.6.1. UARTMSINTR
The modem status interrupt is asserted if any of the modem status signals (nUARTCTS, nUARTDCD, nUARTDSR, and nUARTRI)
change. To clear the modem status interrupt, write a 1 to the bits corresponding to the modem status signals that
generated the interrupt in the Interrupt Clear Register (UARTICR).
12.1.6.2. UARTRXINTR
The receive interrupt changes state when one of the following events occurs:
• The FIFOs are enabled and the receive FIFO reaches the programmed trigger level. This asserts the receive
interrupt HIGH. To clear the receive interrupt, read data from the receive FIFO until it drops below the trigger level.
• The FIFOs are disabled (have a depth of one location) and data is received, thereby filling the receive FIFO. This
asserts the receive interrupt HIGH. To clear the receive interrupt, perform a single read from the receive FIFO.
In both cases, you can also clear the interrupt manually.
12.1.6.3. UARTTXINTR
The transmit interrupt changes state when one of the following events occurs:
• The FIFOs are enabled and the transmit FIFO is equal to or lower than the programmed trigger level. This asserts
the transmit interrupt HIGH. To clear the transmit interrupt, write data to the transmit FIFO until it exceeds the
RP2350 Datasheet
12.1. UART 969
trigger level.
• The FIFOs are disabled (have a depth of one location) and there is no data present in the transmit FIFO. This
asserts the transmit interrupt HIGH. To clear the transmit interrupt, perform a single write to the transmit FIFO.
In both cases, you can also clear the interrupt manually.
To update the transmit FIFO, write data to the transmit FIFO before or after enabling the UART and the interrupts.
 NOTE
The transmit interrupt is based on a transition through a level, rather than on the level itself. When the interrupt and
the UART is enabled before any data is written to the transmit FIFO, the interrupt is not set. The interrupt is only set
after written data leaves the single location of the transmit FIFO and it becomes empty.
12.1.6.4. UARTRTINTR
The receive timeout interrupt is asserted when the receive FIFO is not empty and no more data is received during a 32-
bit period.
The receive timeout interrupt is cleared in the following scenarios:
• the FIFO becomes empty through reading all the data or by reading the holding register
• a 1 is written to the corresponding bit of the Interrupt Clear Register, UARTICR
12.1.6.5. UARTEINTR
The error interrupt is asserted when an error occurs in the reception of data by the UART. The interrupt can be caused
by a number of different error conditions:
• framing
• parity
• break
• overrun
To determine the cause of the interrupt, read the Raw Interrupt Status Register (UARTRIS) or the Masked Interrupt Status
Register (UARTMIS). To clear the interrupt, write to the relevant bits of the Interrupt Clear Register, UARTICR (bits 7 to 10 are
the error clear bits).
## ## 12.1.6.6. UARTINTR
The interrupts are also combined into a single output, that is an OR function of the individual masked sources. You can
connect this output to a system interrupt controller to provide another level of masking on a individual peripheral basis.
The combined UART interrupt is asserted if any of the individual interrupts are asserted and enabled.
## 12.1.7. Programmer’s model
The SDK provides a uart_init function to configure the UART with a particular baud rate. Once the UART is initialised,
the user must configure a GPIO pin as UART_TX and UART_RX. See Section 9.10.1 for more information on selecting a GPIO
function.
To initialise the UART, the uart_init function takes the following steps:
1. De-asserts the reset
RP2350 Datasheet
## 12.1. UART 970
2. Enables clk_peri
3. Sets enable bits in the control register
4. Enables the FIFOs
5. Sets the baud rate divisors
6. Sets the format
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_uart/uart.c Lines 42 - 92
42 uint uart_init(uart_inst_t *uart, uint baudrate) {
43 invalid_params_if(HARDWARE_UART, uart != uart0 && uart != uart1);
44
45 if (uart_clock_get_hz(uart) == 0) {
46 return 0;
47 }
48
49 uart_reset(uart);
50 uart_unreset(uart);
51
52 uart_set_translate_crlf(uart, PICO_UART_DEFAULT_CRLF);
53
54 // Any LCR writes need to take place before enabling the UART
55 uint baud = uart_set_baudrate(uart, baudrate);
56
57 // inline the uart_set_format() call, as we don't need the CR disable/re-enable
58 // protection, and also many people will never call it again, so having
59 // the generic function is not useful, and much bigger than this inlined
60 // code which is only a handful of instructions.
61 //
62 // The UART_UARTLCR_H_FEN_BITS setting is combined as well as it is the same register
63 #ifdef 0
64 uart_set_format(uart, 8, 1, UART_PARITY_NONE);
65 // Enable FIFOs (must be before setting UARTEN, as this is an LCR access)
66 hw_set_bits(&uart_get_hw(uart)->lcr_h, UART_UARTLCR_H_FEN_BITS);
67 #else
68 uint data_bits = 8;
69 uint stop_bits = 1;
70 uint parity = UART_PARITY_NONE;
71 hw_write_masked(&uart_get_hw(uart)->lcr_h,
72 ((data_bits - 5u) << UART_UARTLCR_H_WLEN_LSB) |
73 ((stop_bits - 1u) << UART_UARTLCR_H_STP2_LSB) |
74 (bool_to_bit(parity != UART_PARITY_NONE) << UART_UARTLCR_H_PEN_LSB) |
75 (bool_to_bit(parity == UART_PARITY_EVEN) << UART_UARTLCR_H_EPS_LSB) |
76 UART_UARTLCR_H_FEN_BITS,
77 UART_UARTLCR_H_WLEN_BITS | UART_UARTLCR_H_STP2_BITS |
78 UART_UARTLCR_H_PEN_BITS | UART_UARTLCR_H_EPS_BITS |
79 UART_UARTLCR_H_FEN_BITS);
80 #endif
81
82 // Enable the UART, both TX and RX
83 uart_get_hw(uart)->cr = UART_UARTCR_UARTEN_BITS | UART_UARTCR_TXE_BITS |
  UART_UARTCR_RXE_BITS;
84 // Always enable DREQ signals -- no harm in this if DMA is not listening
85 uart_get_hw(uart)->dmacr = UART_UARTDMACR_TXDMAE_BITS | UART_UARTDMACR_RXDMAE_BITS;
86
87 return baud;
88 }
RP2350 Datasheet
## 12.1. UART 971
## 12.1.7.1. Baud rate calculation
The UART baud rate is derived from dividing clk_peri.
If the required baud rate is 115200 and UARTCLK = 125MHz then:
Baud Rate Divisor = (125 × 106
)/(16 × 115200) ~= 67.817
Therefore, BRDI = 67 and BRDF = 0.817,
Therefore, fractional part, m = integer((0.817 × 64) + 0.5) = 52
Generated baud rate divider = 67 + 52/64 = 67.8125
Generated baud rate = (125 × 106
)/(16 × 67.8125) ~= 115207
Error = (abs(115200 - 115207) / 115200) × 100 ~= 0.006%
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_uart/uart.c Lines 155 - 180
155 uint uart_set_baudrate(uart_inst_t *uart, uint baudrate) {
156 invalid_params_if(HARDWARE_UART, baudrate == 0);
157 uint32_t baud_rate_div = (8 * uart_clock_get_hz(uart) / baudrate) + 1;
158 uint32_t baud_ibrd = baud_rate_div >> 7;
159 uint32_t baud_fbrd;
160
161 if (baud_ibrd == 0) {
162 baud_ibrd = 1;
163 baud_fbrd = 0;
164 } else if (baud_ibrd >= 65535) {
165 baud_ibrd = 65535;
166 baud_fbrd = 0;
167 } else {
168 baud_fbrd = (baud_rate_div & 0x7f) >> 1;
169 }
170
171 uart_get_hw(uart)->ibrd = baud_ibrd;
172 uart_get_hw(uart)->fbrd = baud_fbrd;
173
174 // PL011 needs a (dummy) LCR_H write to latch in the divisors.
175 // We don't want to actually change LCR_H contents here.
176 uart_write_lcr_bits_masked(uart, 0, 0);
177
178 // See datasheet
179 return (4 * uart_clock_get_hz(uart)) / (64 * baud_ibrd + baud_fbrd);
180 }
## 12.1.8. List of registers
The UART0 and UART1 registers start at base addresses of 0x40070000 and 0x40078000 respectively (defined as
UART0_BASE and UART1_BASE in SDK).
Table 1028. List of
UART registers
Offset Name Info
0x000 UARTDR Data Register, UARTDR
0x004 UARTRSR Receive Status Register/Error Clear Register,
UARTRSR/UARTECR
0x018 UARTFR Flag Register, UARTFR
0x020 UARTILPR IrDA Low-Power Counter Register, UARTILPR
RP2350 Datasheet
## 12.1. UART 972
Offset Name Info
0x024 UARTIBRD Integer Baud Rate Register, UARTIBRD
0x028 UARTFBRD Fractional Baud Rate Register, UARTFBRD
0x02c UARTLCR_H Line Control Register, UARTLCR_H
0x030 UARTCR Control Register, UARTCR
0x034 UARTIFLS Interrupt FIFO Level Select Register, UARTIFLS
0x038 UARTIMSC Interrupt Mask Set/Clear Register, UARTIMSC
0x03c UARTRIS Raw Interrupt Status Register, UARTRIS
0x040 UARTMIS Masked Interrupt Status Register, UARTMIS
0x044 UARTICR Interrupt Clear Register, UARTICR
0x048 UARTDMACR DMA Control Register, UARTDMACR
0xfe0 UARTPERIPHID0 UARTPeriphID0 Register
0xfe4 UARTPERIPHID1 UARTPeriphID1 Register
0xfe8 UARTPERIPHID2 UARTPeriphID2 Register
0xfec UARTPERIPHID3 UARTPeriphID3 Register
0xff0 UARTPCELLID0 UARTPCellID0 Register
0xff4 UARTPCELLID1 UARTPCellID1 Register
0xff8 UARTPCELLID2 UARTPCellID2 Register
0xffc UARTPCELLID3 UARTPCellID3 Register
UART: UARTDR Register
Offset: 0x000
Description
Data Register, UARTDR
Table 1029. UARTDR
Register
Bits Description Type Reset
31:12 Reserved. - -
11 OE: Overrun error. This bit is set to 1 if data is received and the receive FIFO is
already full. This is cleared to 0 once there is an empty space in the FIFO and a
new character can be written to it.
RO -
10 BE: Break error. This bit is set to 1 if a break condition was detected, indicating
that the received data input was held LOW for longer than a full-word
transmission time (defined as start, data, parity and stop bits). In FIFO mode,
this error is associated with the character at the top of the FIFO. When a break
occurs, only one 0 character is loaded into the FIFO. The next character is only
enabled after the receive data input goes to a 1 (marking state), and the next
valid start bit is received.
RO -
9 PE: Parity error. When set to 1, it indicates that the parity of the received data
character does not match the parity that the EPS and SPS bits in the Line
Control Register, UARTLCR_H. In FIFO mode, this error is associated with the
character at the top of the FIFO.
RO -
RP2350 Datasheet
## 12.1. UART 973
Bits Description Type Reset
8 FE: Framing error. When set to 1, it indicates that the received character did
not have a valid stop bit (a valid stop bit is 1). In FIFO mode, this error is
associated with the character at the top of the FIFO.
RO -
7:0 DATA: Receive (read) data character. Transmit (write) data character. RWF -
UART: UARTRSR Register
Offset: 0x004
Description
Receive Status Register/Error Clear Register, UARTRSR/UARTECR
Table 1030. UARTRSR
Register
Bits Description Type Reset
31:4 Reserved. - -
3 OE: Overrun error. This bit is set to 1 if data is received and the FIFO is already
full. This bit is cleared to 0 by a write to UARTECR. The FIFO contents remain
valid because no more data is written when the FIFO is full, only the contents
of the shift register are overwritten. The CPU must now read the data, to
empty the FIFO.
WC 0x0
2 BE: Break error. This bit is set to 1 if a break condition was detected, indicating
that the received data input was held LOW for longer than a full-word
transmission time (defined as start, data, parity, and stop bits). This bit is
cleared to 0 after a write to UARTECR. In FIFO mode, this error is associated
with the character at the top of the FIFO. When a break occurs, only one 0
character is loaded into the FIFO. The next character is only enabled after the
receive data input goes to a 1 (marking state) and the next valid start bit is
received.
WC 0x0
1 PE: Parity error. When set to 1, it indicates that the parity of the received data
character does not match the parity that the EPS and SPS bits in the Line
Control Register, UARTLCR_H. This bit is cleared to 0 by a write to UARTECR.
In FIFO mode, this error is associated with the character at the top of the FIFO.
WC 0x0
0 FE: Framing error. When set to 1, it indicates that the received character did
not have a valid stop bit (a valid stop bit is 1). This bit is cleared to 0 by a write
to UARTECR. In FIFO mode, this error is associated with the character at the
top of the FIFO.
WC 0x0
UART: UARTFR Register
Offset: 0x018
Description
Flag Register, UARTFR
Table 1031. UARTFR
Register
Bits Description Type Reset
31:9 Reserved. - -
8 RI: Ring indicator. This bit is the complement of the UART ring indicator,
nUARTRI, modem status input. That is, the bit is 1 when nUARTRI is LOW.
RO -
RP2350 Datasheet
## 12.1. UART 974
Bits Description Type Reset
7 TXFE: Transmit FIFO empty. The meaning of this bit depends on the state of
the FEN bit in the Line Control Register, UARTLCR_H. If the FIFO is disabled,
this bit is set when the transmit holding register is empty. If the FIFO is
enabled, the TXFE bit is set when the transmit FIFO is empty. This bit does not
indicate if there is data in the transmit shift register.
RO 0x1
6 RXFF: Receive FIFO full. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the receive holding register is full. If the FIFO is enabled, the RXFF bit is set
when the receive FIFO is full.
RO 0x0
5 TXFF: Transmit FIFO full. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the transmit holding register is full. If the FIFO is enabled, the TXFF bit is set
when the transmit FIFO is full.
RO 0x0
4 RXFE: Receive FIFO empty. The meaning of this bit depends on the state of the
FEN bit in the UARTLCR_H Register. If the FIFO is disabled, this bit is set when
the receive holding register is empty. If the FIFO is enabled, the RXFE bit is set
when the receive FIFO is empty.
RO 0x1
3 BUSY: UART busy. If this bit is set to 1, the UART is busy transmitting data.
This bit remains set until the complete byte, including all the stop bits, has
been sent from the shift register. This bit is set as soon as the transmit FIFO
becomes non-empty, regardless of whether the UART is enabled or not.
RO 0x0
2 DCD: Data carrier detect. This bit is the complement of the UART data carrier
detect, nUARTDCD, modem status input. That is, the bit is 1 when nUARTDCD
is LOW.
RO -
1 DSR: Data set ready. This bit is the complement of the UART data set ready,
nUARTDSR, modem status input. That is, the bit is 1 when nUARTDSR is LOW.
RO -
0 CTS: Clear to send. This bit is the complement of the UART clear to send,
nUARTCTS, modem status input. That is, the bit is 1 when nUARTCTS is LOW.
RO -
UART: UARTILPR Register
Offset: 0x020
Description
IrDA Low-Power Counter Register, UARTILPR
Table 1032. UARTILPR
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 ILPDVSR: 8-bit low-power divisor value. These bits are cleared to 0 at reset. RW 0x00
UART: UARTIBRD Register
Offset: 0x024
Description
Integer Baud Rate Register, UARTIBRD
Table 1033. UARTIBRD
Register
Bits Description Type Reset
31:16 Reserved. - -
RP2350 Datasheet
## 12.1. UART 975
Bits Description Type Reset
15:0 BAUD_DIVINT: The integer baud rate divisor. These bits are cleared to 0 on
reset.
RW 0x0000
UART: UARTFBRD Register
Offset: 0x028
Description
Fractional Baud Rate Register, UARTFBRD
Table 1034.
UARTFBRD Register
Bits Description Type Reset
31:6 Reserved. - -
5:0 BAUD_DIVFRAC: The fractional baud rate divisor. These bits are cleared to 0
on reset.
RW 0x00
UART: UARTLCR_H Register
Offset: 0x02c
Description
Line Control Register, UARTLCR_H
Table 1035.
UARTLCR_H Register
Bits Description Type Reset
31:8 Reserved. - -
7 SPS: Stick parity select. 0 = stick parity is disabled 1 = either: * if the EPS bit is
0 then the parity bit is transmitted and checked as a 1 * if the EPS bit is 1 then
the parity bit is transmitted and checked as a 0. This bit has no effect when
the PEN bit disables parity checking and generation.
RW 0x0
6:5 WLEN: Word length. These bits indicate the number of data bits transmitted or
received in a frame as follows: b11 = 8 bits b10 = 7 bits b01 = 6 bits b00 = 5
bits.
RW 0x0
4 FEN: Enable FIFOs: 0 = FIFOs are disabled (character mode) that is, the FIFOs
become 1-byte-deep holding registers 1 = transmit and receive FIFO buffers
are enabled (FIFO mode).
RW 0x0
3 STP2: Two stop bits select. If this bit is set to 1, two stop bits are transmitted
at the end of the frame. The receive logic does not check for two stop bits
being received.
RW 0x0
2 EPS: Even parity select. Controls the type of parity the UART uses during
transmission and reception: 0 = odd parity. The UART generates or checks for
an odd number of 1s in the data and parity bits. 1 = even parity. The UART
generates or checks for an even number of 1s in the data and parity bits. This
bit has no effect when the PEN bit disables parity checking and generation.
RW 0x0
1 PEN: Parity enable: 0 = parity is disabled and no parity bit added to the data
frame 1 = parity checking and generation is enabled.
RW 0x0
0 BRK: Send break. If this bit is set to 1, a low-level is continually output on the
UARTTXD output, after completing transmission of the current character. For
the proper execution of the break command, the software must set this bit for
at least two complete frames. For normal use, this bit must be cleared to 0.
RW 0x0
UART: UARTCR Register
RP2350 Datasheet
## 12.1. UART 976
Offset: 0x030
Description
Control Register, UARTCR
Table 1036. UARTCR
Register
Bits Description Type Reset
31:16 Reserved. - -
15 CTSEN: CTS hardware flow control enable. If this bit is set to 1, CTS hardware
flow control is enabled. Data is only transmitted when the nUARTCTS signal is
asserted.
RW 0x0
14 RTSEN: RTS hardware flow control enable. If this bit is set to 1, RTS hardware
flow control is enabled. Data is only requested when there is space in the
receive FIFO for it to be received.
RW 0x0
13 OUT2: This bit is the complement of the UART Out2 (nUARTOut2) modem
status output. That is, when the bit is programmed to a 1, the output is 0. For
DTE this can be used as Ring Indicator (RI).
RW 0x0
12 OUT1: This bit is the complement of the UART Out1 (nUARTOut1) modem
status output. That is, when the bit is programmed to a 1 the output is 0. For
DTE this can be used as Data Carrier Detect (DCD).
RW 0x0
11 RTS: Request to send. This bit is the complement of the UART request to
send, nUARTRTS, modem status output. That is, when the bit is programmed
to a 1 then nUARTRTS is LOW.
RW 0x0
10 DTR: Data transmit ready. This bit is the complement of the UART data
transmit ready, nUARTDTR, modem status output. That is, when the bit is
programmed to a 1 then nUARTDTR is LOW.
RW 0x0
9 RXE: Receive enable. If this bit is set to 1, the receive section of the UART is
enabled. Data reception occurs for either UART signals or SIR signals
depending on the setting of the SIREN bit. When the UART is disabled in the
middle of reception, it completes the current character before stopping.
RW 0x1
8 TXE: Transmit enable. If this bit is set to 1, the transmit section of the UART is
enabled. Data transmission occurs for either UART signals, or SIR signals
depending on the setting of the SIREN bit. When the UART is disabled in the
middle of transmission, it completes the current character before stopping.
RW 0x1
7 LBE: Loopback enable. If this bit is set to 1 and the SIREN bit is set to 1 and
the SIRTEST bit in the Test Control Register, UARTTCR is set to 1, then the
nSIROUT path is inverted, and fed through to the SIRIN path. The SIRTEST bit
in the test register must be set to 1 to override the normal half-duplex SIR
operation. This must be the requirement for accessing the test registers
during normal operation, and SIRTEST must be cleared to 0 when loopback
testing is finished. This feature reduces the amount of external coupling
required during system test. If this bit is set to 1, and the SIRTEST bit is set to
0, the UARTTXD path is fed through to the UARTRXD path. In either SIR mode
or UART mode, when this bit is set, the modem outputs are also fed through to
the modem inputs. This bit is cleared to 0 on reset, to disable loopback.
RW 0x0
6:3 Reserved. - -
RP2350 Datasheet
## 12.1. UART 977
Bits Description Type Reset
2 SIRLP: SIR low-power IrDA mode. This bit selects the IrDA encoding mode. If
this bit is cleared to 0, low-level bits are transmitted as an active high pulse
with a width of 3 / 16th of the bit period. If this bit is set to 1, low-level bits are
transmitted with a pulse width that is 3 times the period of the IrLPBaud16
input signal, regardless of the selected bit rate. Setting this bit uses less
power, but might reduce transmission distances.
RW 0x0
1 SIREN: SIR enable: 0 = IrDA SIR ENDEC is disabled. nSIROUT remains LOW (no
light pulse generated), and signal transitions on SIRIN have no effect. 1 = IrDA
SIR ENDEC is enabled. Data is transmitted and received on nSIROUT and
SIRIN. UARTTXD remains HIGH, in the marking state. Signal transitions on
UARTRXD or modem status inputs have no effect. This bit has no effect if the
UARTEN bit disables the UART.
RW 0x0
0 UARTEN: UART enable: 0 = UART is disabled. If the UART is disabled in the
middle of transmission or reception, it completes the current character before
stopping. 1 = the UART is enabled. Data transmission and reception occurs for
either UART signals or SIR signals depending on the setting of the SIREN bit.
RW 0x0
UART: UARTIFLS Register
Offset: 0x034
Description
Interrupt FIFO Level Select Register, UARTIFLS
Table 1037. UARTIFLS
Register
Bits Description Type Reset
31:6 Reserved. - -
5:3 RXIFLSEL: Receive interrupt FIFO level select. The trigger points for the receive
interrupt are as follows: b000 = Receive FIFO becomes >= 1 / 8 full b001 =
Receive FIFO becomes >= 1 / 4 full b010 = Receive FIFO becomes >= 1 / 2 full
b011 = Receive FIFO becomes >= 3 / 4 full b100 = Receive FIFO becomes >= 7
/ 8 full b101-b111 = reserved.
RW 0x2
2:0 TXIFLSEL: Transmit interrupt FIFO level select. The trigger points for the
transmit interrupt are as follows: b000 = Transmit FIFO becomes <= 1 / 8 full
b001 = Transmit FIFO becomes <= 1 / 4 full b010 = Transmit FIFO becomes <=
1 / 2 full b011 = Transmit FIFO becomes <= 3 / 4 full b100 = Transmit FIFO
becomes <= 7 / 8 full b101-b111 = reserved.
RW 0x2
UART: UARTIMSC Register
Offset: 0x038
Description
Interrupt Mask Set/Clear Register, UARTIMSC
Table 1038.
UARTIMSC Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEIM: Overrun error interrupt mask. A read returns the current mask for the
UARTOEINTR interrupt. On a write of 1, the mask of the UARTOEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
RP2350 Datasheet
## 12.1. UART 978
Bits Description Type Reset
9 BEIM: Break error interrupt mask. A read returns the current mask for the
UARTBEINTR interrupt. On a write of 1, the mask of the UARTBEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
8 PEIM: Parity error interrupt mask. A read returns the current mask for the
UARTPEINTR interrupt. On a write of 1, the mask of the UARTPEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
7 FEIM: Framing error interrupt mask. A read returns the current mask for the
UARTFEINTR interrupt. On a write of 1, the mask of the UARTFEINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
6 RTIM: Receive timeout interrupt mask. A read returns the current mask for the
UARTRTINTR interrupt. On a write of 1, the mask of the UARTRTINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
5 TXIM: Transmit interrupt mask. A read returns the current mask for the
UARTTXINTR interrupt. On a write of 1, the mask of the UARTTXINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
4 RXIM: Receive interrupt mask. A read returns the current mask for the
UARTRXINTR interrupt. On a write of 1, the mask of the UARTRXINTR interrupt
is set. A write of 0 clears the mask.
RW 0x0
3 DSRMIM: nUARTDSR modem interrupt mask. A read returns the current mask
for the UARTDSRINTR interrupt. On a write of 1, the mask of the
UARTDSRINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
2 DCDMIM: nUARTDCD modem interrupt mask. A read returns the current mask
for the UARTDCDINTR interrupt. On a write of 1, the mask of the
UARTDCDINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
1 CTSMIM: nUARTCTS modem interrupt mask. A read returns the current mask
for the UARTCTSINTR interrupt. On a write of 1, the mask of the
UARTCTSINTR interrupt is set. A write of 0 clears the mask.
RW 0x0
0 RIMIM: nUARTRI modem interrupt mask. A read returns the current mask for
the UARTRIINTR interrupt. On a write of 1, the mask of the UARTRIINTR
interrupt is set. A write of 0 clears the mask.
RW 0x0
UART: UARTRIS Register
Offset: 0x03c
Description
Raw Interrupt Status Register, UARTRIS
Table 1039. UARTRIS
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OERIS: Overrun error interrupt status. Returns the raw interrupt state of the
UARTOEINTR interrupt.
RO 0x0
9 BERIS: Break error interrupt status. Returns the raw interrupt state of the
UARTBEINTR interrupt.
RO 0x0
8 PERIS: Parity error interrupt status. Returns the raw interrupt state of the
UARTPEINTR interrupt.
RO 0x0
RP2350 Datasheet
## 12.1. UART 979
Bits Description Type Reset
7 FERIS: Framing error interrupt status. Returns the raw interrupt state of the
UARTFEINTR interrupt.
RO 0x0
6 RTRIS: Receive timeout interrupt status. Returns the raw interrupt state of the
UARTRTINTR interrupt. a
RO 0x0
5 TXRIS: Transmit interrupt status. Returns the raw interrupt state of the
UARTTXINTR interrupt.
RO 0x0
4 RXRIS: Receive interrupt status. Returns the raw interrupt state of the
UARTRXINTR interrupt.
RO 0x0
3 DSRRMIS: nUARTDSR modem interrupt status. Returns the raw interrupt state
of the UARTDSRINTR interrupt.
RO -
2 DCDRMIS: nUARTDCD modem interrupt status. Returns the raw interrupt state
of the UARTDCDINTR interrupt.
RO -
1 CTSRMIS: nUARTCTS modem interrupt status. Returns the raw interrupt state
of the UARTCTSINTR interrupt.
RO -
0 RIRMIS: nUARTRI modem interrupt status. Returns the raw interrupt state of
the UARTRIINTR interrupt.
RO -
UART: UARTMIS Register
Offset: 0x040
Description
Masked Interrupt Status Register, UARTMIS
Table 1040. UARTMIS
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEMIS: Overrun error masked interrupt status. Returns the masked interrupt
state of the UARTOEINTR interrupt.
RO 0x0
9 BEMIS: Break error masked interrupt status. Returns the masked interrupt
state of the UARTBEINTR interrupt.
RO 0x0
8 PEMIS: Parity error masked interrupt status. Returns the masked interrupt
state of the UARTPEINTR interrupt.
RO 0x0
7 FEMIS: Framing error masked interrupt status. Returns the masked interrupt
state of the UARTFEINTR interrupt.
RO 0x0
6 RTMIS: Receive timeout masked interrupt status. Returns the masked
interrupt state of the UARTRTINTR interrupt.
RO 0x0
5 TXMIS: Transmit masked interrupt status. Returns the masked interrupt state
of the UARTTXINTR interrupt.
RO 0x0
4 RXMIS: Receive masked interrupt status. Returns the masked interrupt state
of the UARTRXINTR interrupt.
RO 0x0
3 DSRMMIS: nUARTDSR modem masked interrupt status. Returns the masked
interrupt state of the UARTDSRINTR interrupt.
RO -
2 DCDMMIS: nUARTDCD modem masked interrupt status. Returns the masked
interrupt state of the UARTDCDINTR interrupt.
RO -
RP2350 Datasheet
## 12.1. UART 980
Bits Description Type Reset
1 CTSMMIS: nUARTCTS modem masked interrupt status. Returns the masked
interrupt state of the UARTCTSINTR interrupt.
RO -
0 RIMMIS: nUARTRI modem masked interrupt status. Returns the masked
interrupt state of the UARTRIINTR interrupt.
RO -
UART: UARTICR Register
Offset: 0x044
Description
Interrupt Clear Register, UARTICR
Table 1041. UARTICR
Register
Bits Description Type Reset
31:11 Reserved. - -
10 OEIC: Overrun error interrupt clear. Clears the UARTOEINTR interrupt. WC -
9 BEIC: Break error interrupt clear. Clears the UARTBEINTR interrupt. WC -
8 PEIC: Parity error interrupt clear. Clears the UARTPEINTR interrupt. WC -
7 FEIC: Framing error interrupt clear. Clears the UARTFEINTR interrupt. WC -
6 RTIC: Receive timeout interrupt clear. Clears the UARTRTINTR interrupt. WC -
5 TXIC: Transmit interrupt clear. Clears the UARTTXINTR interrupt. WC -
4 RXIC: Receive interrupt clear. Clears the UARTRXINTR interrupt. WC -
3 DSRMIC: nUARTDSR modem interrupt clear. Clears the UARTDSRINTR
interrupt.
WC -
2 DCDMIC: nUARTDCD modem interrupt clear. Clears the UARTDCDINTR
interrupt.
WC -
1 CTSMIC: nUARTCTS modem interrupt clear. Clears the UARTCTSINTR
interrupt.
WC -
0 RIMIC: nUARTRI modem interrupt clear. Clears the UARTRIINTR interrupt. WC -
UART: UARTDMACR Register
Offset: 0x048
Description
DMA Control Register, UARTDMACR
Table 1042.
UARTDMACR Register
Bits Description Type Reset
31:3 Reserved. - -
2 DMAONERR: DMA on error. If this bit is set to 1, the DMA receive request
outputs, UARTRXDMASREQ or UARTRXDMABREQ, are disabled when the
UART error interrupt is asserted.
RW 0x0
1 TXDMAE: Transmit DMA enable. If this bit is set to 1, DMA for the transmit
FIFO is enabled.
RW 0x0
0 RXDMAE: Receive DMA enable. If this bit is set to 1, DMA for the receive FIFO
is enabled.
RW 0x0
RP2350 Datasheet
## 12.1. UART 981
UART: UARTPERIPHID0 Register
Offset: 0xfe0
Description
UARTPeriphID0 Register
Table 1043.
UARTPERIPHID0
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 PARTNUMBER0: These bits read back as 0x11 RO 0x11
UART: UARTPERIPHID1 Register
Offset: 0xfe4
Description
UARTPeriphID1 Register
Table 1044.
UARTPERIPHID1
Register
Bits Description Type Reset
31:8 Reserved. - -
7:4 DESIGNER0: These bits read back as 0x1 RO 0x1
3:0 PARTNUMBER1: These bits read back as 0x0 RO 0x0
UART: UARTPERIPHID2 Register
Offset: 0xfe8
Description
UARTPeriphID2 Register
Table 1045.
UARTPERIPHID2
Register
Bits Description Type Reset
31:8 Reserved. - -
7:4 REVISION: This field depends on the revision of the UART: r1p0 0x0 r1p1 0x1
r1p3 0x2 r1p4 0x2 r1p5 0x3
RO 0x3
3:0 DESIGNER1: These bits read back as 0x4 RO 0x4
UART: UARTPERIPHID3 Register
Offset: 0xfec
Description
UARTPeriphID3 Register
Table 1046.
UARTPERIPHID3
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 CONFIGURATION: These bits read back as 0x00 RO 0x00
UART: UARTPCELLID0 Register
Offset: 0xff0
Description
UARTPCellID0 Register
RP2350 Datasheet
## 12.1. UART 982
Table 1047.
UARTPCELLID0
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID0: These bits read back as 0x0D RO 0x0d
UART: UARTPCELLID1 Register
Offset: 0xff4
Description
UARTPCellID1 Register
Table 1048.
UARTPCELLID1
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID1: These bits read back as 0xF0 RO 0xf0
UART: UARTPCELLID2 Register
Offset: 0xff8
Description
UARTPCellID2 Register
Table 1049.
UARTPCELLID2
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID2: These bits read back as 0x05 RO 0x05
UART: UARTPCELLID3 Register
Offset: 0xffc
Description
UARTPCellID3 Register
Table 1050.
UARTPCELLID3
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 UARTPCELLID3: These bits read back as 0xB1 RO 0xb1













# 12.6. DMA
The RP2350 Direct Memory Access (DMA) controller performs bulk data transfers on a processor’s behalf. This leaves
processors free to attend to other tasks or enter low-power sleep states. The DMA dual bus manager ports can issue
one read and one write access per cycle. The data throughput is therefore far greater than one of RP2350’s processors.
RP2350 Datasheet
## 12.6. DMA 1094
Control/Status
Registers
Read Address FIFO
Write Address FIFO
Address Generator
AHB5
Read Manager
From
System
Transfer Data FIFO
AHB5
Write Manager
To
System
AHB5
Subordinate
Interface
Figure 122. DMA
Architecture Overview.
The read manager can
read data from some
address every clock
cycle. Likewise, the
write manager can
write to another
address. The address
generator produces
matched pairs of read
and write addresses,
which the managers
consume through the
address FIFOs. The
DMA can run up to 16
transfer sequences
simultaneously,
supervised by
software via the
control and status
registers.
The DMA can perform one read access and one write access, up to 32 bits in size, every clock cycle. There are 16
independent channels, each of which supervises a sequence of bus transfers in one of the following scenarios:
Memory-to-peripheral
a peripheral signals the DMA when it needs more data to transmit. The DMA reads data from an array in RAM or
flash, and writes to the peripheral’s data FIFO.
Peripheral-to-memory
a peripheral signals the DMA when it has received data. The DMA reads this data from the peripheral’s data FIFO,
and writes it to an array in RAM.
Memory-to-memory
the DMA transfers data between two buffers in RAM, as fast as possible.
Each channel has its own control and status registers (CSRs) that software can use to program and monitor the
channel’s progress. When multiple channels are active at the same time, the DMA shares bandwidth evenly between the
channels, with round-robin over all channels that are currently requesting data transfers.
The transfer size can be either 32, 16, or 8 bits. This is configured once for each channel: source transfer size and
destination transfer size are the same. The DMA performs byte lane replication on narrow writes, so byte data is
available in all 4 bytes of the databus, and halfword data in both halfwords.
Channels can be combined in varied ways for more sophisticated behaviour and greater autonomy. For example, one
channel can configure another, loading configuration data from a sequence of control blocks in memory, and the
second can then call back to the first via the CHAIN_TO option when it needs to be reconfigured.
Making the DMA more autonomous means that much less processor supervision is required: overall this allows the
system to do more at once, or to dissipate less power.
## 12.6.1. Changes from RP2040
The following new features have been added:
• Increased the number of DMA channels from 12 to 16.
• Increased the number of shared IRQ outputs from 2 to 4.
• Channels can be assigned to security domains using SECCFG_CH0 through SECCFG_CH15.
• The DMA now filters bus accesses using the built-in memory protection unit (Section ## 12.6.6.3).
• Interrupts can be assigned to security domains using SECCFG_IRQ0 through SECCFG_IRQ3.
• Pacing timers and the CRC sniffer can be assigned to security domains using the SECCFG_MISC register.
• The four most-significant bits of TRANS_COUNT (CH0_TRANS_COUNT) are redefined as the MODE field, which defines
what happens when TRANS_COUNT reaches zero:
RP2350 Datasheet
## 12.6. DMA 1095
◦
This backward-incompatible change reduces the maximum transfers in one sequence from 232-1 to 228-1.
◦ Mode 0x0 has the same behaviour as RP2040, so there is no need to modify software that performs less than
256 million transfers at a time.
◦ Mode 0x1, "trigger self", allows a channel to automatically restart itself after finishing a transfer sequence, in
addition to the usual end-of-sequence actions like raising an interrupt or triggering other channels. This can
be used for example to get periodic interrupts from streaming ring buffer transfers.
◦ Mode 0xf, "endless", allows a channel to run forever: TRANS_COUNT does not decrement.
• New CH0_CTRL_TRIG.INCR_READ_REV and CH0_CTRL_TRIG.INCR_WRITE_REV fields allow addresses to
decrement rather than increment, or to increment by two.
◦
Some existing fields in the CTRL registers, such as CH0_CTRL_TRIG.BUSY, have moved to accommodate the
new fields.
Some existing behaviour has been refined:
• The logic that adjusts values read from WRITE_ADDR and READ_ADDR according to the number of in-flight transfers is
disabled for address-wrapping and non-incrementing transfers (erratum RP2040-E12).
• You can now poll the ABORT register to wait for completion of an aborted channel (erratum RP2040-E13).
• DMA completion actions such as CHAIN_TO are now strictly ordered against the last write completion, so a CHAIN_TO
on a channel whose registers you write to is a well-defined operation.
◦
This enables the use of control blocks that don’t include one of the four trigger register aliases.
◦
Previously, a channel was considered to complete on the first cycle of its last write’s data phase. Now, a
channel is considered to complete on the last cycle of its last write’s data phase. This is usually the same
cycle, but it can be later when the DMA encounters a write data-phase bus stall.
• Previously, the DMA’s internal arbitration logic inserted an idle cycle after completing a round of active high-priority
channels (CH0_CTRL_TRIG.HIGH_PRIORITY), even if there were no active low-priority requests. This reduced DMA
throughput when lightly loaded. This idle cycle has been removed, eliminating lost throughput.
• IRQ assertion latency has been reduced by one cycle.
## 12.6.2. Configuring channels
Each channel has four control/status registers:
• READ_ADDR (CH0_READ_ADDR) is the address of the next memory location to read.
• WRITE_ADDR (CH0_WRITE_ADDR) is the address of the next memory location to write.
• TRANS_COUNT (CH0_TRANS_COUNT) shows the number of transfers remaining in the current transfer sequence and
programs the number of transfers in the next transfer sequence (see Section ## 12.6.2.2).
• CTRL (CH0_CTRL_TRIG) configures all other aspects of the channel’s behaviour, enables/disables the channel, and
provides completion status.
To directly instruct the DMA channel to perform a data transfer, software writes to these four registers, and then
triggers the channel (Section ## 12.6.3). To make the DMA more autonomous, you can also program one DMA channel to
write to another channel’s configuration registers, queueing up many transfer sequences in advance.
All four are live registers; they update their status continuously as the channel progresses.
## 12.6.2.1. Read and write addresses
READ_ADDR and WRITE_ADDR contain the address the channel will next read from, and write to, respectively. These registers
update automatically after each read/write access, incrementing to the next read/write address as required. The size of
the increment varies according to:
RP2350 Datasheet
## 12.6. DMA 1096
• the transfer size: 1, 2 or 4 byte bus accesses as per CH0_CTRL_TRIG.DATA_SIZE
• the increment enable for each address register: CH0_CTRL_TRIG.INCR_READ and CH0_CTRL_TRIG.INCR_WRITE
• the increment direction: CH0_CTRL_TRIG.INCR_READ_REV and CH0_CTRL_TRIG.INCR_WRITE_REV
Software should generally program these registers with new start addresses each time a new transfer sequence starts.
If READ_ADDR and WRITE_ADDR are not reprogrammed, the DMA will use the current values as start addresses for the next
transfer. For example:
• If the address does not increment (e.g. it is the address of a peripheral FIFO), and the next transfer sequence is
to/from that same address, there is no need to write to the register again.
• When transferring to/from a consecutive series of buffers in memory (e.g. scattering and gathering), an address
register will already have incremented to the start of the next buffer at the completion of a transfer.
By not programming all four CSRs for each transfer sequence, software can use shorter interrupt handlers, and more
compact control block formats when used with channel chaining (see register aliases in Section ## 12.6.3.1, chaining in
Section ## 12.6.3.2).
## 12.6.2.1.1. Address alignment
READ_ADDR and WRITE_ADDR must be aligned to the transfer size, specified in CH0_CTRL_TRIG.DATA_SIZE. For 32-bit
transfers, the address must be a multiple of four, and for 16-bit transfers, the address must be a multiple of two.
Software is responsible for correctly aligning addresses written to READ_ADDR and WRITE_ADDR: the DMA does not enforce
alignment.
If software initially writes a correctly aligned address, the address will remain correctly aligned throughout the transfer
sequence, because the DMA always increments READ_ADDR and WRITE_ADDR by a multiple of the transfer size. Specifically, it
increments by transfer size times -1, 0, 1 or 2, depending on the values of CH0_CTRL_TRIG.INCR_READ,
CH0_CTRL_TRIG.INCR_WRITE, CH0_CTRL_TRIG.INCR_READ_REV and CH0_CTRL_TRIG.INCR_WRITE_REV.
The DMA MPU and system-level bus security filters perform protection checks on the lowest byte address of all bytes
transferred on a given cycle (i.e. to the present value of READ_ADDR/WRITE_ADDR). RP2350 memory hardware ensures
unaligned bus accesses do not cause data to be read/written from the other side of a protection boundary. This means
that unaligned access can not be used to violate the memory protection model. Other than this, the result of an
unaligned access is unspecified.
## 12.6.2.2. Transfer count
Reading TRANS_COUNT (CH0_TRANS_COUNT) returns the number of transfers remaining in the current transfer sequence.
This value updates continuously as the channel progresses. Writing to TRANS_COUNT sets the length of the next transfer
sequence. Up to 228-1 transfers can be performed in one sequence (0x0fffffff, approximately 256 million).
Each time the channel starts a new transfer sequence, the most recent value written to TRANS_COUNT is copied to the live
transfer counter, which will then start to decrement again as the new transfer sequence makes progress. For debugging
purposes, the DBG_TCR (TRANS_COUNT reload value) registers display the last value written to each channel’s TRANS_COUNT.
If the channel is triggered multiple times without intervening writes to TRANS_COUNT, it performs the same number of
transfers each time. For example, when chained to, one channel might load a fixed-size control block into another
channel’s CSRs. TRANS_COUNT would be programmed once by software, and then reload automatically every time.
Alternatively, TRANS_COUNT can be written with a new value before starting each transfer sequence. If TRANS_COUNT is the
channel trigger (see Section ## 12.6.3.1), the channel will start immediately, and the value just written will be used, not the
value currently in the reload register.
RP2350 Datasheet
## 12.6. DMA 1097
 NOTE
The TRANS_COUNT is the number of transfers to be performed. The total number of bytes transferred is TRANS_COUNT
times the size of each transfer in bytes, given by CTRL.DATA_SIZE.
## 12.6.2.2.1. Count modes
The four most-significant bits of TRANS_COUNT contain the MODE field (CH0_TRANS_COUNT.MODE), which modifies the
counting behaviour of TRANS_COUNT. Mode 0x0 is the default: TRANS_COUNT decrements once for every bus transfer, and the
channel halts once TRANS_COUNT reaches zero and all in-flight transfers have finished. The value of 0x0 is chosen for
backward-compatibility with RP2040 software, which expects the TRANS_COUNT register to contain a 32-bit count rather
than a 4-bit mode and a 28-bit count. There are few use cases for a finite number of transfers greater than 228, which is
why the four most-significant bits have been reallocated for use with endless transfers.
Mode 0x1, TRIGGER_SELF, behaves the same as mode 0x0, except that rather than halting upon completion, the channel
immediately re-triggers itself. This is equivalent to a trigger performed by any other mechanism (Section ## 12.6.3):
TRANS_COUNT is reloaded, and the channel resumes from the current READ_ADDR and WRITE_ADDR addresses. A completion
interrupt is still raised (if CTRL.IRQ_QUIET is not set) and the specified CHAIN_TO operation is still performed. The main use
for this mode is streaming through SRAM ring buffers, where some action is required at regular intervals, for example
requesting the processor to refill an audio buffer once it is half-empty.
Mode 0xf, ENDLESS, disables the decrement of TRANS_COUNT. This means a channel will generally run indefinitely without
pause, though triggering a channel with a mode of 0xf and a count of 0x0 will result in the channel halting immediately.
All other values are reserved for future use and their effect is unspecified.
## 12.6.2.3. Control/Status
The CTRL register (CH0_CTRL_TRIG) has more, smaller fields than the other 3 registers. Among other things, CTRL is used
to:
• Configure the size of this channel’s data transfers through the DATA_SIZE field. Reads are always the same size as
writes.
• Configure if and how READ_ADDR and WRITE_ADDR increment after each read or write through the INCR_READ,
INCR_READ_REV, INCR_WRITE, INCR_WRITE_REV, RING_SEL, and RING_SIZE fields. Ring transfers are available, where one of the
address pointers wraps at some power-of-2 boundary.
• Select another channel (or none) to trigger when this channel completes through the CHAIN_TO field.
• Select a peripheral data request (DREQ) signal to pace this channel’s transfers, via the TREQ_SEL field.
• See when the channel is idle, using the BUSY flag.
• See if the channel has encountered a bus error in the READ_ERROR and WRITE_ERROR flags, or the combined error status
in the AHB_ERROR flag.
## 12.6.3. Triggering channels
After a channel has been correctly configured, you must trigger it. This instructs the channel to begin scheduling bus
accesses, either paced by a peripheral data request signal (DREQ) or as fast as possible. The following events can
trigger a channel:
• A write to a channel trigger register.
• Completion of another channel whose CHAIN_TO points to this channel.
• A write to the MULTI_CHAN_TRIGGER register (can trigger multiple channels at once).
Each trigger mechanism covers different use cases. For example, trigger registers are simple and efficient when
RP2350 Datasheet
## 12.6. DMA 1098
configuring and starting a channel in an interrupt service routine because the channel is triggered by the last
configuration write. CHAIN_TO allows one channel to callback to another channel, which can then reconfigure the first
channel. MULTI_CHAN_TRIGGER allows software to simply start a channel without touching any of its configuration
registers.
When triggered, the channel sets its CTRL.BUSY flag to indicate it is actively scheduling transfers. This remains set until
the transfer count reaches zero, or the channel is aborted via the CHAN_ABORT register (Section ## 12.6.8.3).
When a channel is already running, indicated by BUSY = 1, it ignores additional triggers. A channel that is disabled (CTRL.EN
is clear) also ignores triggers.
## 12.6.3.1. Aliases and triggers
Table 1145. Control
register aliases. Each
channel has four
control/status
registers. Each
register can be
accessed at multiple
different addresses. In
each naturally-aligned
group of four, all four
registers appear, in
different orders.
Offset +0x0 +0x4 +0x8 +0xc (Trigger)
0x00 (Alias 0) READ_ADDR WRITE_ADDR TRANS_COUNT CTRL_TRIG
0x10 (Alias 1) CTRL READ_ADDR WRITE_ADDR TRANS_COUNT_TRIG
0x20 (Alias 2) CTRL TRANS_COUNT READ_ADDR WRITE_ADDR_TRIG
0x30 (Alias 3) CTRL WRITE_ADDR TRANS_COUNT READ_ADD_TRIG
The four CSRs are aliased multiple times in memory. Each of the four aliases exposes the same four physical registers,
but in a different order. The final register in each alias (at offset +0xc, highlighted) is a trigger register. Writing to the
trigger register starts the channel.
Often, only alias 0 is used, and aliases 1 through 3 can be ignored. To configure and start the channel, write READ_ADDR,
WRITE_ADDR, TRANS_COUNT, and finally CTRL. Since CTRL is the trigger register in alias 0, this starts the channel.
The other aliases allow more compact control block lists when using one channel to configure another, and more
efficient reconfiguration and launch in interrupt handlers:
• Each CSR is a trigger register in one of the aliases:
◦ When gathering fixed-size buffers into a peripheral, the DMA channel can be configured and launched by
writing only READ_ADDR_TRIG.
◦ When scattering from a peripheral to fixed-size buffers, the channel can be configured and launched by
writing only WRITE_ADDR_TRIG.
• Useful combinations of registers appear as naturally-aligned tuples which contain a trigger register. In conjunction
with channel chaining and address wrapping, these implement compressed control block formats, e.g.:
◦
(WRITE_ADDR, TRANS_COUNT_TRIG) for peripheral scatter operations
◦
(TRANS_COUNT, READ_ADDR_TRIG) for peripheral gather operations, or calculating CRCs on a list of buffers
◦
(READ_ADDR, WRITE_ADDR_TRIG) for manipulating fixed-size buffers in memory
Trigger registers do not start the channel if:
• The channel is disabled via CTRL.EN (if the trigger is CTRL, the just-written value of EN is used, not the value currently
in the CTRL register)
• The channel is already running
• The value 0 is written to the trigger register (useful for ending control block chains, see null triggers (Section
## 12.6.3.3))
• The bus access has a security level lower than the channel’s security level (Section ## 12.6.6.1)
RP2350 Datasheet
## 12.6. DMA 1099
## 12.6.3.2. Chaining
When a channel completes, it can name a different channel to immediately be triggered. This can be used as a callback
for the second channel to reconfigure and restart the first.
This feature is configured through the CHAIN_TO field in the channel CTRL register. This 4-bit value selects a channel that
will start when this one finishes. A channel cannot chain to itself. Setting CHAIN_TO to a channel’s own index prevents
chaining.
Chain triggers behave the same as triggers from other sources, such as trigger registers. For example, they cause
TRANS_COUNT to reload, and they are ignored if the targeted channel is already running.
One application for CHAIN_TO is for a channel to request reconfiguration by another channel from a sequence of control
blocks in memory. Channel A is configured to perform a wrapped transfer from memory to channel B’s control registers
(including a trigger register), and channel B is configured to chain back to channel A when it completes each transfer
sequence. This is shown explicitly in the DMA control blocks example (Section ## 12.6.9.2).
Use of the register aliases (Section ## 12.6.3.1) enables compact formats for DMA control blocks: as little as one word, in
some cases.
Another use of chaining is a ping-pong configuration, where two channels each trigger one another. The processor can
respond to the channel completion interrupts and reconfigure each channel after it completes. However, the chained
channel, which has already been configured, starts immediately. In other words, channel configuration and channel
operation are pipelined. This can improve performance dramatically when a usage pattern requires many short transfer
sequences.
The Section ## 12.6.9 goes into more detail on the possibilities of chain triggers in the real world.
## 12.6.3.3. Null triggers and chain interrupts
As mentioned in Section ## 12.6.3.1, writing all-zeroes to a trigger register does not start the channel. This is called a null
trigger, and it has two purposes:
• Cause a halt at the end of an array of control blocks, by appending an all-zeroes block.
• Reduce the number of interrupts generated when using control blocks.
By default, channels generate an interrupt each time they finish a transfer sequence, unless that channel’s IRQ is
masked in INTE0 through INTE3. The rate of interrupts can be excessive, particularly as processor attention is generally
not required while a sequence of control blocks are in progress. However, processor attention is required at the end of a
chain.
The channel CTRL register has a field called IRQ_QUIET. Its default value is 0. When this set to 1, channels generate an
interrupt when they receive a null trigger, but not on normal completion of a transfer sequence. The interrupt is
generated by the channel that receives the trigger.
## 12.6.4. Data request (DREQ)
Peripherals produce or consume data at their own pace. If the DMA transferred data as fast as possible, loss or
corruption of data would ensue. DREQs are a communication channel between peripherals and the DMA that enables
the DMA to pace transfers according to the needs of the peripheral.
The CTRL.TREQ_SEL (transfer request) field selects an external DREQ. It can also be used to select one of the internal
pacing timers, or select no TREQ at all (the transfer proceeds as fast as possible), e.g. for memory-to-memory transfers.
## 12.6.4.1. System DREQ table
DREQ numbers use the following global assignment to peripheral DREQ channels:
RP2350 Datasheet
## 12.6. DMA 1100
Table 1146. DREQs DREQ DREQ Channel DREQ DREQ Channel DREQ DREQ Channel DREQ DREQ Channel
0 DREQ_PIO0_TX0 14 DREQ_PIO1_RX2 28 DREQ_UART0_TX 42 DREQ_PWM_WRAP10
1 DREQ_PIO0_TX1 15 DREQ_PIO1_RX3 29 DREQ_UART0_RX 43 DREQ_PWM_WRAP11
2 DREQ_PIO0_TX2 16 DREQ_PIO2_TX0 30 DREQ_UART1_TX 44 DREQ_I2C0_TX
3 DREQ_PIO0_TX3 17 DREQ_PIO2_TX1 31 DREQ_UART1_RX 45 DREQ_I2C0_RX
4 DREQ_PIO0_RX0 18 DREQ_PIO2_TX2 32 DREQ_PWM_WRAP0 46 DREQ_I2C1_TX
5 DREQ_PIO0_RX1 19 DREQ_PIO2_TX3 33 DREQ_PWM_WRAP1 47 DREQ_I2C1_RX
6 DREQ_PIO0_RX2 20 DREQ_PIO2_RX0 34 DREQ_PWM_WRAP2 48 DREQ_ADC
7 DREQ_PIO0_RX3 21 DREQ_PIO2_RX1 35 DREQ_PWM_WRAP3 49 DREQ_XIP_STREAM
8 DREQ_PIO1_TX0 22 DREQ_PIO2_RX2 36 DREQ_PWM_WRAP4 50 DREQ_XIP_QMITX
9 DREQ_PIO1_TX1 23 DREQ_PIO2_RX3 37 DREQ_PWM_WRAP5 51 DREQ_XIP_QMIRX
10 DREQ_PIO1_TX2 24 DREQ_SPI0_TX 38 DREQ_PWM_WRAP6 52 DREQ_HSTX
11 DREQ_PIO1_TX3 25 DREQ_SPI0_RX 39 DREQ_PWM_WRAP7 53 DREQ_CORESIGHT
12 DREQ_PIO1_RX0 26 DREQ_SPI1_TX 40 DREQ_PWM_WRAP8 54 DREQ_SHA256
13 DREQ_PIO1_RX1 27 DREQ_SPI1_RX 41 DREQ_PWM_WRAP9
## 12.6.4.2. Credit-based DREQ Scheme
The RP2350 DMA is designed for systems where:
• The area and power cost of large peripheral data FIFOs is prohibitive.
• The bandwidth demands of individual peripherals can be high, for example, >50% bus injection rate for short
periods.
• Bus latency is low, but multiple managers can compete for bus access.
In addition, the DMA’s transfer FIFOs and dual-manager-port structure permit multiple accesses to the same peripheral
to be in-flight at once to improve throughput. Choice of DREQ mechanism is therefore critical:
• The traditional "turn on the tap" method can cause overflow if multiple writes are backed up in the TDF. Some
systems solve this by over-provisioning peripheral FIFOs and setting the DREQ threshold below the full level at the
expense of precious area and power.
• The Arm-style single and burst handshake does not permit additional requests to be registered while the current
request is being served. This limits performance when FIFOs are very shallow.
The RP2350 DMA uses a credit-based DREQ mechanism. For each peripheral, the DMA attempts to keep as many
transfers in-flight as the peripheral has capacity for. This enables full bus throughput (1 word per clock) through an 8-
deep peripheral FIFO with no possibility of overflow or underflow in the absence of fabric latency or contention.
For each channel, the DMA maintains a counter. Each 1-clock pulse on the dreq signal increments this counter. When
non-zero, the channel requests a transfer from the DMA’s internal arbiter. The counter decrements when the transfer is
issued to the address FIFOs. At this point the transfer is in flight, but has not yet necessarily completed.
The counter is saturating, and six bits in size. The counter ignores increments at the maximum value or decrements at
zero. The six-bit counter size supports counts up to the depth of any FIFO on RP2350.
RP2350 Datasheet
## 12.6. DMA 1101
clk
0 1 0 1 2
dreq
chan count
chan issue
1
Figure 123. DREQ
counting
The effect is to upper bound the number of in-flight transfers based on the amount of room or data available in the
peripheral FIFO. In the steady state, this gives maximum throughput, but can’t underflow or underflow. This approach
has the following caveats:
• The user must not access a FIFO currently being serviced by the DMA. This causes the channel and peripheral to
become desynchronised, and can cause corruption or loss of data.
• Multiple channels must not be connected to the same DREQ.
## 12.6.5. Interrupts
Each channel can generate interrupts; these can be masked on a per-channel basis using one of the four identical
interrupt enable registers, INTE0 through INTE3. There are three circumstances where a channel raises an interrupt
request:
• On the completion of each transfer sequence, if CTRL.IRQ_QUIET is disabled
• On receiving a null trigger, if CTRL.IRQ_QUIET is enabled
• On a read or write bus error
The masked interrupt status is visible in the INTS registers; there is one bit for each channel. Interrupts are cleared by
writing a bit mask to INTS. One idiom for acknowledging interrupts is to read INTS, then write the same value back, so
only enabled interrupts are cleared.
The RP2350 DMA provides four system IRQs, with independent masking and status registers (e.g. INTE0, INTE1). Any
combination of channel interrupt requests can be routed to each system IRQ, though generally software only routes
each channel interrupt to a single system IRQ. For example:
• Some channels can be given a higher priority in the system interrupt controller, if they have particularly tight timing
requirements.
• In multiprocessor systems, different channel interrupts can be routed independently to different cores.
• When channels are assigned to a mixture of security domains, IRQs can also be assigned, so that software in each
security domain can get interrupts from its own channels.
For debugging purposes, the INTF registers can force any channel interrupt to be asserted, which will cause assertion of
any system IRQs that have that channel interrupt’s enable bit set in their respective INTE registers.
## 12.6.6. Security
RP2350’s processors support partitioning of memory and peripherals into multiple security domains. This partitioning is
extended into the DMA, so that different security contexts can safely use their assigned channels without breaking any
of the security invariants laid out by the processor security model. For example, an Arm processor in the Non-secure
state must not be able to use the DMA to access memory or peripherals owned by Secure software.
The DMA defines four security levels that map onto Arm or RISC-V processor security states:
• 3: SP (secure and privileged)
◦
Equivalent to Arm processors in the Secure, Privileged state
◦
Equivalent to RISC-V processors in Machine mode
• 2: SU (secure and unprivileged)
RP2350 Datasheet
## 12.6. DMA 1102
◦
Equivalent to Arm processors in the Secure, Normal state
• 1: NSP (nonsecure and privileged)
◦
Equivalent to Arm processors in the Non-secure, Privileged state
◦
Equivalent to RISC-V processors in Supervisor mode
• 0: NSU (nonsecure and unprivileged)
◦
Equivalent to Arm processors in the Non-secure, Normal state
◦
Equivalent to RISC-V processors in User mode
So that the DMA can compare different security levels in a consistent way, they are considered ordered, with SP > SU >
NSP > NSU. For example, when we say that a channel requires a minimum of SU to access its registers, this means that
SP and SU are acceptable, and NSP and NSU are not. As a rule, every action has a reaction that is at or below the
security level of the original action, and so the DMA can not be used to escalate accesses to a higher security level.
Software assigns internal DMA resources, like channels, interrupts, pacing timers and the CRC sniffer, to one of the four
possible security levels. These resources are then accessible only at and above that level. Channel assignment in
particular is discussed in Section ## 12.6.6.1.
The DMA memory protection unit (Section ## 12.6.6.3) defines the minimum security level required to access up to eight
programmable address ranges, so that channels of a given security level can not access memory beyond their means.
This MPU is intended to mirror the SRAM and XIP memory protection boundaries configured in the processor SAU or
PMP. In addition to the internal filtering performed by the DMA MPU, accesses are filtered by the system bus according
to the ACCESSCTRL filter rules described in Section 10.6.2.
The combination of these features allows the DMA to be safely shared by software running in different security
domains. If this is not desired, the entire DMA block can instead be assigned wholesale to a single security domain
using the ACCESSCTRL DMA register.
## 12.6.6.1. Channel security assignment
Channels are assigned to security domains using the channel SECCFG registers, SECCFG_CH0 through SECCFG_CH15.
There is one register per channel. Each register contains a 2-bit security level, and a lock bit that prevents that SECCFG
register from being changed once configured. At reset, all channels are assigned to the SP security level, which is the
highest.
The security level of a channel defines:
• The security level of bus transfers performed by this channel, which is checked against both the DMA memory
protection unit and the ACCESSCTRL bus-level filters described in Section 10.6.2.
• The minimum security level required to read or write this channel’s registers; access from a lower level returns a
bus fault.
• The minimum security level that must be defined on a shared IRQ line for that IRQ to be able to observe this
channel’s interrupts (Section ## 12.6.6.2), or for this channel’s interrupt to be set/cleared through that IRQ’s registers.
• The minimum bus security level required to clear this channel’s interrupts through the INTR register.
• Which DREQs a channel can observe: channels assigned to the NSP or NSU security levels can not observe DREQs
of Secure-only peripherals (as defined by the ACCESSCTRL peripheral configuration).
• Which pacing timer TREQs can be observed; pacing timer security levels are configured by SECCFG_MISC and
must be no higher than the channel security level for the channel in order to observe the TREQ.
• Whether the channel is visible to the CRC sniffer; the sniffer’s security level is configured by SECCFG_MISC and
must be no lower than the observed channel’s security level.
• Which channels this channel can trigger with a CHAIN_TO; chaining from lower to higher security levels is not
permitted.
RP2350 Datasheet
## 12.6. DMA 1103
• The minimum bus security level required to trigger this channel with a write to MULTI_CHAN_TRIGGER.
The channel SECCFG registers require privileged writes (SP/NSP), and will generate a bus fault on an attempted
unprivileged write (SU/NSU). Additionally, the S bit (MSB of the security level) and the LOCK bit are writable only by SP,
whilst the P bit (LSB of the security level) is also writable by NSP, if and only if the S bit is clear. Reads are always
allowed: it is always possible to query which channels are assigned to you by reading the channel SECCFG registers.
Each channel SECCFG register can be locked manually by writing a one to the LOCK bit in that register, and will also lock
automatically upon a successful write to one of the channel’s control registers such as CH0_CTRL_TRIG. This
automatic locking avoids any race conditions that can arise from a channel’s security level changing after it has already
started making transfers, or from leaking secure pointers that have been written to its control registers. After a channel
SECCFG register has been locked, it becomes read-only. LOCK bits can be cleared only by a full reset of the DMA block.
SECCFG registers can be written multiple times before being locked, so the full assignment does not have to be known up
front: for example, Secure Arm software can set spare channels to NSP before launching the Non-secure software
context, and Non-secure, Privileged software can then set the remaining channels it does not need to NSU before
returning to the Non-secure, Normal context.
## 12.6.6.2. Interrupt Security Assignment
The RP2350 DMA has four system-level interrupt request lines (IRQs), each of which can be asserted on any
combination of channel interrupts, as defined by the channel masks in the interrupt enable registers INTE0 through
INTE3. Because the timing of interrupts can leak information, and because it is possible to cause software to
malfunction by deliberately manipulating its interrupts, access to the channel interrupt flags must be controlled.
The interrupt security configuration registers, SECCFG_IRQ0 through SECCFG_IRQ3, define the security level for each
interrupt. This is one of the four security levels laid out in Section ## 12.6.6. The security level of an IRQ defines:
• Which channels are visible in this IRQ’s status registers; channels of a level higher than the IRQ’s will read back as
zero.
• Whether a bus access to this IRQ’s control and status registers is permitted; bus accesses below this IRQ’s
security level will return bus faults and have no effect on the DMA.
• Which channels will assert this IRQ; channels of a level higher than this IRQ’s level will not cause the interrupt to
assert, even if relevant INTE bit is set.
• Whether a channel’s interrupt can be cleared through this IRQ’s INTS register, or set through this channel’s INTF
register; the interrupt flags of channels of higher security level than the IRQ can not be set or cleared.
The INTR register is shared between all IRQs, so it does not respect any of the IRQ security levels. Instead, it follows the
security level of the bus access: reads of INTR will return the interrupt flags of all channels at or below the security level
of the bus access (with higher-level channels reading back as zeroes), and writes to INTR have write-one-clear behaviour
on channels which are at or below the security level of the bus access.
## 12.6.6.3. Memory protection unit
The DMA memory protection unit (MPU) monitors the addresses of all read/write transfers performed by the DMA, and
notes the security level of the originating channel. The MPU is configured in advance with a user-defined security
address map, which specifies the minimum security level required to access up to eight dynamically configured regions.
This is one of the four security levels defined in Section ## 12.6.6.
Transfers that fail to meet the minimum security level for their address are shot down before reaching the system bus,
and a bus error is returned to the originating channel. This will be reported as either a read or write bus error in the
channel’s CTRL register, depending on whether it was a read or write address that failed the security check.
The intended use for the DMA MPU is to mirror the security definitions of SRAM and XIP memory from the processor
SAU or PMP. The number of DMA MPU regions is not sufficient for assigning individual peripherals, so the
ACCESSCTRL bus access registers (Section 10.6.2) are provided for this purpose.
RP2350 Datasheet
## 12.6. DMA 1104
Each of the eight MPU regions is configured with a base address, MPU_BAR0 through MPU_BAR7 for each region, and a
limit address, MPU_LAR0 through MPU_LAR7.
MPU regions have a granularity of 32 bytes, so the base/limit addresses are configured by the 27 most-significant bits
of each BAR/LAR register (bits 31:5). Addresses match MPU regions when the 27 most-significant bits of the address are
greater than or equal to the BAR address bits, and less than or equal to the LAR address bits. For example, when
MPU_BAR0 and MPU_LAR0 both have the value 0x10000000, MPU region 0 matches on a 32-byte region extending from
byte address 0x10000000 to 0x1000001f (inclusive). Regions can be enabled or disabled using the LAR.EN bits — if a region is
disabled, it matches no addresses.
The minimum security level required to access each region is defined by the S and P bits in the LSBs of that region’s LAR
register. When an address matches multiple regions, the lowest-numbered region applies. This matches the tie-break
rules for the RISC-V PMP, but is different from the Arm SAU tie-break rules, so care must be taken when mirroring SAU
mappings with overlapping regions. When none of the MPU regions are matched, the security level is defined by the
global MPU_CTRL.S and MPU_CTRL.P bits.
The MPU configuration registers (MPU_CTRL, MPU_BAR0 through MPU_BAR7 and MPU_LAR0 through MPU_LAR7) do
not permit unprivileged access. Bus accesses at the SU and NSU security levels will return a bus fault and have no other
effect.
The MPU registers are also mostly read-only to NSP accesses, with the sole exception being the region P bits which are
NSP-writable if and only if the corresponding region’s S bit is clear. This delegates to Privileged, Non-secure software
the decision of whether Non-secure regions are NSU-accessible.
## 12.6.7. Bus error handling
A bus error is an error condition flagged to one of the DMA’s manager ports in response to an attempted read or write
transfer, indicating the transfer was rejected for one of the following reasons:
• The DMA MPU forbids access to this address at the originating channel’s security level (Section ## 12.6.6.3).
• The bus fabric failed to decode the address; the address did not match any known memory location (for example
SIO is not visible from the DMA bus ports as it is tightly coupled to the processors).
• ACCESSCTRL forbids access to the addressed region at the originating channel’s privilege level (Section 10.6.2).
• ACCESSCTRL forbids DMA access to the addressed region, irrespective of privilege.
• The APB bridge returned a timeout fault for a transfer exceeding 65535 cycles (e.g. accessed ADC whilst clk_adc
was stopped).
• The downstream bus port returned an error response for any other device-specific reason, e.g. attempting to
access configuration registers for a DMA channel with higher security level (Section ## 12.6.6.1).
## 12.6.7.1. Response to bus errors
Upon encountering a bus error, the DMA halts the offending channel and reports the error through the channel’s
CH0_CTRL_TRIG.READ_ERROR and WRITE_ERROR flags. The channel stops scheduling bus accesses.
Bus errors are exceptional events which usually indicate misconfiguration of the DMA or some other system hardware.
Therefore the DMA refuses to restart the offending channel until its error status is cleared by writing 1 to the relevant
error flag. Other channels are not affected, and continue their transfer sequences uninterrupted.
A channel which encounters a bus error does not CHAIN_TO other channels.
Bus errors always cause the channel’s interrupt request to be asserted. Whether or not this causes a system-level IRQ
depends on the channel masks configured in interrupt enable registers INTE0 through INTE3.
RP2350 Datasheet
## 12.6. DMA 1105
## 12.6.7.2. Recovery after bus errors
If an error is reported through READ_ERR/WRITE_ERR then, before restarting the channel, software must:
1. Poll for a low BUSY status to ensure that all in-flight transfers for this channel have been flushed from the DMA’s bus
pipeline.
2. Clear the error flags by writing 1 to each flag.
Generally the BUSY flag will already be low long before the processor enters its interrupt handler and checks the error
status, but it is possible for these events to overlap when the DMA is accessing a slow device such as XIP with a high
SCK divisor and processors are executing from SRAM.
READ_ADDR and WRITE_ADDR contain the approximate address where the bus error was encountered. This can be useful for
the programmer to understand why the bus error occurred, and fix the software to avoid it in future.
Since the DMA performs reads and writes in parallel, it is possible for a channel to encounter both a read and write error
simultaneously, and in this case the DMA sets both READ_ERR and WRITE_ERR. You must clear both.
## 12.6.7.3. Halt timing
The DMA halts the channel as soon as possible following a bus error. This suppresses future reads and writes. Because
the request to access the bus is masked, the bus access has no side effects on the system. The timing relationships are
not straightforward due to the DMA’s pipelining and buffering. The DMA provides the following ordering guarantees
between transfers originating from one channel:
• Read error → read suppression: Any reads scheduled to occur after a faulting read will be suppressed, but can still
increment READ_ADDR up to two times total
• Write error → write suppression: Any writes scheduled to occur after a faulting write will be suppressed, but can
still increment WRITE_ADDR up to four times total
• Read error → write suppression:
◦
Any write paired with a faulting read will be suppressed, but will increment WRITE_ADDR
◦
Any write following the first write paired with a faulting read will be suppressed, but can increment WRITE_ADDR
up to three times total
◦
Up to three writes immediately preceding the first write paired with a faulting read can be suppressed, but will
increment WRITE_ADDR
• Write error → read suppression:
◦
Reads paired with writes before the first faulting write will not be suppressed, and will increment READ_ADDR.
◦
Up to two read transfers paired with writes after the first faulting write can be suppressed, and can increment
READ_ADDR
"Paired with" in the above paragraph refers to the write access which writes data originating from a particular read
transfer, or vice versa. The DMA always schedules read and write accesses in matched pairs.
Slight variability in halt behaviour is due to the buffering of in-flight transfers, and the parallel operation of the read and
write bus ports. The values of READ_ADDR/WRITE_ADDR following a bus error can be slightly beyond the address that
experienced the first error, but the difference is bounded, and usually this is still sufficient to diagnose the reason for the
fault. Additionally, READ_ADDR and WRITE_ADDR are guaranteed to over-increment by the same amount, since reads and
writes are always scheduled in pairs.
In addition to the increments mentioned above, READ_ADDR/WRITE_ADDR always point to the next address to be written, so
always point slightly past the faulting address if address increment is enabled.
RP2350 Datasheet
## 12.6. DMA 1106
## 12.6.8. Additional features
## 12.6.8.1. Pacing timers
These allow transfer of data roughly once every n clk_sys clocks instead of using external peripheral DREQ to trigger
transfers. A fractional (X/Y) divider is used, and will generate a maximum of 1 request per clk_sys cycle.
There are 4 timers available in RP2350. Each DMA channel is able to select any of these in CTRL.TREQ_SEL. There is one
register used to configure the pacing coefficients for each timer, TIMER0 through TIMER3.
Each timer’s security level is defined by a register field in SECCFG_MISC. This defines the minimum bus security level
required to configure that timer (lower levels will get a bus fault), and the minimum channel security level required to
observe that timer’s TREQ.
## 12.6.8.2. CRC calculation
The DMA can watch data from a given channel passing through the data FIFO, and calculate checksums based on this
data. This a purely passive affair: the data is not altered by this hardware, only observed.
The feature is controlled via the SNIFF_CTRL and SNIFF_DATA registers, and can be enabled/disabled per DMA transfer via
the CTRL.SNIFF_EN field.
As this hardware cannot place back-pressure on the FIFO, it must keep up with the DMA’s maximum transfer rate of 32
bits per clock.
The supported checksums are:
• CRC-32, MSB-first and LSB-first
• CRC-16-CCITT, MSB-first and LSB-first
• Simple summation (add to 32-bit accumulator)
• Even parity
The result register is both readable and writable, so that the initial seed value can be set.
Bit/byte manipulations are available on the result, which can aid specific use cases:
• Bit inversion
• Bit reversal
• Byte swap
These manipulations do not affect the CRC calculation, just how the data is presented in the result register.
The sniffer’s security level is configured by the SECCFG_MISC.SNIFF_S and SECCFG_MISC.SNIFF_P bits. This
determines the minimum bus security level required to access the sniffer’s control registers, as well as the maximum
channel security level that the sniffer can observe.
## 12.6.8.3. Channel abort
It is possible for a channel to get into an irrecoverable state. If commanded to transfer more data than a peripheral will
ever request, the channel will never complete. Clearing the CTRL.EN bit pauses the channel, but does not solve the
problem. This should not occur under normal circumstances, but it is important that there is a mechanism to recover
without simply hard-resetting the entire DMA block.
In such a situation, use the CHAN_ABORT register to force the channel to complete early. There is one bit for each
channel. Writing a 1 to the corresponding bit terminates the channel. This clears the transfer counter and forces the
channel into an inactive state.
RP2350 Datasheet
## 12.6. DMA 1107
At the time an abort is triggered, a channel might have bus transfers currently in flight between the read and write
manager. These transfers cannot be revoked. The CTRL.BUSY flag stays high until these transfers complete, and the
channel reaches a safe state. This generally takes only a few cycles. The channel must not be restarted until its
CTRL.BUSY flag de-asserts. Starting a new sequence of transfers whilst transfers from an old sequence are still in flight
will cause unpredictable behaviour.
The sequence to abort one or more channels in an unknown state (also accounting for the behaviour described in
RP2350-E5 is:
1. Clear the EN bit and disable CHAIN_TO for all channels to be aborted.
2. Write the CHAN_ABORT register with a bitmap of those same channels.
3. Poll the ABORT register until all bits set by the previous write are clear.
When aborting a channel involved in a CHAIN_TO, it is recommended to simultaneously abort all other channels involved in
the chain.
## 12.6.8.4. Debug
Debug registers are available for each DMA channel to show the dreq counter DBG_CTDREQ and next transfer count DBG_TCR.
These can also be used to reset a DMA channel if required.
## 12.6.9. Example use cases
## 12.6.9.1. Using interrupts to reconfigure a channel
When a channel finishes a block of transfers, it becomes available for making more transfers. Software detects that the
channel is no longer busy, and reconfigures and restarts the channel. One approach is to poll the CTRL_BUSY bit until the
channel is done, but this loses one of the key advantages of the DMA, namely that it does not have to operate in
lockstep with a processor. By setting the correct bit in INTE0 through INTE3, you can instruct the DMA to raise one of its
four interrupt request lines when a given channel completes. Rather than repeatedly asking if a channel is done, you are
told.
 NOTE
Having four system interrupt lines allows different channel completion interrupts to be routed to different cores, or
to pre-empt one another on the same core if one channel is more time-critical. It also allows channel interrupts to
target different security domains.
When the interrupt is asserted, the processor can be configured to drop whatever it is doing and call a user-specified
handler function. The handler can reconfigure and restart the channel. When the handler exits, the processor returns to
the interrupted code running in the foreground.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/dma/channel_irq/channel_irq.c Lines 35 - 52
35 void dma_handler() {
36 static int pwm_level = 0;
37 static uint32_t wavetable[N_PWM_LEVELS];
38 static bool first_run = true;
39 // Entry number `i` has `i` one bits and `(32 - i)` zero bits.
40 if (first_run) {
41 first_run = false;
42 for (int i = 0; i < N_PWM_LEVELS; ++i)
43 wavetable[i] = ~(~0u << i);
44 }
45
RP2350 Datasheet
## 12.6. DMA 1108
46 // Clear the interrupt request.
47 dma_hw->ints0 = 1u << dma_chan;
48 // Give the channel a new wave table entry to read from, and re-trigger it
49 dma_channel_set_read_addr(dma_chan, &wavetable[pwm_level], true);
50
51 pwm_level = (pwm_level + 1) % N_PWM_LEVELS;
52 }
In many cases, most of the configuration can be done the first time the channel starts. This way, only addresses and
transfer lengths need reprogramming in the interrupt handler.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/dma/channel_irq/channel_irq.c Lines 54 - 94
54 int main() {
55 #ifndef PICO_DEFAULT_LED_PIN
56 #warning dma/channel_irq example requires a board with a regular LED
57 #else
58 // Set up a PIO state machine to serialise our bits
59 uint offset = pio_add_program(pio0, &pio_serialiser_program);
60 pio_serialiser_program_init(pio0, 0, offset, PICO_DEFAULT_LED_PIN, PIO_SERIAL_CLKDIV);
61
62 // Configure a channel to write the same word (32 bits) repeatedly to PIO0
63 // SM0's TX FIFO, paced by the data request signal from that peripheral.
64 dma_chan = dma_claim_unused_channel(true);
65 dma_channel_config c = dma_channel_get_default_config(dma_chan);
66 channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
67 channel_config_set_read_increment(&c, false);
68 channel_config_set_dreq(&c, DREQ_PIO0_TX0);
69
70 dma_channel_configure(
71 dma_chan,
72 &c,
73 &pio0_hw->txf[0], // Write address (only need to set this once)
74 NULL, // Don't provide a read address yet
75 PWM_REPEAT_COUNT, // Write the same value many times, then halt and interrupt
76 false // Don't start yet
77 );
78
79 // Tell the DMA to raise IRQ line 0 when the channel finishes a block
80 dma_channel_set_irq0_enabled(dma_chan, true);
81
82 // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
83 irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
84 irq_set_enabled(DMA_IRQ_0, true);
85
86 // Manually call the handler once, to trigger the first transfer
87 dma_handler();
88
89 // Everything else from this point is interrupt-driven. The processor has
90 // time to sit and think about its early retirement -- maybe open a bakery?
91 while (true)
92 tight_loop_contents();
93 #endif
94 }
One disadvantage of this technique is that you don’t start to reconfigure the channel until some time after the channel
makes its last transfer. If there is heavy interrupt activity on the processor, this can be quite a long time, and quite a
large gap in transfers. This makes it difficult to sustain a high data throughput.
This is solved by using two channels, with their CHAIN_TO fields crossed over, so that channel A triggers channel B when it
completes, and vice versa. At any point in time, one of the channels is transferring data. The other is either already
RP2350 Datasheet
## 12.6. DMA 1109
configured to start the next transfer immediately when the current one finishes, or it is in the process of being
reconfigured. When channel A completes, it immediately starts the cued-up transfer on channel B. At the same time, the
interrupt is fired, and the handler reconfigures channel A so that it is ready when channel B completes.
## 12.6.9.2. DMA control blocks
Frequently, multiple smaller buffers must be gathered together and sent to the same peripheral. To address this use
case, the RP2350 DMA can execute a long and complex sequence of transfers without processor control. One channel
repeatedly reconfigures a second channel, and the second channel restarts the first each time it completes block of
transfers.
Because the first DMA channel transfers data directly from memory to the second channel’s control registers, the
format of the control blocks in memory must match those registers. Each time, the last register written to will be one of
the trigger registers (Section ## 12.6.3.1), which will start the second channel on its programmed block of transfers. The
register aliases (Section ## 12.6.3.1) give some flexibility for the block layout, and more importantly allow some registers
to be omitted from the blocks, so they occupy less memory and can be loaded more quickly.
This example shows how multiple buffers can be gathered and transferred to the UART, by reprogramming TRANS_COUNT
and READ_ADDR_TRIG:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/dma/control_blocks/control_blocks.c
  1 /**
  2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
  3 *
  4 * SPDX-License-Identifier: BSD-3-Clause
  5 */
  6
  7 // Use two DMA channels to make a programmed sequence of data transfers to the
  8 // UART (a data gather operation). One channel is responsible for transferring
  9 // the actual data, the other repeatedly reprograms that channel.
 10
 11 #include <stdio.h>
 12 #include "pico/stdlib.h"
 13 #include "hardware/dma.h"
 14 #include "hardware/structs/uart.h"
 15
 16 // These buffers will be DMA'd to the UART, one after the other.
 17
 18 const char word0[] = "Transferring ";
 19 const char word1[] = "one ";
 20 const char word2[] = "word ";
 21 const char word3[] = "at ";
 22 const char word4[] = "a ";
 23 const char word5[] = "time.\n";
 24
 25 // Note the order of the fields here: it's important that the length is before
 26 // the read address, because the control channel is going to write to the last
 27 // two registers in alias 3 on the data channel:
 28 // +0x0 +0x4 +0x8 +0xC (Trigger)
 29 // Alias 0: READ_ADDR WRITE_ADDR TRANS_COUNT CTRL
 30 // Alias 1: CTRL READ_ADDR WRITE_ADDR TRANS_COUNT
 31 // Alias 2: CTRL TRANS_COUNT READ_ADDR WRITE_ADDR
 32 // Alias 3: CTRL WRITE_ADDR TRANS_COUNT READ_ADDR
 33 //
 34 // This will program the transfer count and read address of the data channel,
 35 // and trigger it. Once the data channel completes, it will restart the
 36 // control channel (via CHAIN_TO) to load the next two words into its control
 37 // registers.
 38
 39 const struct {uint32_t len; const char *data;} control_blocks[] = {
RP2350 Datasheet
## 12.6. DMA 1110
 40 {count_of(word0) - 1, word0}, // Skip null terminator
 41 {count_of(word1) - 1, word1},
 42 {count_of(word2) - 1, word2},
 43 {count_of(word3) - 1, word3},
 44 {count_of(word4) - 1, word4},
 45 {count_of(word5) - 1, word5},
 46 {0, NULL} // Null trigger to end chain.
 47 };
 48
 49 int main() {
 50 #ifndef uart_default
 51 #warning dma/control_blocks example requires a UART
 52 #else
 53 stdio_init_all();
 54 puts("DMA control block example:");
 55
 56 // ctrl_chan loads control blocks into data_chan, which executes them.
 57 int ctrl_chan = dma_claim_unused_channel(true);
 58 int data_chan = dma_claim_unused_channel(true);
 59
 60 // The control channel transfers two words into the data channel's control
 61 // registers, then halts. The write address wraps on a two-word
 62 // (eight-byte) boundary, so that the control channel writes the same two
 63 // registers when it is next triggered.
 64
 65 dma_channel_config c = dma_channel_get_default_config(ctrl_chan);
 66 channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
 67 channel_config_set_read_increment(&c, true);
 68 channel_config_set_write_increment(&c, true);
 69 channel_config_set_ring(&c, true, 3); // 1 << 3 byte boundary on write ptr
 70
 71 dma_channel_configure(
 72 ctrl_chan,
 73 &c,
 74 &dma_hw->ch[data_chan].al3_transfer_count, // Initial write address
 75 &control_blocks[0], // Initial read address
 76 2, // Halt after each control block
 77 false // Don't start yet
 78 );
 79
 80 // The data channel is set up to write to the UART FIFO (paced by the
 81 // UART's TX data request signal) and then chain to the control channel
 82 // once it completes. The control channel programs a new read address and
 83 // data length, and retriggers the data channel.
 84
 85 c = dma_channel_get_default_config(data_chan);
 86 channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
 87 channel_config_set_dreq(&c, uart_get_dreq(uart_default, true));
 88 // Trigger ctrl_chan when data_chan completes
 89 channel_config_set_chain_to(&c, ctrl_chan);
 90 // Raise the IRQ flag when 0 is written to a trigger register (end of chain):
 91 channel_config_set_irq_quiet(&c, true);
 92
 93 dma_channel_configure(
 94 data_chan,
 95 &c,
 96 &uart_get_hw(uart_default)->dr,
 97 NULL, // Initial read address and transfer count are unimportant;
 98 0, // the control channel will reprogram them each time.
 99 false // Don't start yet.
100 );
101
102 // Everything is ready to go. Tell the control channel to load the first
103 // control block. Everything is automatic from here.
RP2350 Datasheet
## 12.6. DMA 1111
104 dma_start_channel_mask(1u << ctrl_chan);
105
106 // The data channel will assert its IRQ flag when it gets a null trigger,
107 // indicating the end of the control block list. We're just going to wait
108 // for the IRQ flag instead of setting up an interrupt handler.
109 while (!(dma_hw->intr & 1u << data_chan))
110 tight_loop_contents();
111 dma_hw->ints0 = 1u << data_chan;
112
113 puts("DMA finished.");
114 #endif
115 }
## 12.6.10. List of Registers
The DMA registers start at a base address of 0x50000000 (defined as DMA_BASE in SDK).
Table 1147. List of
DMA registers
Offset Name Info
0x000 CH0_READ_ADDR DMA Channel 0 Read Address pointer
0x004 CH0_WRITE_ADDR DMA Channel 0 Write Address pointer
0x008 CH0_TRANS_COUNT DMA Channel 0 Transfer Count
0x00c CH0_CTRL_TRIG DMA Channel 0 Control and Status
0x010 CH0_AL1_CTRL Alias for channel 0 CTRL register
0x014 CH0_AL1_READ_ADDR Alias for channel 0 READ_ADDR register
0x018 CH0_AL1_WRITE_ADDR Alias for channel 0 WRITE_ADDR register
0x01c CH0_AL1_TRANS_COUNT_TRIG Alias for channel 0 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x020 CH0_AL2_CTRL Alias for channel 0 CTRL register
0x024 CH0_AL2_TRANS_COUNT Alias for channel 0 TRANS_COUNT register
0x028 CH0_AL2_READ_ADDR Alias for channel 0 READ_ADDR register
0x02c CH0_AL2_WRITE_ADDR_TRIG Alias for channel 0 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x030 CH0_AL3_CTRL Alias for channel 0 CTRL register
0x034 CH0_AL3_WRITE_ADDR Alias for channel 0 WRITE_ADDR register
0x038 CH0_AL3_TRANS_COUNT Alias for channel 0 TRANS_COUNT register
0x03c CH0_AL3_READ_ADDR_TRIG Alias for channel 0 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x040 CH1_READ_ADDR DMA Channel 1 Read Address pointer
0x044 CH1_WRITE_ADDR DMA Channel 1 Write Address pointer
0x048 CH1_TRANS_COUNT DMA Channel 1 Transfer Count
0x04c CH1_CTRL_TRIG DMA Channel 1 Control and Status
RP2350 Datasheet
## 12.6. DMA 1112
Offset Name Info
0x050 CH1_AL1_CTRL Alias for channel 1 CTRL register
0x054 CH1_AL1_READ_ADDR Alias for channel 1 READ_ADDR register
0x058 CH1_AL1_WRITE_ADDR Alias for channel 1 WRITE_ADDR register
0x05c CH1_AL1_TRANS_COUNT_TRIG Alias for channel 1 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x060 CH1_AL2_CTRL Alias for channel 1 CTRL register
0x064 CH1_AL2_TRANS_COUNT Alias for channel 1 TRANS_COUNT register
0x068 CH1_AL2_READ_ADDR Alias for channel 1 READ_ADDR register
0x06c CH1_AL2_WRITE_ADDR_TRIG Alias for channel 1 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x070 CH1_AL3_CTRL Alias for channel 1 CTRL register
0x074 CH1_AL3_WRITE_ADDR Alias for channel 1 WRITE_ADDR register
0x078 CH1_AL3_TRANS_COUNT Alias for channel 1 TRANS_COUNT register
0x07c CH1_AL3_READ_ADDR_TRIG Alias for channel 1 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x080 CH2_READ_ADDR DMA Channel 2 Read Address pointer
0x084 CH2_WRITE_ADDR DMA Channel 2 Write Address pointer
0x088 CH2_TRANS_COUNT DMA Channel 2 Transfer Count
0x08c CH2_CTRL_TRIG DMA Channel 2 Control and Status
0x090 CH2_AL1_CTRL Alias for channel 2 CTRL register
0x094 CH2_AL1_READ_ADDR Alias for channel 2 READ_ADDR register
0x098 CH2_AL1_WRITE_ADDR Alias for channel 2 WRITE_ADDR register
0x09c CH2_AL1_TRANS_COUNT_TRIG Alias for channel 2 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x0a0 CH2_AL2_CTRL Alias for channel 2 CTRL register
0x0a4 CH2_AL2_TRANS_COUNT Alias for channel 2 TRANS_COUNT register
0x0a8 CH2_AL2_READ_ADDR Alias for channel 2 READ_ADDR register
0x0ac CH2_AL2_WRITE_ADDR_TRIG Alias for channel 2 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x0b0 CH2_AL3_CTRL Alias for channel 2 CTRL register
0x0b4 CH2_AL3_WRITE_ADDR Alias for channel 2 WRITE_ADDR register
0x0b8 CH2_AL3_TRANS_COUNT Alias for channel 2 TRANS_COUNT register
0x0bc CH2_AL3_READ_ADDR_TRIG Alias for channel 2 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
RP2350 Datasheet
## 12.6. DMA 1113
Offset Name Info
0x0c0 CH3_READ_ADDR DMA Channel 3 Read Address pointer
0x0c4 CH3_WRITE_ADDR DMA Channel 3 Write Address pointer
0x0c8 CH3_TRANS_COUNT DMA Channel 3 Transfer Count
0x0cc CH3_CTRL_TRIG DMA Channel 3 Control and Status
0x0d0 CH3_AL1_CTRL Alias for channel 3 CTRL register
0x0d4 CH3_AL1_READ_ADDR Alias for channel 3 READ_ADDR register
0x0d8 CH3_AL1_WRITE_ADDR Alias for channel 3 WRITE_ADDR register
0x0dc CH3_AL1_TRANS_COUNT_TRIG Alias for channel 3 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x0e0 CH3_AL2_CTRL Alias for channel 3 CTRL register
0x0e4 CH3_AL2_TRANS_COUNT Alias for channel 3 TRANS_COUNT register
0x0e8 CH3_AL2_READ_ADDR Alias for channel 3 READ_ADDR register
0x0ec CH3_AL2_WRITE_ADDR_TRIG Alias for channel 3 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x0f0 CH3_AL3_CTRL Alias for channel 3 CTRL register
0x0f4 CH3_AL3_WRITE_ADDR Alias for channel 3 WRITE_ADDR register
0x0f8 CH3_AL3_TRANS_COUNT Alias for channel 3 TRANS_COUNT register
0x0fc CH3_AL3_READ_ADDR_TRIG Alias for channel 3 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x100 CH4_READ_ADDR DMA Channel 4 Read Address pointer
0x104 CH4_WRITE_ADDR DMA Channel 4 Write Address pointer
0x108 CH4_TRANS_COUNT DMA Channel 4 Transfer Count
0x10c CH4_CTRL_TRIG DMA Channel 4 Control and Status
0x110 CH4_AL1_CTRL Alias for channel 4 CTRL register
0x114 CH4_AL1_READ_ADDR Alias for channel 4 READ_ADDR register
0x118 CH4_AL1_WRITE_ADDR Alias for channel 4 WRITE_ADDR register
0x11c CH4_AL1_TRANS_COUNT_TRIG Alias for channel 4 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x120 CH4_AL2_CTRL Alias for channel 4 CTRL register
0x124 CH4_AL2_TRANS_COUNT Alias for channel 4 TRANS_COUNT register
0x128 CH4_AL2_READ_ADDR Alias for channel 4 READ_ADDR register
0x12c CH4_AL2_WRITE_ADDR_TRIG Alias for channel 4 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x130 CH4_AL3_CTRL Alias for channel 4 CTRL register
RP2350 Datasheet
## 12.6. DMA 1114
Offset Name Info
0x134 CH4_AL3_WRITE_ADDR Alias for channel 4 WRITE_ADDR register
0x138 CH4_AL3_TRANS_COUNT Alias for channel 4 TRANS_COUNT register
0x13c CH4_AL3_READ_ADDR_TRIG Alias for channel 4 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x140 CH5_READ_ADDR DMA Channel 5 Read Address pointer
0x144 CH5_WRITE_ADDR DMA Channel 5 Write Address pointer
0x148 CH5_TRANS_COUNT DMA Channel 5 Transfer Count
0x14c CH5_CTRL_TRIG DMA Channel 5 Control and Status
0x150 CH5_AL1_CTRL Alias for channel 5 CTRL register
0x154 CH5_AL1_READ_ADDR Alias for channel 5 READ_ADDR register
0x158 CH5_AL1_WRITE_ADDR Alias for channel 5 WRITE_ADDR register
0x15c CH5_AL1_TRANS_COUNT_TRIG Alias for channel 5 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x160 CH5_AL2_CTRL Alias for channel 5 CTRL register
0x164 CH5_AL2_TRANS_COUNT Alias for channel 5 TRANS_COUNT register
0x168 CH5_AL2_READ_ADDR Alias for channel 5 READ_ADDR register
0x16c CH5_AL2_WRITE_ADDR_TRIG Alias for channel 5 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x170 CH5_AL3_CTRL Alias for channel 5 CTRL register
0x174 CH5_AL3_WRITE_ADDR Alias for channel 5 WRITE_ADDR register
0x178 CH5_AL3_TRANS_COUNT Alias for channel 5 TRANS_COUNT register
0x17c CH5_AL3_READ_ADDR_TRIG Alias for channel 5 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x180 CH6_READ_ADDR DMA Channel 6 Read Address pointer
0x184 CH6_WRITE_ADDR DMA Channel 6 Write Address pointer
0x188 CH6_TRANS_COUNT DMA Channel 6 Transfer Count
0x18c CH6_CTRL_TRIG DMA Channel 6 Control and Status
0x190 CH6_AL1_CTRL Alias for channel 6 CTRL register
0x194 CH6_AL1_READ_ADDR Alias for channel 6 READ_ADDR register
0x198 CH6_AL1_WRITE_ADDR Alias for channel 6 WRITE_ADDR register
0x19c CH6_AL1_TRANS_COUNT_TRIG Alias for channel 6 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x1a0 CH6_AL2_CTRL Alias for channel 6 CTRL register
0x1a4 CH6_AL2_TRANS_COUNT Alias for channel 6 TRANS_COUNT register
RP2350 Datasheet
## 12.6. DMA 1115
Offset Name Info
0x1a8 CH6_AL2_READ_ADDR Alias for channel 6 READ_ADDR register
0x1ac CH6_AL2_WRITE_ADDR_TRIG Alias for channel 6 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x1b0 CH6_AL3_CTRL Alias for channel 6 CTRL register
0x1b4 CH6_AL3_WRITE_ADDR Alias for channel 6 WRITE_ADDR register
0x1b8 CH6_AL3_TRANS_COUNT Alias for channel 6 TRANS_COUNT register
0x1bc CH6_AL3_READ_ADDR_TRIG Alias for channel 6 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x1c0 CH7_READ_ADDR DMA Channel 7 Read Address pointer
0x1c4 CH7_WRITE_ADDR DMA Channel 7 Write Address pointer
0x1c8 CH7_TRANS_COUNT DMA Channel 7 Transfer Count
0x1cc CH7_CTRL_TRIG DMA Channel 7 Control and Status
0x1d0 CH7_AL1_CTRL Alias for channel 7 CTRL register
0x1d4 CH7_AL1_READ_ADDR Alias for channel 7 READ_ADDR register
0x1d8 CH7_AL1_WRITE_ADDR Alias for channel 7 WRITE_ADDR register
0x1dc CH7_AL1_TRANS_COUNT_TRIG Alias for channel 7 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x1e0 CH7_AL2_CTRL Alias for channel 7 CTRL register
0x1e4 CH7_AL2_TRANS_COUNT Alias for channel 7 TRANS_COUNT register
0x1e8 CH7_AL2_READ_ADDR Alias for channel 7 READ_ADDR register
0x1ec CH7_AL2_WRITE_ADDR_TRIG Alias for channel 7 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x1f0 CH7_AL3_CTRL Alias for channel 7 CTRL register
0x1f4 CH7_AL3_WRITE_ADDR Alias for channel 7 WRITE_ADDR register
0x1f8 CH7_AL3_TRANS_COUNT Alias for channel 7 TRANS_COUNT register
0x1fc CH7_AL3_READ_ADDR_TRIG Alias for channel 7 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x200 CH8_READ_ADDR DMA Channel 8 Read Address pointer
0x204 CH8_WRITE_ADDR DMA Channel 8 Write Address pointer
0x208 CH8_TRANS_COUNT DMA Channel 8 Transfer Count
0x20c CH8_CTRL_TRIG DMA Channel 8 Control and Status
0x210 CH8_AL1_CTRL Alias for channel 8 CTRL register
0x214 CH8_AL1_READ_ADDR Alias for channel 8 READ_ADDR register
0x218 CH8_AL1_WRITE_ADDR Alias for channel 8 WRITE_ADDR register
RP2350 Datasheet
## 12.6. DMA 1116
Offset Name Info
0x21c CH8_AL1_TRANS_COUNT_TRIG Alias for channel 8 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x220 CH8_AL2_CTRL Alias for channel 8 CTRL register
0x224 CH8_AL2_TRANS_COUNT Alias for channel 8 TRANS_COUNT register
0x228 CH8_AL2_READ_ADDR Alias for channel 8 READ_ADDR register
0x22c CH8_AL2_WRITE_ADDR_TRIG Alias for channel 8 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x230 CH8_AL3_CTRL Alias for channel 8 CTRL register
0x234 CH8_AL3_WRITE_ADDR Alias for channel 8 WRITE_ADDR register
0x238 CH8_AL3_TRANS_COUNT Alias for channel 8 TRANS_COUNT register
0x23c CH8_AL3_READ_ADDR_TRIG Alias for channel 8 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x240 CH9_READ_ADDR DMA Channel 9 Read Address pointer
0x244 CH9_WRITE_ADDR DMA Channel 9 Write Address pointer
0x248 CH9_TRANS_COUNT DMA Channel 9 Transfer Count
0x24c CH9_CTRL_TRIG DMA Channel 9 Control and Status
0x250 CH9_AL1_CTRL Alias for channel 9 CTRL register
0x254 CH9_AL1_READ_ADDR Alias for channel 9 READ_ADDR register
0x258 CH9_AL1_WRITE_ADDR Alias for channel 9 WRITE_ADDR register
0x25c CH9_AL1_TRANS_COUNT_TRIG Alias for channel 9 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x260 CH9_AL2_CTRL Alias for channel 9 CTRL register
0x264 CH9_AL2_TRANS_COUNT Alias for channel 9 TRANS_COUNT register
0x268 CH9_AL2_READ_ADDR Alias for channel 9 READ_ADDR register
0x26c CH9_AL2_WRITE_ADDR_TRIG Alias for channel 9 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x270 CH9_AL3_CTRL Alias for channel 9 CTRL register
0x274 CH9_AL3_WRITE_ADDR Alias for channel 9 WRITE_ADDR register
0x278 CH9_AL3_TRANS_COUNT Alias for channel 9 TRANS_COUNT register
0x27c CH9_AL3_READ_ADDR_TRIG Alias for channel 9 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x280 CH10_READ_ADDR DMA Channel 10 Read Address pointer
0x284 CH10_WRITE_ADDR DMA Channel 10 Write Address pointer
0x288 CH10_TRANS_COUNT DMA Channel 10 Transfer Count
RP2350 Datasheet
## 12.6. DMA 1117
Offset Name Info
0x28c CH10_CTRL_TRIG DMA Channel 10 Control and Status
0x290 CH10_AL1_CTRL Alias for channel 10 CTRL register
0x294 CH10_AL1_READ_ADDR Alias for channel 10 READ_ADDR register
0x298 CH10_AL1_WRITE_ADDR Alias for channel 10 WRITE_ADDR register
0x29c CH10_AL1_TRANS_COUNT_TRIG Alias for channel 10 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x2a0 CH10_AL2_CTRL Alias for channel 10 CTRL register
0x2a4 CH10_AL2_TRANS_COUNT Alias for channel 10 TRANS_COUNT register
0x2a8 CH10_AL2_READ_ADDR Alias for channel 10 READ_ADDR register
0x2ac CH10_AL2_WRITE_ADDR_TRIG Alias for channel 10 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x2b0 CH10_AL3_CTRL Alias for channel 10 CTRL register
0x2b4 CH10_AL3_WRITE_ADDR Alias for channel 10 WRITE_ADDR register
0x2b8 CH10_AL3_TRANS_COUNT Alias for channel 10 TRANS_COUNT register
0x2bc CH10_AL3_READ_ADDR_TRIG Alias for channel 10 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x2c0 CH11_READ_ADDR DMA Channel 11 Read Address pointer
0x2c4 CH11_WRITE_ADDR DMA Channel 11 Write Address pointer
0x2c8 CH11_TRANS_COUNT DMA Channel 11 Transfer Count
0x2cc CH11_CTRL_TRIG DMA Channel 11 Control and Status
0x2d0 CH11_AL1_CTRL Alias for channel 11 CTRL register
0x2d4 CH11_AL1_READ_ADDR Alias for channel 11 READ_ADDR register
0x2d8 CH11_AL1_WRITE_ADDR Alias for channel 11 WRITE_ADDR register
0x2dc CH11_AL1_TRANS_COUNT_TRIG Alias for channel 11 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x2e0 CH11_AL2_CTRL Alias for channel 11 CTRL register
0x2e4 CH11_AL2_TRANS_COUNT Alias for channel 11 TRANS_COUNT register
0x2e8 CH11_AL2_READ_ADDR Alias for channel 11 READ_ADDR register
0x2ec CH11_AL2_WRITE_ADDR_TRIG Alias for channel 11 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x2f0 CH11_AL3_CTRL Alias for channel 11 CTRL register
0x2f4 CH11_AL3_WRITE_ADDR Alias for channel 11 WRITE_ADDR register
0x2f8 CH11_AL3_TRANS_COUNT Alias for channel 11 TRANS_COUNT register
RP2350 Datasheet
## 12.6. DMA 1118
Offset Name Info
0x2fc CH11_AL3_READ_ADDR_TRIG Alias for channel 11 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x300 CH12_READ_ADDR DMA Channel 12 Read Address pointer
0x304 CH12_WRITE_ADDR DMA Channel 12 Write Address pointer
0x308 CH12_TRANS_COUNT DMA Channel 12 Transfer Count
0x30c CH12_CTRL_TRIG DMA Channel 12 Control and Status
0x310 CH12_AL1_CTRL Alias for channel 12 CTRL register
0x314 CH12_AL1_READ_ADDR Alias for channel 12 READ_ADDR register
0x318 CH12_AL1_WRITE_ADDR Alias for channel 12 WRITE_ADDR register
0x31c CH12_AL1_TRANS_COUNT_TRIG Alias for channel 12 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x320 CH12_AL2_CTRL Alias for channel 12 CTRL register
0x324 CH12_AL2_TRANS_COUNT Alias for channel 12 TRANS_COUNT register
0x328 CH12_AL2_READ_ADDR Alias for channel 12 READ_ADDR register
0x32c CH12_AL2_WRITE_ADDR_TRIG Alias for channel 12 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x330 CH12_AL3_CTRL Alias for channel 12 CTRL register
0x334 CH12_AL3_WRITE_ADDR Alias for channel 12 WRITE_ADDR register
0x338 CH12_AL3_TRANS_COUNT Alias for channel 12 TRANS_COUNT register
0x33c CH12_AL3_READ_ADDR_TRIG Alias for channel 12 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x340 CH13_READ_ADDR DMA Channel 13 Read Address pointer
0x344 CH13_WRITE_ADDR DMA Channel 13 Write Address pointer
0x348 CH13_TRANS_COUNT DMA Channel 13 Transfer Count
0x34c CH13_CTRL_TRIG DMA Channel 13 Control and Status
0x350 CH13_AL1_CTRL Alias for channel 13 CTRL register
0x354 CH13_AL1_READ_ADDR Alias for channel 13 READ_ADDR register
0x358 CH13_AL1_WRITE_ADDR Alias for channel 13 WRITE_ADDR register
0x35c CH13_AL1_TRANS_COUNT_TRIG Alias for channel 13 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x360 CH13_AL2_CTRL Alias for channel 13 CTRL register
0x364 CH13_AL2_TRANS_COUNT Alias for channel 13 TRANS_COUNT register
0x368 CH13_AL2_READ_ADDR Alias for channel 13 READ_ADDR register
RP2350 Datasheet
## 12.6. DMA 1119
Offset Name Info
0x36c CH13_AL2_WRITE_ADDR_TRIG Alias for channel 13 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x370 CH13_AL3_CTRL Alias for channel 13 CTRL register
0x374 CH13_AL3_WRITE_ADDR Alias for channel 13 WRITE_ADDR register
0x378 CH13_AL3_TRANS_COUNT Alias for channel 13 TRANS_COUNT register
0x37c CH13_AL3_READ_ADDR_TRIG Alias for channel 13 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x380 CH14_READ_ADDR DMA Channel 14 Read Address pointer
0x384 CH14_WRITE_ADDR DMA Channel 14 Write Address pointer
0x388 CH14_TRANS_COUNT DMA Channel 14 Transfer Count
0x38c CH14_CTRL_TRIG DMA Channel 14 Control and Status
0x390 CH14_AL1_CTRL Alias for channel 14 CTRL register
0x394 CH14_AL1_READ_ADDR Alias for channel 14 READ_ADDR register
0x398 CH14_AL1_WRITE_ADDR Alias for channel 14 WRITE_ADDR register
0x39c CH14_AL1_TRANS_COUNT_TRIG Alias for channel 14 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x3a0 CH14_AL2_CTRL Alias for channel 14 CTRL register
0x3a4 CH14_AL2_TRANS_COUNT Alias for channel 14 TRANS_COUNT register
0x3a8 CH14_AL2_READ_ADDR Alias for channel 14 READ_ADDR register
0x3ac CH14_AL2_WRITE_ADDR_TRIG Alias for channel 14 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x3b0 CH14_AL3_CTRL Alias for channel 14 CTRL register
0x3b4 CH14_AL3_WRITE_ADDR Alias for channel 14 WRITE_ADDR register
0x3b8 CH14_AL3_TRANS_COUNT Alias for channel 14 TRANS_COUNT register
0x3bc CH14_AL3_READ_ADDR_TRIG Alias for channel 14 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x3c0 CH15_READ_ADDR DMA Channel 15 Read Address pointer
0x3c4 CH15_WRITE_ADDR DMA Channel 15 Write Address pointer
0x3c8 CH15_TRANS_COUNT DMA Channel 15 Transfer Count
0x3cc CH15_CTRL_TRIG DMA Channel 15 Control and Status
0x3d0 CH15_AL1_CTRL Alias for channel 15 CTRL register
0x3d4 CH15_AL1_READ_ADDR Alias for channel 15 READ_ADDR register
0x3d8 CH15_AL1_WRITE_ADDR Alias for channel 15 WRITE_ADDR register
RP2350 Datasheet
## 12.6. DMA 1120
Offset Name Info
0x3dc CH15_AL1_TRANS_COUNT_TRIG Alias for channel 15 TRANS_COUNT register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x3e0 CH15_AL2_CTRL Alias for channel 15 CTRL register
0x3e4 CH15_AL2_TRANS_COUNT Alias for channel 15 TRANS_COUNT register
0x3e8 CH15_AL2_READ_ADDR Alias for channel 15 READ_ADDR register
0x3ec CH15_AL2_WRITE_ADDR_TRIG Alias for channel 15 WRITE_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x3f0 CH15_AL3_CTRL Alias for channel 15 CTRL register
0x3f4 CH15_AL3_WRITE_ADDR Alias for channel 15 WRITE_ADDR register
0x3f8 CH15_AL3_TRANS_COUNT Alias for channel 15 TRANS_COUNT register
0x3fc CH15_AL3_READ_ADDR_TRIG Alias for channel 15 READ_ADDR register
This is a trigger register (0xc). Writing a nonzero value will
reload the channel counter and start the channel.
0x400 INTR Interrupt Status (raw)
0x404 INTE0 Interrupt Enables for IRQ 0
0x408 INTF0 Force Interrupts
0x40c INTS0 Interrupt Status for IRQ 0
0x414 INTE1 Interrupt Enables for IRQ 1
0x418 INTF1 Force Interrupts
0x41c INTS1 Interrupt Status for IRQ 1
0x424 INTE2 Interrupt Enables for IRQ 2
0x428 INTF2 Force Interrupts
0x42c INTS2 Interrupt Status for IRQ 2
0x434 INTE3 Interrupt Enables for IRQ 3
0x438 INTF3 Force Interrupts
0x43c INTS3 Interrupt Status for IRQ 3
0x440 TIMER0 Pacing timer (generate periodic TREQs)
0x444 TIMER1 Pacing timer (generate periodic TREQs)
0x448 TIMER2 Pacing timer (generate periodic TREQs)
0x44c TIMER3 Pacing timer (generate periodic TREQs)
0x450 MULTI_CHAN_TRIGGER Trigger one or more channels simultaneously
0x454 SNIFF_CTRL Sniffer Control
0x458 SNIFF_DATA Data accumulator for sniff hardware
0x460 FIFO_LEVELS Debug RAF, WAF, TDF levels
0x464 CHAN_ABORT Abort an in-progress transfer sequence on one or more channels
RP2350 Datasheet
## 12.6. DMA 1121
Offset Name Info
0x468 N_CHANNELS The number of channels this DMA instance is equipped with.
This DMA supports up to 16 hardware channels, but can be
configured with as few as one, to minimise silicon area.
0x480 SECCFG_CH0 Security level configuration for channel 0.
0x484 SECCFG_CH1 Security level configuration for channel 1.
0x488 SECCFG_CH2 Security level configuration for channel 2.
0x48c SECCFG_CH3 Security level configuration for channel 3.
0x490 SECCFG_CH4 Security level configuration for channel 4.
0x494 SECCFG_CH5 Security level configuration for channel 5.
0x498 SECCFG_CH6 Security level configuration for channel 6.
0x49c SECCFG_CH7 Security level configuration for channel 7.
0x4a0 SECCFG_CH8 Security level configuration for channel 8.
0x4a4 SECCFG_CH9 Security level configuration for channel 9.
0x4a8 SECCFG_CH10 Security level configuration for channel 10.
0x4ac SECCFG_CH11 Security level configuration for channel 11.
0x4b0 SECCFG_CH12 Security level configuration for channel ## 12.
0x4b4 SECCFG_CH13 Security level configuration for channel 13.
0x4b8 SECCFG_CH14 Security level configuration for channel 14.
0x4bc SECCFG_CH15 Security level configuration for channel 15.
0x4c0 SECCFG_IRQ0 Security configuration for IRQ 0. Control whether the IRQ permits
configuration by Non-secure/Unprivileged contexts, and whether
it can observe Secure/Privileged channel interrupt flags.
0x4c4 SECCFG_IRQ1 Security configuration for IRQ 1. Control whether the IRQ permits
configuration by Non-secure/Unprivileged contexts, and whether
it can observe Secure/Privileged channel interrupt flags.
0x4c8 SECCFG_IRQ2 Security configuration for IRQ 2. Control whether the IRQ permits
configuration by Non-secure/Unprivileged contexts, and whether
it can observe Secure/Privileged channel interrupt flags.
0x4cc SECCFG_IRQ3 Security configuration for IRQ 3. Control whether the IRQ permits
configuration by Non-secure/Unprivileged contexts, and whether
it can observe Secure/Privileged channel interrupt flags.
0x4d0 SECCFG_MISC Miscellaneous security configuration
0x500 MPU_CTRL Control register for DMA MPU. Accessible only from a Privileged
context.
0x504 MPU_BAR0 Base address register for MPU region 0. Writable only from a
Secure, Privileged context.
0x508 MPU_LAR0 Limit address register for MPU region 0. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x50c MPU_BAR1 Base address register for MPU region 1. Writable only from a
Secure, Privileged context.
RP2350 Datasheet
## 12.6. DMA 1122
Offset Name Info
0x510 MPU_LAR1 Limit address register for MPU region 1. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x514 MPU_BAR2 Base address register for MPU region 2. Writable only from a
Secure, Privileged context.
0x518 MPU_LAR2 Limit address register for MPU region 2. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x51c MPU_BAR3 Base address register for MPU region 3. Writable only from a
Secure, Privileged context.
0x520 MPU_LAR3 Limit address register for MPU region 3. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x524 MPU_BAR4 Base address register for MPU region 4. Writable only from a
Secure, Privileged context.
0x528 MPU_LAR4 Limit address register for MPU region 4. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x52c MPU_BAR5 Base address register for MPU region 5. Writable only from a
Secure, Privileged context.
0x530 MPU_LAR5 Limit address register for MPU region 5. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x534 MPU_BAR6 Base address register for MPU region 6. Writable only from a
Secure, Privileged context.
0x538 MPU_LAR6 Limit address register for MPU region 6. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x53c MPU_BAR7 Base address register for MPU region 7. Writable only from a
Secure, Privileged context.
0x540 MPU_LAR7 Limit address register for MPU region 7. Writable only from a
Secure, Privileged context, with the exception of the P bit.
0x800 CH0_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x804 CH0_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x840 CH1_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x844 CH1_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x880 CH2_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x884 CH2_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
RP2350 Datasheet
## 12.6. DMA 1123
Offset Name Info
0x8c0 CH3_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x8c4 CH3_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x900 CH4_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x904 CH4_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x940 CH5_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x944 CH5_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x980 CH6_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x984 CH6_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0x9c0 CH7_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0x9c4 CH7_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xa00 CH8_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xa04 CH8_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xa40 CH9_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xa44 CH9_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xa80 CH10_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
RP2350 Datasheet
## 12.6. DMA 1124
Offset Name Info
0xa84 CH10_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xac0 CH11_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xac4 CH11_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xb00 CH12_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xb04 CH12_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xb40 CH13_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xb44 CH13_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xb80 CH14_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xb84 CH14_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer
0xbc0 CH15_DBG_CTDREQ Read: get channel DREQ counter (i.e. how many accesses the
DMA expects it can perform on the peripheral without
overflow/underflow. Write any value: clears the counter, and
cause channel to re-initiate DREQ handshake.
0xbc4 CH15_DBG_TCR Read to get channel TRANS_COUNT reload value, i.e. the length
of the next transfer




# I2C
## 12.2. I2C
Synopsys Documentation
Synopsys Proprietary. Used with permission.
I2C is a commonly used 2-wire interface that can be used to connect devices for low speed data transfer using clock SCL
and data SDA wires.
RP2350 has two identical instances of an I2C controller. The external pins of each controller are connected to GPIO pins
as defined in the GPIO muxing table in Section 9.4. The muxing options give some IO flexibility.
RP2350 Datasheet
## 12.2. I2C 983
## 12.2.1. Features
Each I2C controller is based on a configuration of the Synopsys DW_apb_i2c (v2.03a) IP. The following features are
supported:
• Master or Slave (Default to Master mode)
• Standard mode, Fast mode or Fast mode plus
• Default slave address 0x055
• Supports 10-bit addressing in Master mode
• 16-element transmit buffer
• 16-element receive buffer
• Can be driven from DMA
• Can generate interrupts
## 12.2.1.1. Standard
The I2C controller was designed for I2C Bus specification, version 6.0, dated April 2014.
12.2.1.2. Clocking
All clocks in the I2C controller are connected to clk_sys, including ic_clk, which is mentioned in later sections. The I2C
clock is generated by dividing down this clock, controlled by registers inside the block.
## 12.2.1.3. IOs
Each controller must connect its clock SCL and data SDA to one pair of GPIOs. The I2C standard requires that drivers drive
a signal low, or when not driven the signal will be pulled high. This applies to SCL and SDA. The GPIO pads should be
configured for:
• pull-up enabled
• slew rate limited
• schmitt trigger enabled
 NOTE
There should also be external pull-ups on the board as the internal pad pull-ups may not be strong enough to pull up
external circuits.
## 12.2.2. IP configuration
I2C configuration details (each instance is fully independent):
• 32-bit APB access
• Supports Standard mode, Fast mode or Fast mode plus (not High speed)
• Default slave address of 0x055
• Master or Slave mode
• Master by default (Slave mode disabled at reset)
RP2350 Datasheet
## 12.2. I2C 984
• 10-bit addressing supported in master mode (7-bit by default)
• 16 entry transmit buffer
• 16 entry receive buffer
• Allows restart conditions when a master (can be disabled for legacy device support)
• Configurable timing to adjust TsuDAT/ThDAT
• General calls responded to on reset
• Interface to DMA
• Single interrupt output
• Configurable timing to adjust clock frequency
• Spike suppression (default 7 clk_sys cycles)
• Can NACK after data received by Slave
• Hold transfer when TX FIFO empty
• Hold bus until space available in RX FIFO
• Restart detect interrupt in Slave mode
• Optional blocking Master commands (not enabled by default)
## 12.2.3. I2C overview
The I2C bus is a 2-wire serial interface, consisting of a serial data line SDA and a serial clock SCL. These wires carry
information between the devices connected to the bus. Each device is recognized by a unique address and can operate
as either a "transmitter" or "receiver", depending on the function of the device. Devices can also be considered as
masters or slaves when performing data transfers. A master is a device that initiates a data transfer on the bus and
generates the clock signals to permit that transfer. At that time, any device addressed is considered a slave.
 NOTE
The I2C block must only be programmed to operate in either master OR slave mode only. Operating as a master and
slave simultaneously is not supported.
The I2C block can operate in these modes:
• standard mode (with data rates from 0 to 100 kb/s),
• fast mode (with data rates up to 400 kb/s),
• fast mode plus (with data rates up to 1000 kb/s).
These modes are not supported:
• High-speed mode (with data rates up to 3.4Mb/s),
• Ultra-Fast Speed Mode (with data rates up to 5Mb/s).
 NOTE
References to fast mode also apply to fast mode plus, unless specifically stated otherwise.
The I2C block can communicate with devices in one of these modes as long as they are attached to the bus.
Additionally, fast mode devices are downward compatible. For instance, fast mode devices can communicate with
standard mode devices at up to 100 kb/s over the I2C bus system. However, standard mode devices are not upward
compatible and should not be incorporated in a fast-mode I2C bus system as they cannot follow the higher transfer
rate; unpredictable states would occur.
RP2350 Datasheet
## 12.2. I2C 985
The following devices commonly use high-speed mode:
• LCD displays
• high-bit count ADCs
• high capacity EEPROMs
These devices typically need to transfer large amounts of data.
Most maintenance and control applications, the common use for the I2C bus, typically operate at 100 kHz in standard
and fast modes. Any DW_apb_i2c device can be attached to an I2C bus. Every device can talk with any master, passing
information back and forth. There needs to be at least one master (such as a microcontroller or DSP) on the bus, but
there can be multiple masters, which require them to arbitrate for ownership. Multiple masters and arbitration are
explained later in this chapter. The I2C block does not support SMBus and PMBus protocols (for System management
and Power management).
The DW_apb_i2c is made up of:
• an AMBA APB slave interface
• an I2C interface
• FIFO logic to maintain coherency between the two interfaces
The blocks of the component are illustrated in Figure 68.
AMBA Bus
Interface Unit Register File Slave State
Machine
Master State
Machine
Clock Generator Rx Shift Tx Shift Rx Filter
Toggle Synchronizer DMA Interface Interrupt
Controller
RX FIFO TX FIFO
DW_apb_i2c
Figure 68. I2C Block
diagram
The following define the functions of the blocks in Figure 68:
• AMBA Bus Interface Unit: Takes the APB interface signals and translates them into a common generic interface
that allows the register file to be bus protocol-agnostic.
• Register File: Contains configuration registers and is the interface with software.
• Slave State Machine: Follows the protocol for a slave and monitors bus for address match.
• Master State Machine: Generates the I2C protocol for the master transfers.
• Clock Generator: Calculates the required timing to do the following:
◦
Generate the SCL clock when configured as a master
◦
Check for bus idle
◦
Generate a START and a STOP
◦
Setup the data and hold the data
• RX Shift: Takes data into the design and extracts it in byte format.
RP2350 Datasheet
## 12.2. I2C 986
• TX Shift: Presents data supplied by CPU for transfer on the I2C bus.
• RX Filter: Detects the events in the bus; for example, start, stop and arbitration lost.
• Toggle: Generates pulses on both sides and toggles to transfer signals across clock domains.
• Synchronizer: Transfers signals from one clock domain to another.
• DMA Interface: Generates the handshaking signals to the central DMA controller in order to automate the data
transfer without CPU intervention.
• Interrupt Controller: Generates the raw interrupt and interrupt flags, allowing them to be set and cleared.
• RX FIFO/TX FIFO: Holds the RX FIFO and TX FIFO register banks and controllers, along with their status levels.
## 12.2.4. I2C terminology
This section defines key terms used in various parts of the I2C.
## 12.2.4.1. I2C bus terms
The following terms relate to how the role of the I2C device and how it interacts with other I2C devices on the bus.
Transmitter
the device that sends data to the bus. A transmitter can either be a device that initiates the data transmission to the
bus (a master-transmitter) or the device that responds to a request from the master to send data to the bus (a
slave-transmitter).
Receiver
the device that receives data from the bus. A receiver can either be a device that receives data on its own request (a
master-receiver) or a device that receives data in response to a request from the master (a slave-receiver).
Master
the component that initializes a transfer (START command), generates the clock SCL signal and terminates the
transfer (STOP command). A master can be either a transmitter or a receiver.
Slave
the device addressed by the master. A slave can be either receiver or transmitter.
Multi-master
the ability for more than one master to co-exist on the bus at the same time without collision or data loss.
Arbitration
the predefined procedure that authorizes only one master at a time to take control of the bus. For more information
about this behaviour, refer to Section ## 12.2.8.
Synchronization
the predefined procedure that synchronizes the clock signals provided by two or more masters. For more
information about this feature, refer to Section ## 12.2.9.
SDA
the data signal line (Serial Data).
SCL
the clock signal line (Serial Clock).
RP2350 Datasheet
## 12.2. I2C 987
## 12.2.4.2. Bus transfer terms
The following terms are specific to data transfers that occur to and from the I2C bus.
START (RESTART)
data transfer begins with a START or RESTART condition. The level of the SDA data line changes from high to low,
while the SCL clock line remains high. When this occurs, the bus becomes busy.
 NOTE
START and RESTART conditions are functionally identical.
STOP
data transfer is terminated by a STOP condition. This occurs when the level on the SDA data line passes from the low
state to the high state, while the SCL clock line remains high. When the data transfer has been terminated, the bus is
free or idle once again. The bus stays busy if a RESTART is generated instead of a STOP condition.
## 12.2.5. I2C behaviour
The DW_apb_i2c can be controlled with software to be one of the following:
• An I2C master only, communicating with other I2C slaves
• An I2C slave only, communicating with one or more I2C masters.
The master is responsible for generating the clock and controlling the transfer of data. The slave is responsible for
either transmitting or receiving data to and from the master. The acknowledgement of data is sent by the device that is
receiving data, which can be either a master or a slave. As mentioned previously, the I2C protocol also allows multiple
masters to reside on the I2C bus and uses an arbitration procedure to determine bus ownership.
Each slave has a unique address determined by the system designer. When a master wants to communicate with a
slave:
1. The master transmits a START/RESTART condition that is then followed by the slave’s address and a control bit
(R/W) to determine if the master wants to transmit data or receive data from the slave.
2. The slave then sends an acknowledge (ACK) pulse after the address.
When the master (master-transmitter) writes to the slave (slave-receiver), the receiver gets one byte of data. This
transaction continues until the master terminates the transmission with a STOP condition.
When the master reads from a slave (master-receiver), the slave transmits (slave-transmitter) a byte of data to the
master. The master then acknowledges the transaction with the ACK pulse. This transaction continues until the master
terminates the transmission by not acknowledging (NACK) the transaction after the last byte is received, and then the
master issues a STOP condition or addresses another slave after issuing a RESTART condition. This behaviour is
illustrated in Figure 69.
SDA
SCL S
or
R
START or RESTART Condition
P
or
R
R
or
P
Byte Complete Interrupt STOP AND RESTART Condition
within Slave
SCL held low while
servicing interrupts
MSB
1 2 7 8 9 1 2 3-8 9
LSB ACK
from slave from receiver
ACK
Figure 69. Data
transfer on the I2C
Bus
The DW_apb_i2c is a synchronous serial interface. The SDA line is a bidirectional signal that changes only while the SCL line
is low except for STOP, START, and RESTART conditions. The output drivers are open-drain or open-collector to perform
wire-AND functions on the bus. The maximum number of devices on the bus is limited by only the maximum
capacitance specification of 400 pF. Data is transmitted in byte packages.
The I2C protocols implemented in DW_apb_i2c are described in more details in Section ## 12.2.6.
RP2350 Datasheet
## 12.2. I2C 988
## 12.2.5.1. START and STOP generation
When operating as an I2C master, putting data into the TX FIFO causes the DW_apb_i2c to generate a START condition on
the I2C bus. Writing a 1 to IC_DATA_CMD.STOP causes the DW_apb_i2c to generate a STOP condition on the I2C bus; a
STOP condition is not issued if this bit is not set, even if the TX FIFO is empty.
When operating as a slave, the DW_apb_i2c does not generate START and STOP conditions, as per the protocol. However,
if a read request is made to the DW_apb_i2c, it holds the SCL line low until read data has been supplied to it. This stalls the
I2C bus until read data is provided to the slave DW_apb_i2c, or the DW_apb_i2c slave is disabled by writing a 0 to
IC_ENABLE.ENABLE.
## 12.2.5.2. Combined formats
The DW_apb_i2c supports mixed read and write combined format transactions in both 7-bit and 10-bit addressing modes.
The DW_apb_i2c does not support mixed address and mixed address format - that is, a 7-bit address transaction followed
by a 10-bit address transaction or vice versa-combined format transactions. To initiate combined format transfers,
IC_CON.IC_RESTART_EN should be set to 1. With this value set and operating as a master, when the DW_apb_i2c
completes an I2C transfer, it checks the TX FIFO and executes the next transfer. If the direction of this transfer differs
from the previous transfer, the combined format is used to issue the transfer. If the TX FIFO is empty when the current
I2C transfer completes:
• IC_DATA_CMD.STOP is checked and:
◦
If set to 1, a STOP bit is issued.
◦
If set to 0, the SCL is held low until the next command is written to the TX FIFO.
For more details, refer to Section ## 12.2.7.
## 12.2.6. I2C protocols
This section defines protocols used in the DW_apb_i2c.
## 12.2.6.1. START and STOP conditions
When the bus is idle, both the SCL and SDA signals are pulled high through external pull-up resistors on the bus. When the
master wants to start a transmission on the bus, the master issues a START condition: a high-to-low transition of the
SDA signal while SCL is 1. When the master wants to terminate the transmission, the master issues a STOP condition: a
low-to-high transition of the SDA signal while SCL is 1. Figure 70 shows the timing of the START and STOP conditions.
When data is being transmitted on the bus, the SDA signal must be stable when SCL is set to 1.
SDA
SCL
S
Data line Stable Data Valid Change of Data
Allowed
Start Condition Change of Data Allowed Stop Condition
P
Figure 70. I2C START
and STOP Condition
RP2350 Datasheet
## 12.2. I2C 989
 NOTE
The signal transitions for the START/STOP conditions, as depicted in Figure 70, reflect those observed at the output
signals of the master driving the I2C bus. Care should be taken when observing the SDA/SCL signals at the input
signals of slaves, because unequal line delays may result in an incorrect SDA/SCL timing relationship.
## 12.2.6.2. Addressing slave protocol
There are two address formats: 7-bit and 10-bit.
## 12.2.6.2.1. 7-bit address format
In the 7-bit address format, the first seven bits (bits 7:1) of the first byte set the slave address and the LSB bit (bit 0)
defines the R/W status, as shown in Figure 71. When bit 0 is set to 0, the master writes to the slave. When bit 0 is set to
1, the master reads from the slave.
S A6 A5 A4 A3 A2 A1 A0 R/W ACK
sent by slave
Slave Address
S = START Condition ACK = Acknowledge R/W = Read/Write Pulse
Figure 71. I2C 7-bit
Address Format
## 12.2.6.2.2. 10-bit address format
The 10-bit address format transfers two bytes for each 10-bit address.
• In the first byte, the first five bits (bits 7:3) indicate a 10-bit transfer. The next two bits (bits 2:1) contain bits 9:8 of
the slave address. The LSB bit (bit 0) defines the R/W status.
• The second byte contains bits 7:0 of the slave address.
Figure 72 shows the 10-bit address format:
S ‘1’ ‘1’ ‘1’ ‘0’ A9 A8 R/W ACK A7 A6 A5 A4 A3 A2 A1 A0
sent by slave
Reserved for 10-bit Address
sent by slave
S = START Condition ACK = Acknowledge R/W = Read/Write Pulse
ACK
Figure 72. 10-bit
Address Format
This table defines the special purpose and reserved first byte addresses.
Table 1051.
I2C/SMBus Definition
of Bits in First Byte
Slave Address R/W Bit Description
0000 000 0 General Call Address. DW_apb_i2c
places the data in the receive buffer
and issues a General Call interrupt.
0000 000 1 START byte. For more details, refer to
Section ## 12.2.6.4.
0000 001 X CBUS address. DW_apb_i2c ignores
these accesses.
0000 010 X Reserved.
RP2350 Datasheet
## 12.2. I2C 990
Slave Address R/W Bit Description
0000 011 X Reserved.
0000 1XX X High-speed master code (for more
information, refer to Section ## 12.2.8).
1111 1XX X Reserved.
1111 0XX X 10-bit slave addressing.
0001 000 X SMbus Host. (not supported)
0001 100 X SMBus Alert Response Address. (not
supported)
1100 001 X SMBus Device Default Address. (not
supported)
DW_apb_i2c does not restrict you from using reserved addresses. However, if you use these reserved addresses, you may
experience incompatibilities with I2C components.
## 12.2.6.3. Transmitting and receiving protocol
The master can initiate data transmission and reception to and from the bus, acting as either a master-transmitter or
master-receiver. A slave responds to requests from the master to either transmit data or receive data to/from the bus,
acting as either a slave-transmitter or slave-receiver, respectively.
## 12.2.6.3.1. Master-transmitter and slave-receiver
All data is transmitted in byte format, with no limit on the number of bytes transferred per data transfer. After the master
sends the address and R/W bit or the master transmits a byte of data to the slave, the slave-receiver must respond with
the acknowledge signal (ACK). When no slave-receiver responds with an ACK pulse, the master aborts the transfer by
issuing a STOP condition. The slave must leave the SDA line high so that the master can abort the transfer. If the mastertransmitter is transmitting data as shown in Figure 73, the slave-receiver responds to the master-transmitter with an
acknowledge pulse after every byte of data is received.
S
For 7-bit Address
R/W
‘0’ (read)
Slave Address A DATA A DATA A/A P
S DATA A/A P
For 10-bit Address
From Master to Slave A = Acknowledge (SDA low)
A = No Acknowledge (SDA high)
S = START Condition
From Slave to Master P = STOP Condition
R/W
‘0’ (write)
A A
Slave Address
First 7 bits
Slave Address
Second Byte
‘11110xxx’
Figure 73. I2C MasterTransmitter Protocol
## 12.2.6.3.2. Master-receiver and slave-transmitter
If the master is receiving data as shown in Figure 74 the master responds to the slave-transmitter with an acknowledge
pulse after receiving each byte of data, except for the last byte. This is the way the master-receiver notifies the slavetransmitter that this is the last byte. The slave-transmitter relinquishes the SDA line after detecting No Acknowledge
(NACK) so that the master can issue a STOP condition.
RP2350 Datasheet
## 12.2. I2C 991
S
For 7-bit Address
R/W
‘1’ (read)
Slave Address A DATA A DATA A P
‘1’ (read)
S
For 10-bit Address
From Master to Slave A = Acknowledge (SDA low)
A = No Acknowledge (SDA high)
S = START Condition
R = RESTART Condition
From Slave to Master P = STOP Condition
R/W
‘0’ (write)
A A Sr A DATA A P
Slave Address
First 7 bits
Slave Address
Second Byte R/W Slave Address
First 7 bits
‘11110xxx’ ‘11110xxx’
Figure 74. I2C MasterReceiver Protocol
When a master does not want to relinquish the bus with a STOP condition, the master can issue a RESTART condition.
This is identical to a START condition except it occurs after the ACK pulse. Operating in master mode, the DW_apb_i2c can
then communicate with the same slave using a transfer of a different direction. For a description of the combined
format transactions that the DW_apb_i2c supports, see Section ## 12.2.5.2.
 NOTE
The DW_apb_i2c must be completely disabled before the target slave address register (IC_TAR) can be reprogrammed.
## 12.2.6.4. START BYTE Transfer Protocol
The START BYTE transfer protocol is designed for systems that do not have an on-board dedicated I2C hardware
module. When the DW_apb_i2c is addressed as a slave, it always samples the I2C bus at the highest speed supported so
that it never requires a START BYTE transfer. However, when DW_apb_i2c is a master, it supports the generation of START
BYTE transfers at the beginning of every transfer in case a slave device requires it.
This protocol consists of the transmission of seven zeros, followed by a one, as illustrated in Figure 75. This allows the
processor polling the bus to under-sample the address phase until zero is detected. Once the microcontroller detects a
zero, it switches from the under sampling rate to the correct rate of the master.
SDA
SCL 1 2
S Ack
(HIGH)
dummy
acknowledge
Sr
7 8 9
start byte 00000001
Figure 75. I2C Start
Byte Transfer
The START BYTE procedure is as follows:
1. Master generates a START condition.
2. Master transmits the START byte (0000 0001).
3. Master transmits the ACK clock pulse. (Present only to conform with the byte handling format used on the bus)
4. No slave sets the ACK signal to zero.
5. Master generates a RESTART (R) condition.
Hardware receivers do not respond to the START BYTE procedure because it uses a reserved address and resets after
the RESTART condition generates.
RP2350 Datasheet
## 12.2. I2C 992
## 12.2.7. TX FIFO Management and START, STOP and RESTART Generation
When operating as a master, the DW_apb_i2c component supports the mode of TX (transmit) FIFO management
illustrated in Figure 76.
## 12.2.7.1. TX FIFO management
The component does not generate a STOP if the TX FIFO becomes empty; in this situation the component holds the SCL
line low, stalling the bus until a new entry is available in the TX FIFO. A STOP condition is generated only when the user
specifically requests it by setting bit nine (Stop bit) of the command written to IC_DATA_CMD register. Figure 76 shows
the bits in the IC_DATA_CMD register.
IC_DATA_CMD Restart
Data Read/Write field; data retrieved from slave is read from
this field; data to be sent to slave is written to this field
CDM Write-only field; this bit determines whether transfer to
be carried out is Read (CMD=1) or Write (CMD=0)
Stop Write-only field; this bit determines whether STOP is
generated after data byte is sent or received
Restart Write-only field; this bit determines whether RESTART
(or STOP followed by START in case or restart
capability is not enabled) is generated before data is
sent or received
9 8 7 0
Stop CMD DATA
Figure 76.
IC_DATA_CMD
Register
Figure 77 illustrates the behaviour of the DW_apb_i2c when the TX FIFO becomes empty while operating as a master
transmitter, as well as the generation of a STOP condition.
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with data
(write data in this example)
Last byte popped from
Tx FIFO, with STOP bit
not set
Master releases SCL line and
resumes transmission because
new data became available
Data availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 W Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack
P
Because STOP bit was not set on
last byte popped from Tx FIFO,
Master holds SCL low
Tx FIFO loaded
with new data
Last byte popped from Tx FIFO
with STOP bit set
STOP bit enabled triggers
STOP condition on bus
Figure 77. Master
Transmitter - TX FIFO
Empties/STOP
Generation
Figure 78 illustrates the behaviour of the DW_apb_i2c when the TX FIFO becomes empty while operating as a master
receiver, as well as the generation of a STOP condition.
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with command
(read operation in this example)
Last command
popped from Tx
FIFO, with STOP bit
not set
Tx FIFO loaded
with new command
Last command popped from
Tx FIFO with STOP bit set
STOP bit enabled triggers
STOP condition on bus
Master releases SCL line and
resumes transmission
because new command
became available
Because STOP bit was
not set on last
command popped
from Tx FIFO, Master
holds SCL low
Command availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 R Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Nak
Figure 78. Master S
Receiver - TX FIFO
Empties/STOP
Generation
Figure 79 and Figure 80 illustrate configurations where the user can control the generation of RESTART conditions on
the I2C bus. If bit 10 (Restart) of the IC_DATA_CMD register is set and the restart capability is enabled (IC_RESTART_EN=1),
a RESTART is generated before the data byte is written to or read from the slave. If the restart capability is not enabled,
a STOP followed by a START is generated in place of the RESTART. Figure 79 illustrates this situation during operation
as a master transmitter.
RP2350 Datasheet
## 12.2. I2C 993
SDA
SCL
FIFO_
EMPTY
A6
S
Next byte in Tx FIFO
has RESTART bit set
Because next byte on Tx FIFO has
been tagged with RESTART bit,
Master issues RESTART and
initiates new transmission
Data availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 W Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack A6 A5 A4 A3 A2 A1 A0 W Ack D7 D6
SR
Tx FIFO loaded with data
(write data in this example)
Figure 79. Master
Transmitter - Restart
Bit of IC_DATA_CMD
Is Set
Figure 80 illustrates the same situation, but during operation as a master receiver.
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with command
(read operation in this example)
Next command in Tx FIFO
has RESTART bit set
Master issues NOT ACK as
required before RESTART
when operating as receiver
Because next command on Tx FIFO
has been tagged with RESTART bit,
Master issues RESTART and
initiates new transmission
Command availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 R Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Nak A6 A5 A4 A3 A2 A1 A0 R Ack D7 D6
Figure 80. Master SR
Receiver - Restart Bit
of IC_DATA_CMD Is
Set
Figure 81 illustrates operation as a master transmitter where the Stop bit of the IC_DATA_CMD register is set and the TX
FIFO is not empty.
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with data
(write data in this example)
One byte (not last one)
is popped from Tx FIFO
with STOP bit set
Because more data is available in
Tx FIFO, a new transmission is
immediately initiated (provided
master is granted access to bus)
Data availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 W Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack A6 A5 A4 A3 A2 A1 A0 W Ack D7 D6
P S
Because STOP bit was set on last
byte popped from Tx FIFO, Master
generates STOP condition
Figure 81. Master
Transmitter - Stop Bit
of IC_DATA_CMD
Set/TX FIFO not empty
Figure 82 illustrates operation as a master transmitter where the first byte loaded into the TX FIFO is allowed to go
empty with the Restart bit set.
SDA
SCL
FIFO_
EMPTY
A6
S
Last byte popped
from Tx FIFO with
STOP bit not set
Tx FIFO loaded
with new command
Master issues RESTART and
initiates new transmission
Because STOP bit was
not set on last byte
popped from Tx FIFO,
Data availability triggers START Master holds SCL low
condition on bus
A5 A4 A3 A2 A1 A0 W Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack A6 A5 A4 A3 A2 A1 A0 W Ack D7 D6
SR
Tx FIFO loaded with data
(write data in this example)
Figure 82. Master
Transmitter - First
Byte Loaded Into TX
FIFO Allowed to
Empty, Restart Bit Set
Figure 83 illustrates operation as a master receiver where the Stop bit of the IC_DATA_CMD register is set and the TX
FIFO is not empty.
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with command
(read operation in this example)
One command
(not last one) is
popped from
Tx FIFO with
STOP bit set
Because more commands
are available inTx FIFO, a
new transmission is
immediately initiated
(provided master is granted
access to bus)
Because STOP bit was
set on last command
popped from Tx FIFO,
Master generates
STOP condition
Command availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 R Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 A6 A5 A4 A3 A2 A1 A0 R Ack D7 D6
P S
Nak
Figure 83. Master
Receiver - Stop Bit of
IC_DATA_CMD Set/TX
FIFO Not Empty
Figure 84 illustrates operation as a master receiver where the first command loaded after the TX FIFO is allowed to
empty and the Restart bit is set.
RP2350 Datasheet
## 12.2. I2C 994
SDA
SCL
FIFO_
EMPTY
A6
S
Tx FIFO loaded with command
(read operation in this example)
Last command popped
from Tx FIFO with
STOP bit not set
Tx FIFO loaded
with new command
Next command loaded into
Tx FIFO has RESTART bit set
Master issues NOT ACK as
required before RESTART
when operating as receiver
Master issues RESTART and
initiates new transmission
Because STOP bit
was not set on last
command popped
from Tx FIFO, Master
holds SCL low
Command availability triggers
START condition on bus
A5 A4 A3 A2 A1 A0 R Ack D7 D6 D5 D4 D3 D2 D1 D0 Ack D7 D6 D5 D4 D3 D2 D1 D0 Nak A6 A5 A4 A3 A2 A1 A0 R Ack D7 D6
Figure 84. Master SR
Receiver - First
Command Loaded
After TX FIFO Allowed
to Empty/Restart Bit
Set
## 12.2.8. Multiple master arbitration
The DW_apb_i2c bus protocol allows multiple masters to reside on the same bus. If there are two masters on the same
I2C bus, there is an arbitration procedure if both try to take control of the bus at the same time by generating a START
condition at the same time. Once a master (for example, a microcontroller) has control of the bus, no other master can
take control until the first master sends a STOP condition and places the bus in an idle state.
Arbitration takes place on the SDA line, while the SCL line is set to 1. The master, which transmits a one while the other
master transmits zero, loses arbitration and turns off its data output stage. The master that lost arbitration can continue
to generate clocks until the end of the byte transfer. If both masters address the same slave device, the arbitration
could go into the data phase.
Upon detecting that it has lost arbitration to another master, the DW_apb_i2c stops generating SCL by disabling the output
driver. Figure 85 illustrates the timing of two masters arbitrating on the bus.
CLKA
DATA2
SDA
SCL
MSB
MSB
MSB
‘0’
matching data
DATA1 loses arbitration
SDA mirrors DATA2
SDA lines up
with DATA1
START condition
‘1’
Figure 85. Multiple
Master Arbitration
Control of the bus is determined by address or master code and data sent by competing masters, so there is no central
master nor any order of priority on the bus.
Arbitration is not allowed between the following conditions:
• A RESTART condition and a data bit
• A STOP condition and a data bit
• A RESTART condition and a STOP condition
 NOTE
Slaves do not participate in the arbitration process.
## 12.2.9. Clock synchronisation
When two or more masters try to transfer information on the bus at the same time, they must arbitrate and synchronize
the SCL clock. All masters generate their own clock to transfer messages. Data is valid only during the high period of SCL
RP2350 Datasheet
## 12.2. I2C 995
clock. Clock synchronisation is performed using the wired-AND connection to the SCL signal. When the master
transitions the SCL clock to zero, the master starts counting the low time of the SCL clock and transitions the SCL clock
signal to one at the beginning of the next clock period. However, if another master is holding the SCL line to 0, then the
master goes into a HIGH wait state until the SCL clock line transitions to one.
All masters then count off their high time, and the master with the shortest high time transitions the SCL line to zero. The
masters then count out their low time and the one with the longest low time forces the other masters into a HIGH wait
state. Therefore, a synchronized SCL clock is generated, which is illustrated in Figure 86. Optionally, slaves may hold the
SCL line low to slow down the timing on the I2C bus.
CLKA
CLKB
SCL
Wait State
SCL LOW transition Resets all CLKs
to start counting their LOW periods
SCL transitions HIGH when
all CLKs are in HIGH state
Start counting HIGH period
Figure 86. MultiMaster Clock
Synchronisation
## 12.2.10. Operation modes
This section provides information about operation modes.
 NOTE
Only set the DW_apb_i2c to operate as an I2C Master or an I2C Slave. Never set the DW_apb_i2c to operate as both
simultaneously. To avoid this, never simultaneously set IC_CON.IC_SLAVE_DISABLE and IC_CON.MASTER_MODE to
zero and one, respectively.
## 12.2.10.1. Slave mode operation
This section discusses slave mode procedures.
## 12.2.10.1.1. Initial configuration
To use the DW_apb_i2c as a slave, perform the following steps:
1. Disable the DW_apb_i2c by writing a 0 to IC_ENABLE.ENABLE.
2. Write to the IC_SAR register (bits 9:0) to set the slave address. This is the address to which the DW_apb_i2c
responds.
3. Write to the IC_CON register to specify which type of addressing is supported (7-bit or 10-bit by setting bit 3).
Enable the DW_apb_i2c in slave-only mode by writing a 0 into bit six (IC_CON.IC_SLAVE_DISABLE) and a 0 to bit zero
(IC_CON.MASTER_MODE).
RP2350 Datasheet
## 12.2. I2C 996
 NOTE
Slaves and masters can use different addressing settings. For instance, a slave can be programmed with 7-bit
addressing and a master with 10-bit addressing, and vice versa.
4. Enable the DW_apb_i2c by writing a 1 to IC_ENABLE.ENABLE.
 NOTE
Depending on the reset values chosen, steps two and three may not be necessary because the reset values can be
configured. For instance, if the device is only going to be a master, there would be no need to set the slave address
because you can configure DW_apb_i2c to have the slave disabled after reset and to enable the master after reset. The
values stored are static and do not need to be reprogrammed if the DW_apb_i2c is disabled.
 WARNING
Only bring the DW_apb_i2c Slave out of reset when the I2C bus is IDLE. De-asserting the reset when a transfer is
ongoing on the bus causes internal synchronization flip-flops used to synchronize SDA and SCL to toggle from a reset
value of one to the actual value on the bus. This can result in SDA toggling from one to zero while SCL is one, thereby
causing a false START condition to be detected by the DW_apb_i2c Slave. This scenario can also be avoided by
configuring the DW_apb_i2c with IC_SLAVE_DISABLE = 1 and MASTER_MODE = 1 so that the Slave interface is disabled after
reset. It can then be enabled by programming IC_CON[0] = 0 and IC_CON[6] = 0 after the internal SDA and SCL have
synchronized to the value on the bus; this takes approximately six ic_clk cycles after reset de-assertion.
## 12.2.10.1.2. Slave-transmitter operation for a single byte
When another I2C master device on the bus addresses the DW_apb_i2c and requests data, the DW_apb_i2c acts as a slavetransmitter. The following steps occur:
1. The other I2C master device initiates an I2C transfer with an address that matches the slave address in the IC_SAR
register of the DW_apb_i2c.
2. The DW_apb_i2c acknowledges the sent address and recognizes the direction of the transfer to indicate that it is
acting as a slave-transmitter.
3. The DW_apb_i2c asserts the RD_REQ interrupt (bit five of the IC_RAW_INTR_STAT register) and holds the SCL line low. It
remains in a wait state until software responds. If the RD_REQ interrupt has been masked, due to
IC_INTR_MASK.M_RD_REQ being set to zero, use a hardware and/or software timing routine to instruct the CPU to
perform periodic reads of the IC_RAW_INTR_STAT register.
◦
Reads that indicate IC_RAW_INTR_STAT.RD_REQ being set to one must be treated as the equivalent of the
RD_REQ interrupt being asserted.
◦
Software must then act to satisfy the I2C transfer.
◦
The timing interval used should be in the order of 10 times the fastest SCL clock period the DW_apb_i2c can
handle. For example, for 400 kb/s, the timing interval is 25μs.
 NOTE
The value of 10 is recommended here because this is approximately the amount of time required for a
single byte of data transferred on the I2C bus.
4. If there is any data remaining in the TX FIFO before receiving the read request, the DW_apb_i2c asserts a TX_ABRT
interrupt (bit six of the IC_RAW_INTR_STAT register) to flush the old data from the TX FIFO. If the TX_ABRT interrupt
has been masked, due to IC_INTR_MASK.M_TX_ABRT being set to zero, re-use the timing routine described in the
previous step to read the IC_RAW_INTR_STAT register.
RP2350 Datasheet
## 12.2. I2C 997
 NOTE
Because the DW_apb_i2c's TX FIFO is forced into a flushed/reset state whenever a TX_ABRT event occurs, software
must release the DW_apb_i2c from this state by reading the IC_CLR_TX_ABRT register before attempting to write
into the TX FIFO. See register IC_RAW_INTR_STAT for more details.
◦
Reads that indicate bit six (R_TX_ABRT) being set to one must be treated as the equivalent of the TX_ABRT
interrupt being asserted.
◦
There is no further action required from software.
◦
The timing interval used should be similar to that described in the previous step for the
IC_RAW_INTR_STAT.RD_REQ register.
5. Software writes to the IC_DATA_CMD register with the data to be written (by writing a 0 in bit 8).
6. Software must clear the RD_REQ and TX_ABRT interrupts (bits five and six, respectively) of the IC_RAW_INTR_STAT
register before proceeding. If the RD_REQ or TX_ABRT interrupts have been masked, then clearing of the
IC_RAW_INTR_STAT register will have already been performed when either the R_RD_REQ or R_TX_ABRT bit has been
read as one.
7. The DW_apb_i2c releases the SCL and transmits the byte.
8. The master may hold the I2C bus by issuing a RESTART condition or release the bus by issuing a STOP condition.
 NOTE
Slave-Transmitter Operation for a single byte is not applicable in Ultra-Fast mode, since this mode does not support
read transfers.
## 12.2.10.1.3. Slave-receiver operation for a single byte
When another I2C master device on the bus addresses the DW_apb_i2c and is sending data, the DW_apb_i2c acts as a slavereceiver and the following steps occur:
1. The other I2C master device initiates an I2C transfer with an address that matches the DW_apb_i2c's slave address in
the IC_SAR register.
2. The DW_apb_i2c acknowledges the sent address and recognizes the direction of the transfer to indicate that the
DW_apb_i2c is acting as a slave-receiver.
3. DW_apb_i2c receives the transmitted byte and places it in the receive buffer.
 NOTE
If the Rx (receive) FIFO is completely filled with data when a byte is pushed, then the DW_apb_i2c slave holds the
I2C SCL line low until the Rx FIFO has some space, and then continues with the next read request.
4. DW_apb_i2c asserts the RX_FULL interrupt IC_RAW_INTR_STAT.RX_FULL. If the RX_FULL interrupt has been masked, due
to setting IC_INTR_MASK.M_RX_FULL to zero or setting IC_TX_TL to a value larger than zero, you should
implement a timing routine (described in Section ## 12.2.10.1.2) for periodic reads of the IC_STATUS register. This
timing routine should treat reads of the IC_STATUS register, with bit 3 (RFNE) set at one as the equivalent of an
RX_FULL interrupt.
5. Software may read the byte from the IC_DATA_CMD register (bits 7:0).
6. The other master device may hold the I2C bus by issuing a RESTART condition, or release the bus by issuing a
STOP condition.
RP2350 Datasheet
## 12.2. I2C 998
## 12.2.10.1.4. Slave-transfer operation for bulk transfers
In the standard I2C protocol, all transactions are single byte transactions; the programmer responds to a remote master
read request by writing one byte into the slave’s TX FIFO. When a slave (slave-transmitter) receives a read request
(RD_REQ) from the remote master (master-receiver), at a minimum there should be at least one entry placed into the
slave-transmitter’s TX FIFO.
DW_apb_i2c handles more data in the TX FIFO. This enables subsequent read requests to take data without raising an
interrupt. This eliminates latencies incurred between interrupts. This mode only occurs when DW_apb_i2c acts as a slavetransmitter. If the remote master acknowledges the data sent by the slave-transmitter and there is no data in the slave’s
TX FIFO, the DW_apb_i2c holds the I2C SCL line low while it raises the read request interrupt (RD_REQ) and waits for a data
write into the TX FIFO.
If the RD_REQ interrupt is masked by setting IC_INTR_STAT.R_RD_REQ to zero, use a timing routine to activate periodic
reads of the IC_RAW_INTR_STAT register. Reads of IC_RAW_INTR_STAT that return bit five (RD_REQ) set to one must be
treated as the equivalent of RD_REQ. This timing routine is similar to that described in Section ## 12.2.10.1.2.
The RD_REQ interrupt is raised upon a read request. Always clear this interrupt when exiting the interrupt service handling
routine (ISR). The ISR allows you to either write one byte or more than one byte into the TX FIFO. The master can
request additional data at the end of a transmission by acknowledging the last byte. In this scenario, the slave must
raise RD_REQ again.
If you know in advance that the remote master requests a packet of n bytes, you can write n byte to the TX FIFO. Then,
when another master addresses DW_apb_i2c and requests data, the remote master will receive a continuous stream of
data. This happens because the DW_apb_i2c slave continues to send data to the remote master as long as the remote
master acknowledges the data sent and there is data available in the TX FIFO. There is no need to hold the SCL line low
or to issue RD_REQ again.
If the remote master doesn’t read all of the bytes from the TX FIFO, the DW_apb_i2c ignores the excess bytes with the
following procedure:
• The DW_apb_i2c clears the TX FIFO.
• The DW_apb_i2c generates a transmit abort (TX_ABRT) event.
At the time an ACK/NACK is expected, if a NACK is received, then the remote master has all the data it wants. At this
time, a flag is raised within the slave’s state machine to clear the leftover data in the TX FIFO. This flag is transferred to
the processor bus clock domain where the FIFO exists and the contents of the TX FIFO is cleared at that time.
## 12.2.10.2. Master mode operation
This section discusses master mode procedures.
## 12.2.10.2.1. Initial configuration
To use the DW_apb_i2c as a master, perform the following steps:
1. Disable the DW_apb_i2c by writing zero to IC_ENABLE.ENABLE.
2. Write to the IC_CON register to set the maximum speed mode supported (bits 2:1) and the desired speed of the
DW_apb_i2c master-initiated transfers, either 7-bit or 10-bit addressing (bit 4). Ensure that bit six
(IC_SLAVE_DISABLE) is written with a 1 and bit zero (MASTER_MODE) is written with a 1.
RP2350 Datasheet
## 12.2. I2C 999
 NOTE
Slaves and masters can use different addressing settings. For instance, a slave can be programmed with 7-bit
addressing and a master with 10-bit addressing, and vice versa.
3. Write the address of the I2C device to be addressed to bits 9:0 of the IC_TAR register. This register also
determines whether the I2C will perform a General Call or a START BYTE command.
4. Enable the DW_apb_i2c by writing a one to IC_ENABLE.ENABLE.
5. Write the transfer direction and the data to be sent to the IC_DATA_CMD register. This step generates the START
condition and the address byte on the DW_apb_i2c. Once DW_apb_i2c is enabled and there is data in the TX FIFO,
DW_apb_i2c starts reading the data.
 NOTE
If you write to the IC_DATA_CMD register before enabling the DW_apb_i2c, the data and commands are lost: the
buffers are kept cleared when DW_apb_i2c is disabled.
The values stored are static and do not need to be reprogrammed when the DW_apb_i2c is disabled except for transfer
direction and data. As a result, you may not need to perform steps two, three, four, and five if you already configured the
reset values.
## 12.2.10.2.2. Master transmit and master receive
The DW_apb_i2c supports switching back and forth between reading and writing dynamically. To transmit data, write data
to the lower byte of the I2C RX/TX Data Buffer and Command Register (IC_DATA_CMD). For I2C write operations, write
zero to the CMD bit [8]. Subsequently, to issue a read command, write a one to the CMD bit and write don’t care to the
lower byte of the IC_DATA_CMD register. The DW_apb_i2c master continues to initiate transfers as long as there are
commands present in the TX FIFO. If the TX FIFO becomes empty, the master performs one of the following actions
based on the value of IC_DATA_CMD:
• If set to one, it issues a STOP condition after completing the current transfer.
• If set to zero, it holds SCL low until next command is written to the TX FIFO.
For more details, refer to Section ## 12.2.7.
## 12.2.10.3. Disabling DW_apb_i2c
The IC_ENABLE_STATUS register allows software to unambiguously determine when the I2C hardware has completely
shut down.
 NOTE
Earlier versions of DW_apb_i2c required the programmer to monitor two registers: (IC_STATUS and
IC_RAW_INTR_STAT). RP2350 only requires the programmer to monitor IC_ENABLE_STATUS.
To shut down I2C hardware, write a zero to IC_ENABLE.ENABLE. The DW_apb_i2c master can be disabled only if the
command currently processing when the de-assertion occurs has the STOP bit set to one. If you attempt to disable the
DW_apb_i2c master while processing a command without the STOP bit set, the DW_apb_i2c master continues to remain
active, holding the SCL line low until a new command is received in the TX FIFO.
To relinquish the I2C bus and disable DW_apb_i2c while the DW_apb_i2c master is processing a command without the STOP
bit set, issue an ABORT request.
RP2350 Datasheet
## 12.2. I2C 1000
## 12.2.10.3.1. Procedure
1. Define a timer interval (ti2c_poll) equal to the 10 times the signalling period for the highest I2C transfer speed used in
the system and supported by DW_apb_i2c. For example, if the highest I2C transfer mode is 400 kb/s, ti2c_poll is 25μs.
2. Define a maximum time-out parameter, MAX_T_POLL_COUNT, such that if any repeated polling operation exceeds this
maximum value, an error is reported.
3. Execute a blocking thread, process, or function that prevents any further I2C master transactions from starting
from software, but allows any pending transfers to be completed.
 NOTE
This step can be ignored if DW_apb_i2c is programmed to operate as an I2C slave only.
1. The variable POLL_COUNT is initialized to zero.
2. Set bit zero of the IC_ENABLE register to zero.
3. Read the IC_ENABLE_STATUS register and test the IC_EN bit (bit 0). Increment POLL_COUNT by one. If
POLL_COUNT >= MAX_T_POLL_COUNT, exit with the relevant error code.
4. If IC_ENABLE_STATUS[0] is one, sleep for ti2c_poll and proceed to the previous step. Otherwise, exit with a
relevant success code.
## 12.2.10.4. Aborting I2C transfers
The ABORT control bit of the IC_ENABLE register allows the software to relinquish the I2C bus before completing the
issued transfer commands from the TX FIFO. In response to an ABORT request, the controller issues the STOP condition
over the I2C bus, followed by a TX FIFO flush. Aborting the transfer is allowed only in master mode of operation.
## 12.2.10.4.1. Procedure
1. Stop filling the TX FIFO (IC_DATA_CMD) with new commands.
2. When operating in DMA mode, disable the transmit DMA by setting TDMAE to zero.
3. Set IC_ENABLE.ABORT to one.
4. Wait for the M_TX_ABRT interrupt.
5. Read the IC_TX_ABRT_SOURCE register to identify the source as ABRT_USER_ABRT.
## 12.2.11. Spike suppression
The DW_apb_i2c contains programmable spike suppression logic that matches requirements imposed by the I2C Bus
Specification for SS/FS modes. This logic is based on counters that monitor the input signals (SCL and SDA), checking if
they remain stable for a predetermined amount of ic_clk cycles before they are sampled internally. There is one
separate counter for each signal (SCL and SDA). The number of ic_clk cycles can be programmed by the user. The value
should account for the frequency of ic_clk and the relevant spike length specification. Each counter starts whenever its
input signal changes value. Depending on the behaviour of the input signal, one of the following scenarios occurs:
• The input signal remains unchanged until the counter reaches its count limit value. When this happens, the counter
resets and stops, and the internal version of the signal updates to the input value.
• The input signal changes again before the counter reaches its count limit value. When this happens, the counter
resets and stops, but the internal version of the signal does not update.
The timing diagram in Figure 87 illustrates the behaviour described above.
RP2350 Datasheet
## 12.2. I2C 1001
Recovery Clocks
Spike length counter
SCL
Internal filtered SCL
0 1 2 3 0 1 2 3 4 5 0
Figure 87. Spike
Suppression Example
 NOTE
There is a 2-stage synchronizer on the SCL input. For the sake of simplicity, this synchronization delay was not
included in the timing diagram in Figure 87.
The I2C Bus Specification calls for different maximum spike lengths according to the operating mode (50 ns for SS and
FS). Register IC_FS_SPKLEN holds the maximum spike length for SS and FS modes.
This register is 8 bits wide and accessible through the APB interface for reads and writes. However, you can only write
to this register when the DW_apb_i2c is disabled. The minimum value that can be programmed into these registers is one;
attempting to program a value smaller than one results in the value one being written.
The default value for these registers is based on the value of 100 ns for ic_clk period, so should be updated for the
clk_sys period in use on RP2350.
 NOTE
• Because the minimum value that can be programmed into the IC_FS_SPKLEN register is one, the spike length
specification can be exceeded for low frequencies of ic_clk. Consider the simple example of a 10 MHz (100 ns
period) ic_clk; in this case, the minimum spike length that can be programmed is 100 ns, which means that
spikes up to this length are suppressed.
• Standard synchronization logic (two flip-flops in series) is implemented upstream of the spike suppression
logic and is not affected in any way by the contents of the spike length registers or the operation of the spike
suppression logic; the two operations (synchronization and spike suppression) are completely independent.
Because the SCL and SDA inputs are asynchronous to ic_clk, there is one ic_clk cycle uncertainty in the sampling
of these signals. Depending on when they occur relative to the rising edge of ic_clk, spikes of the same original
length might show a difference of one ic_clk cycle after being sampled.
• Spike suppression is symmetrical; the behaviour is exactly the same for transitions from zero to one and from
one to zero.
## 12.2.## 12. Fast mode plus operation
In fast mode plus, the DW_apb_i2c extends fast mode operation to be support speeds up to 1000 kb/s. To enable the
DW_apb_i2c for fast mode plus operation, perform the following steps before initiating any data transfer:
1. Set ic_clk frequency greater than or equal to 32 MHz (refer to Section ## 12.2.14.2.1).
2. Program the IC_CON register [2:1] = 2’b10 for fast mode or fast mode plus.
3. Program IC_FS_SCL_LCNT and IC_FS_SCL_HCNT registers to meet the fast mode plus SCL (refer to Section
## 12.2.14).
4. Program the IC_FS_SPKLEN register to suppress the maximum spike of 50 ns.
5. Program the IC_SDA_SETUP register to meet the minimum data setup time (tSU; DAT).
## 12.2.13. Bus clear feature
DW_apb_i2c supports the bus clear feature that provides graceful recovery of data SDA and clock SCL lines during unlikely
events in which either the clock or data line is stuck at LOW.
RP2350 Datasheet
## 12.2. I2C 1002
## 12.2.13.1. SDA line is stuck at LOW
In case of SDA line stuck at LOW, the master performs the following actions to recover as shown in Figure 88 and Figure
89:
1. Master sends a maximum of nine clock pulses to recover the bus LOW within those nine clocks.
◦
The number of clock pulses will vary with the number of bits that remain to be sent by the slave. As the
maximum number of bits is nine, master sends up to nine clock pluses and allows the slave to recover.
◦
The master attempts to assert a Logic 1 on the SDA line and check whether SDA is recovered. If the SDA is not
recovered, it will continue to send a maximum of nine SCL clocks.
2. If SDA line is recovered within nine clock pulses, the master will send STOP to release the bus.
3. If SDA line is not recovered even after the ninth clock pulse, you must hardware reset the system.
Recovery Clocks
SDA
SCL
MST_SDA
0 1 2 3 4 5 6 7 8 9 10
Master drives 9 clocks to recover SDA stuck at low
Figure 88. SDA
Recovery with 9 SCL
Clocks
Recovery Clocks
SDA
SCL
MST_SDA
0 1 2 3 4 5 6 7
Master drives 9 clocks to recover SDA stuck at low
Figure 89. SDA
Recovery with 6 SCL
Clocks
## 12.2.13.2. SCL line is stuck at LOW
In the unlikely event (due to an electric failure of a circuit) where the clock (SCL) is stuck to LOW, there is no effective
method to overcome this problem. Instead, reset the bus using the hardware reset signal.
## 12.2.14. IC_CLK frequency configuration
When the DW_apb_i2c is configured as a Standard (SS), Fast (FS), or Fast-Mode Plus (FM+), the *CNT registers must be set
before any I2C bus transaction can take place in order to ensure proper I/O timing. The *CNT registers are:
• IC_SS_SCL_HCNT
• IC_SS_SCL_LCNT
• IC_FS_SCL_HCNT
• IC_FS_SCL_LCNT
 NOTE
The tBUF timing and setup/hold time of START, STOP and RESTART registers uses *HCNT/*LCNT register settings for
the corresponding speed mode.
RP2350 Datasheet
## 12.2. I2C 1003
 NOTE
It is not necessary to program any of the *CNT registers if the DW_apb_i2c is enabled to operate only as an I2C slave,
since these registers are used only to determine the SCL timing requirements for operation as an I2C master.
Table 1052 lists the derivation of I2C timing parameters from the *CNT programming registers.
Table 1052. Derivation
of I2C Timing
Parameters from
*CNT Registers
Timing Parameter Symbol Standard Speed Fast Speed / Fast Speed Plus
LOW period of the SCL clock tLOW IC_SS_SCL_LCNT IC_FS_SCL_LCNT
HIGH period of the SCL clock tHIGH IC_SS_SCL_HCNT IC_FS_SCL_HCNT
Setup time for a repeated
START condition
tSU;STA IC_SS_SCL_LCNT IC_FS_SCL_HCNT
Hold time (repeated) START
condition
tHD;STA IC_SS_SCL_HCNT IC_FS_SCL_HCNT
Setup time for STOP
condition
tSU;STO IC_SS_SCL_HCNT IC_FS_SCL_HCNT
Bus free time between a
STOP and a START
condition
tBUF IC_SS_SCL_LCNT IC_FS_SCL_LCNT
Spike length tSP IC_FS_SPKLEN IC_FS_SPKLEN
Data hold time tHD;DAT IC_SDA_HOLD IC_SDA_HOLD
Data setup time tSU;DAT IC_SDA_SETUP IC_SDA_SETUP
## 12.2.14.1. Minimum high and low counts in SS, FS, and FM+ modes.
When the DW_apb_i2c operates as an I2C master, in both transmit and receive transfers:
• IC_SS_SCL_LCNT and IC_FS_SCL_LCNT register values must be larger than IC_FS_SPKLEN + 7.
• IC_SS_SCL_HCNT and IC_FS_SCL_HCNT register values must be larger than IC_FS_SPKLEN + 5.
Details regarding the DW_apb_i2c high and low counts are as follows:
• The minimum value of IC_*_SPKLEN + 7 for the *_LCNT registers is due to the time required for the DW_apb_i2c to drive
SDA after a negative edge of SCL.
• The minimum value of IC_*_SPKLEN + 5 for the *_HCNT registers is due to the time required for the DW_apb_i2c to
sample SDA during the high period of SCL.
• The DW_apb_i2c adds one cycle to the programmed *_LCNT value in order to generate the low period of the SCL clock;
this is due to the counting logic for SCL low counting to (*_LCNT + 1).
• The DW_apb_i2c adds IC_*_SPKLEN + 7 cycles to the programmed *_HCNT value in order to generate the high period of
the SCL clock, due to the following factors:
◦
The counting logic for SCL high counts to (*_HCNT + 1).
◦
The digital filtering applied to the SCL line incurs a delay of SPKLEN + 2 ic_clk cycles, where SPKLEN is
IC_FS_SPKLEN if the component is operating in SS or FS.
◦ Whenever SCL is driven one to zero by the DW_apb_i2c (completing the SCL high time) an internal logic latency of
three ic_clk cycles is incurred. Consequently, the minimum SCL low time of which the DW_apb_i2c is capable is
nine ic_clk periods (7 + 1 + 1), while the minimum SCL high time is thirteen ic_clk periods (6 + 1 + 3 + 3).
RP2350 Datasheet
## 12.2. I2C 1004
 NOTE
The total high time and low time of SCL generated by the DW_apb_i2c master is also influenced by the rise time and fall
time of the SCL line, as shown in the illustration and equations in Figure 90. SCL rise and fall time parameters vary
depending on external factors such as:
• Characteristics of the IO driver
• Pull-up resistor value
• Total capacitance on SCL line
These characteristics are beyond the control of the DW_apb_i2c.
HCNT + IC_*_SPKLEN + 7
SCL
rise time
SCL
fall time
SCL
rise time
LCNT + 1
SCL_High_time = [(HCNT + IC_*_SPKLEN + 7) * ic_clk] + SCL_Fall_time
SCL_low_time = [(LCNT + 1) * ic_clk] - SCL_Fall_time + SCL_Rise_time
ic_clk
ic_clk_in_a/SCL
Figure 90. Impact of
SCL Rise Time and Fall
Time on Generated
SCL
## 12.2.14.2. Minimum IC_CLK frequency
This section describes the minimum ic_clk frequencies that the DW_apb_i2c supports for each speed mode, and the
associated high and low count values. In slave mode, IC_SDA_HOLD (Thd;dat) and IC_SDA_SETUP (Tsu:dat) need to be
programmed to satisfy the I2C protocol timing requirements. The following examples are for the case where
IC_FS_SPKLEN is programmed to two.
## 12.2.14.2.1. Standard Mode (SM), Fast Mode (FM), and Fast Mode Plus (FM+)
This section details how to derive a minimum ic_clk value for standard and fast modes of the DW_apb_i2c. Although the
following method shows how to do fast mode calculations, you can also use the same method in order to do
calculations for standard mode and fast mode plus.
 NOTE
The following computations do not consider the SCL_Rise_time and SCL_Fall_time.
Given conditions and calculations for the minimum DW_apb_i2c ic_clk value in fast mode:
• Fast mode has data rate of 400 kb/s; implies SCL period of 1/400 kHz = 2.5μs
• Minimum hcnt value of 14 as a seed value; IC_HCNT_FS = 14
• Protocol minimum SCL high and low times:
◦
MIN_SCL_LOWtime_FS = 1300 ns
◦
MIN_SCL_HIGHtime_FS = 600 ns
Derived equations:
RP2350 Datasheet
## 12.2. I2C 1005
SCL_PERIOD_FS / (IC_HCNT_FS + IC_LCNT_FS) = IC_CLK_PERIOD
IC_LCNT_FS × IC_CLK_PERIOD = MIN_SCL_LOWtime_FS
Combined, the previous equations produce the following:
IC_LCNT_FS × (SCL_PERIOD_FS / (IC_LCNT_FS + IC_HCNT_FS) ) = MIN_SCL_LOWtime_FS
Solving for IC_LCNT_FS:
IC_LCNT_FS × (2.5μs / (IC_LCNT_FS + 14) ) = 1.3μs
The previous equation gives:
IC_LCNT_FS = roundup(15.166) = 16
These calculations produce IC_LCNT_FS = 16 and IC_HCNT_FS = 14, giving an ic_clk value of:
2.5μs / (16 + 14) = 83.3ns = 12 MHz
Testing these results shows that protocol requirements are satisfied.
Table 1053 lists the minimum ic_clk values for all modes with high and low count values.
Table 1053. ic_clk in
Relation to High and
Low Counts
Speed Mode ic_clkfreq
(MHz)
Minimum
Value of
IC_*_SPKLEN
SCL Low Time
in `ic_clk`s
SCL Low
Program
Value
SCL Low Time SCL High
Time in
`ic_clk`s
SCL High
Program
Value
SCL High
Time
SS 2.7 1 13 12 4.7μs 14 6 5.2μs
FS ## 12.0 1 16 15 1.33μs 14 6 1.16μs
FM+ 32 2 16 15 500 ns 16 7 500 ns
• The IC_*_SCL_LCNT and IC_*_SCL_HCNT registers are programmed using the SCL low and high program values in Table
1053, which are calculated using SCL low count minus one, and SCL high counts minus eight, respectively. The
values in Table 1053 are based on IC_SDA_RX_HOLD = 0. The maximum IC_SDA_RX_HOLD value depends on the IC_*CNT
registers in Master mode.
• In order to compute the HCNT and LCNT considering RC timings, use the following equations:
◦
IC_HCNT_* = [(HCNT + IC_*_SPKLEN + 7) * ic_clk] + SCL_Fall_time
◦
IC_LCNT_* = [(LCNT + 1) * ic_clk] - SCL_Fall_time + SCL_Rise_time
## 12.2.14.3. Calculating high and low counts
The calculations below show how to calculate SCL high and low counts for each speed mode in the DW_apb_i2c. For the
calculations to work, the ic_clk frequencies used must not be less than the minimum ic_clk frequencies specified in
Table 1053.
The default ic_clk period value is set to 100 ns, so default SCL high and low count values are calculated for each speed
RP2350 Datasheet
## 12.2. I2C 1006
mode based on this clock. These values need updating according to the guidelines below.
The equation to calculate the proper number of ic_clk signals required for setting the proper SCL clocks high and low
times is as follows:
IC_xCNT = (ROUNDUP(MIN_SCL_xxxtime*OSCFREQ,0))
MIN_SCL_HIGHtime = Minimum High Period
MIN_SCL_HIGHtime = 4000ns for 100kb/s,
  600ns for 400kb/s,
  260ns for 1000kb/s,
MIN_SCL_LOWtime = Minimum Low Period
MIN_SCL_LOWtime = 4700ns for 100kb/s,
  1300ns for 400kb/s,
  500ns for 1000kb/s,
OSCFREQ = ic_clk Clock Frequency (Hz).
For example:
OSCFREQ = 100MHz
I2Cmode = fast, 400kb/s
MIN_SCL_HIGHtime = 600ns.
MIN_SCL_LOWtime = 1300ns.
IC_xCNT = (ROUNDUP(MIN_SCL_HIGH_LOWtime*OSCFREQ,0))
IC_HCNT = (ROUNDUP(600ns * 100MHz,0))
IC_HCNTSCL PERIOD = 60
IC_LCNT = (ROUNDUP(1300ns * 100MHz,0))
IC_LCNTSCL PERIOD = 130
Actual MIN_SCL_HIGHtime = 60*(1/100MHz) = 600ns
Actual MIN_SCL_LOWtime = 130*(1/100MHz) = 1300ns
## 12.2.15. DMA controller interface
The DW_apb_i2c has built-in DMA capability; it has a handshaking interface to the DMA Controller to request and control
transfers. The APB bus is used to perform data transfers to and from the DMA. DMA transfers use single accesses,
since the data rate is relatively low.
## 12.2.15.1. Enabling the DMA controller interface
To enable the DMA Controller interface on the DW_apb_i2c, you must write the DMA Control Register (IC_DMA_CR).
Writing a one into the TDMAE bit field of IC_DMA_CR register enables the DW_apb_i2c transmit handshaking interface.
Writing a one into the RDMAE bit field of the IC_DMA_CR register enables the DW_apb_i2c receive handshaking interface.
## 12.2.15.2. Overview of operation
The DMA Controller is programmed with the number of data items (transfer count) that are to be transmitted or
received by DW_apb_i2c.
The transfer is broken into single transfers on the bus, each initiated by a request from the DW_apb_i2c.
RP2350 Datasheet
## 12.2. I2C 1007
For example, where the transfer count programmed into the DMA Controller is four. The DMA transfer consists of a
series of four single transactions. If the DW_apb_i2c makes a transmit request to this channel, a single data item is written
to the DW_apb_i2c TX FIFO. Similarly, if the DW_apb_i2c makes a receive request to this channel, a single data item is read
from the DW_apb_i2c RX FIFO. Four separate requests must be made to this DMA channel before all four data items are
written or read.
## 12.2.15.3. Watermark levels
In DW_apb_i2c the registers for setting watermarks to allow DMA bursts do not need be set to anything other than their
reset value. Specifically, IC_DMA_TDLR and IC_DMA_RDLR can be left at reset values of zero. This is because only
single transfers are needed due to the low bandwidth of I2C relative to system bandwidth. Because the DMA controller
normally has the highest priority on the system bus, transfers complete quickly.
## 12.2.16. Operation of interrupt registers
Table 1054 lists the operation of the DW_apb_i2c interrupt registers and how they are set and cleared. Some bits are set
by hardware and cleared by software, whereas other bits are set and cleared by hardware.
Table 1054. Clearing
and Setting of
Interrupt Registers
Interrupt Bit Fields Set by Hardware/Cleared by Software Set and Cleared by Hardware
RESTART_DET Y N
GEN_CALL Y N
START_DET Y N
STOP_DET Y N
ACTIVITY Y N
RX_DONE Y N
TX_ABRT Y N
RD_REQ Y N
TX_EMPTY N Y
TX_OVER Y N
RX_FULL N Y
RX_OVER Y N
RX_UNDER Y N
## 12.2.17. List of registers
The I2C0 and I2C1 registers start at base addresses of 0x40090000 and 0x40098000 respectively (defined as I2C0_BASE and
I2C1_BASE in SDK).
RP2350 Datasheet
## 12.2. I2C 1008
 NOTE
You may see references to configuration constants in the I2C register descriptions; these are fixed values, set at
hardware design time. A full list of their values can be found in i2c.h in the pico-sdk GitHub repository.
Table 1055. List of I2C
registers
Offset Name Info
0x00 IC_CON I2C Control Register
0x04 IC_TAR I2C Target Address Register
0x08 IC_SAR I2C Slave Address Register
0x10 IC_DATA_CMD I2C Rx/Tx Data Buffer and Command Register
0x14 IC_SS_SCL_HCNT Standard Speed I2C Clock SCL High Count Register
0x18 IC_SS_SCL_LCNT Standard Speed I2C Clock SCL Low Count Register
0x1c IC_FS_SCL_HCNT Fast Mode or Fast Mode Plus I2C Clock SCL High Count Register
0x20 IC_FS_SCL_LCNT Fast Mode or Fast Mode Plus I2C Clock SCL Low Count Register
0x2c IC_INTR_STAT I2C Interrupt Status Register
0x30 IC_INTR_MASK I2C Interrupt Mask Register
0x34 IC_RAW_INTR_STAT I2C Raw Interrupt Status Register
0x38 IC_RX_TL I2C Receive FIFO Threshold Register
0x3c IC_TX_TL I2C Transmit FIFO Threshold Register
0x40 IC_CLR_INTR Clear Combined and Individual Interrupt Register
0x44 IC_CLR_RX_UNDER Clear RX_UNDER Interrupt Register
0x48 IC_CLR_RX_OVER Clear RX_OVER Interrupt Register
0x4c IC_CLR_TX_OVER Clear TX_OVER Interrupt Register
0x50 IC_CLR_RD_REQ Clear RD_REQ Interrupt Register
0x54 IC_CLR_TX_ABRT Clear TX_ABRT Interrupt Register
0x58 IC_CLR_RX_DONE Clear RX_DONE Interrupt Register
0x5c IC_CLR_ACTIVITY Clear ACTIVITY Interrupt Register
0x60 IC_CLR_STOP_DET Clear STOP_DET Interrupt Register
0x64 IC_CLR_START_DET Clear START_DET Interrupt Register
0x68 IC_CLR_GEN_CALL Clear GEN_CALL Interrupt Register
0x6c IC_ENABLE I2C ENABLE Register
0x70 IC_STATUS I2C STATUS Register
0x74 IC_TXFLR I2C Transmit FIFO Level Register
0x78 IC_RXFLR I2C Receive FIFO Level Register
0x7c IC_SDA_HOLD I2C SDA Hold Time Length Register
0x80 IC_TX_ABRT_SOURCE I2C Transmit Abort Source Register
0x84 IC_SLV_DATA_NACK_ONLY Generate Slave Data NACK Register
0x88 IC_DMA_CR DMA Control Register
RP2350 Datasheet
## 12.2. I2C 1009
Offset Name Info
0x8c IC_DMA_TDLR DMA Transmit Data Level Register
0x90 IC_DMA_RDLR DMA Transmit Data Level Register
0x94 IC_SDA_SETUP I2C SDA Setup Register
0x98 IC_ACK_GENERAL_CALL I2C ACK General Call Register
0x9c IC_ENABLE_STATUS I2C Enable Status Register
0xa0 IC_FS_SPKLEN I2C SS, FS or FM+ spike suppression limit
0xa8 IC_CLR_RESTART_DET Clear RESTART_DET Interrupt Register
0xf4 IC_COMP_PARAM_1 Component Parameter Register 1
0xf8 IC_COMP_VERSION I2C Component Version Register
0xfc IC_COMP_TYPE I2C Component Type Register











# Chapter 11. PIO
11.1. Overview
RP2350 contains 3 identical PIO blocks. Each PIO block has dedicated connections to the bus fabric, GPIO and interrupt
controller. The diagram for a single PIO block is shown below in Figure 44.
IRQ Masking SM IRQs
Instruction Memory
32 Instructions
4 Read Ports
IRQ 0
IRQ 1
FIFO IRQs
FIFO
FIFO
FIFO
FIFO
FIFO
State Machine 3
State Machine 2
State Machine 1
State Machine 0
FIFO
FIFO
FIFO
Write only
IO Mapping
Figure 44. PIO blocklevel diagram. There
are three PIO blocks,
each containing four
state machines. The
four state machines
simultaneously
execute programs
from shared
instruction memory.
FIFO data queues
buffer data transferred
between PIO and the
system. GPIO mapping
logic allows each
state machine to
observe and
manipulate up to 32
GPIOs.
The programmable input/output block (PIO) is a versatile hardware interface. It can support a variety of IO standards,
including:
• 8080 and 6800 parallel bus
• I2C
• 3-pin I2S
• SDIO
• SPI, DSPI, QSPI
• UART
• DPI or VGA (via resistor DAC)
PIO is programmable in the same sense as a processor. There are three PIO blocks with four state machines. Each can
independently execute sequential programs to manipulate GPIOs and transfer data. Unlike a general purpose processor,
PIO state machines are specialised for IO, with a focus on determinism, precise timing, and close integration with fixedfunction hardware. Each state machine is equipped with:
• Two 32-bit shift registers (either direction, any shift count)
• Two 32-bit scratch registers
• 4 × 32-bit bus FIFO in each direction (TX/RX), reconfigurable as 8 × 32 in a single direction
• Fractional clock divider (16 integer, 8 fractional bits)
RP2350 Datasheet
11.1. Overview 876
• Flexible GPIO mapping
• DMA interface (sustained throughput up to 1 word per clock from system DMA)
• IRQ flag set/clear/status
Each state machine, along with its supporting hardware, occupies approximately the same silicon area as a standard
serial interface block, such as an SPI or I2C controller. However, PIO state machines can be configured and
reconfigured dynamically to implement numerous different interfaces.
Making state machines programmable in a software-like manner, rather than a fully configurable logic fabric like a
complex programmable logic device (CPLD), allows more hardware interfaces to be offered in the same cost and power
envelope. This also presents a more familiar programming model, and simpler tool flow, to those who wish to exploit
PIO’s full flexibility by programming it directly, rather than using a pre-made interface from the PIO library.
PIO is performant as well as flexible, thanks to a carefully selected set of fixed-function hardware inside each state
machine. When outputting DPI, PIO can sustain 360 Mb/s during the active scanline period when running from a
48 MHz system clock. In this example, one state machine handles frame/scanline timing and generates the pixel clock.
Another handles the pixel data and unpacks run-length-encoded scanlines.
State machines' inputs and outputs are mapped to up to 32 GPIOs (limited to 30 GPIOs for RP2350). All state machines
have independent, simultaneous access to any GPIO. For example, the standard UART code allows TX, RX, CTS and RTS to
be any four arbitrary GPIOs, and I2C permits the same for SDA and SCL. The amount of freedom available depends on how
exactly a given PIO program chooses to use PIO’s pin mapping resources, but at the minimum, an interface can be
freely shifted up or down by some number of GPIOs.
11.1.1. Changes from RP2040
RP2350 adds the following new registers and controls:
• DBG_CFGINFO.VERSION indicates the PIO version, to allow PIO feature detection at runtime.
◦
This 4-bit field was reserved-0 on RP2040 (indicating version 0), and reads as 1 on RP2350.
• GPIOBASE adds support for more than 32 GPIOs per PIO block.
◦
Each PIO block is still limited to 32 GPIOs at a time, but GPIOBASE selects which 32.
• CTRL.NEXT_PIO_MASK and CTRL.PREV_PIO_MASK apply some CTRL register operations to state machines in
neighbouring PIO blocks simultaneously.
◦
CTRL.NEXTPREV_SM_DISABLE stops PIO state machines in multiple PIO blocks simultaneously.
◦
CTRL.NEXTPREV_SM_ENABLE starts PIO state machines in multiple PIO blocks simultaneously.
◦
CTRL.NEXTPREV_CLKDIV_RESTART synchronises the clock dividers of PIO state machines in multiple PIO
blocks
• SM0_SHIFTCTRL.IN_COUNT masks unneeded IN-mapped pins to zero.
◦
This is useful for MOV x, PINS instructions, which previously always returned a full rotated 32-bit value.
• IRQ0_INTE and IRQ1_INTE now expose all eight SM IRQ flags to system-level interrupts (not just the lower four).
• Registers starting from RXF0_PUTGET0 expose each RX FIFO’s internal storage registers for random read or write
access from the system,
◦
The new FJOIN_RX_PUT FIFO join mode enables random writes from the state machine, and random reads from
the system (for implementing status registers).
◦
The new FJOIN_RX_GET FIFO join mode enables random reads from the state machine, and random writes from
the system (for implementing control registers).
◦
Setting both FJOIN_RX_PUT and FJOIN_RX_GET enables random read and write access from the state machine, but
disables system access.
RP2350 Datasheet
11.1. Overview 877
RP2350 adds the following new instruction features:
• Adds PINCTRL_JMP_PIN as a source for the WAIT instruction, plus an offset in the range 0-3.
◦
This gives WAIT pin arguments a per-SM mapping that is independent of the IN-mapped pins.
• Adds PINDIRS as a destination for MOV.
◦
This allows changing the direction of all OUT-mapped pins with a single instruction: MOV PINDIRS, NULL or MOV
PINDIRS, ~NULL
• Adds SM IRQ flags as a source for MOV x, STATUS
◦
This allows branching (as well as blocking) on the assertion of SM IRQ flags.
• Extends IRQ instruction encoding to allow state machines to set, clear and observe IRQ flags from different PIO
blocks.
◦
There is no delay penalty for cross-PIO IRQ flags: an IRQ on one state machine is observable to all state
machines on the next cycle.
• Adds the FJOIN_RX_GET FIFO mode.
◦
A new MOV encoding reads any of the four RX FIFO storage registers into OSR.
◦
This instruction permits random reads of the four FIFO entries, indexed either by instruction bits or the Y
scratch register.
• Adds the FJOIN_RX_PUT FIFO mode.
◦
A new MOV encoding writes the ISR into any of the four RX FIFO storage registers.
◦
The registers are indexed either by instruction bits or the Y scratch register.
RP2350 adds the following security features:
• Limits Non-secure PIOs (set to via ACCESSCTRL) to observation of only Non-secure GPIOs. Attempting to read a
Secure GPIO returns a 0.
• Disables cross-PIO functionality (IRQs, CTRL_NEXTPREV operations) between Non-secure PIO blocks (those which
permit Non-secure access according to ACCESSCTRL) and Secure-only blocks (those which do not).
RP2350 includes the following general improvements:
• Increased the number of PIO blocks from two to three (8 → 12 state machines).
• Improved GPIO input/output delay and skew.
• Reduced DMA request (DREQ) latency by one cycle vs RP2040.
11.2. Programmer’s model
The four state machines execute from shared instruction memory. System software loads programs into this memory,
configures the state machines and IO mapping, and then sets the state machines running. PIO programs come from
various sources: assembled directly by the user, drawn from the PIO library, or generated programmatically by user
software.
From this point on, state machines are generally autonomous, and system software interacts through DMA, interrupts
and control registers, as with other peripherals on RP2350. For more complex interfaces, PIO provides a small but
flexible set of primitives which allow system software to be more hands-on with state machine control flow.
RP2350 Datasheet
11.2. Programmer’s model 878
Figure 45. State
machine overview.
Data flows in and out
through a pair of
FIFOs. The state
machine executes a
program which
transfers data
between these FIFOs,
a set of internal
registers, and the pins.
The clock divider can
reduce the state
machine’s execution
speed by a constant
factor.
11.2.1. PIO programs
PIO state machines execute short binary programs.
Programs for common interfaces, such as UART, SPI, or I2C, are available in the PIO library. In many cases, it is not
necessary to write PIO programs. However, the PIO is much more flexible when programmed directly, supporting a wide
variety of interfaces which may not have been foreseen by its designers.
The PIO has a total of nine instructions: JMP, WAIT, IN, OUT, PUSH, PULL, MOV, IRQ, and SET. For more information about these
instructions, see Section 11.4.
Though the PIO only has a total of nine instructions, it would be difficult to edit PIO program binaries by hand. PIO
assembly is a textual format, describing a PIO program, where each command corresponds to one instruction in the
output binary. The following code snippet contains an example program written in in PIO assembly:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.pio Lines 8 - 13
 8 .program squarewave
 9 set pindirs, 1 ; Set pin to output
10 again:
11 set pins, 1 [1] ; Drive pin high and then delay for one cycle
12 set pins, 0 ; Drive pin low
13 jmp again ; Set PC to label `again`
The PIO assembler is included with the SDK, and is called pioasm. This program processes a PIO assembly input text file,
which may contain multiple programs, and writes out the assembled programs ready for use. For the SDK, these
assembled programs are emitted as C headers, containing constant arrays.
For more information, see Section 11.3.
11.2.2. Control flow
On every system clock cycle, each state machine fetches, decodes and executes one instruction. Each instruction takes
precisely one cycle, unless it explicitly stalls (such as the WAIT instruction). Instructions may insert a delay of up to 31
cycles before the next instruction execute, to help write cycle-exact programs.
The program counter, or PC, points to the location in the instruction memory being executed on this cycle. Generally, PC
increments by one each cycle, wrapping at the end of the instruction memory. Jump instructions are an exception and
explicitly provide the next value that PC will take.
Our example assembly program (listed as .program squarewave above) shows both of these concepts in practice. It drives
a 50/50 duty cycle square wave with a period of four cycles onto a GPIO. Using some other features (e.g. side-set) this
can be made as low as two cycles.
RP2350 Datasheet
11.2. Programmer’s model 879
 NOTE
Side-set is where a state machine drives a small number of GPIOs in addition to the main side effects of the
instruction it executes. It’s described fully in Section 11.5.1.
The system has write-only access to the instruction memory, which is used to load programs. The clock divider slows
the state machine’s execution by a constant factor, represented as a 16.8 fixed-point fractional number. In the following
example, if a clock division of 2.5 were programmed, the square wave would have a period of cycles.
This is useful for setting a precise baud rate for a serial interface, such as a UART.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.c Lines 34 - 38
34 // Load the assembled program directly into the PIO's instruction memory.
35 // Each PIO instance has a 32-slot instruction memory, which all 4 state
36 // machines can see. The system has write-only access.
37 for (uint i = 0; i < count_of(squarewave_program_instructions); ++i)
38 pio->instr_mem[i] = squarewave_program_instructions[i];
The following code fragments are part of a complete code example which drives a 12.5 MHz square wave out of GPIO 0
(or any other pins we might choose to map). We can also use pins WAIT PIN instruction to stall a state machine’s
execution for some amount of time, or a JMP PIN instruction to branch on the state of a pin, so control flow can vary
based on pin state.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.c Lines 42 - 47
42 // Configure state machine 0 to run at sysclk/2.5. The state machines can
43 // run as fast as one instruction per clock cycle, but we can scale their
44 // speed down uniformly to meet some precise frequency target, e.g. for a
45 // UART baud rate. This register has 16 integer divisor bits and 8
46 // fractional divisor bits.
47 pio->sm[0].clkdiv = (uint32_t) (2.5f * (1 << 16));
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.c Lines 51 - 59
51 // There are five pin mapping groups (out, in, set, side-set, jmp pin)
52 // which are used by different instructions or in different circumstances.
53 // Here we're just using SET instructions. Configure state machine 0 SETs
54 // to affect GPIO 0 only; then configure GPIO0 to be controlled by PIO0,
55 // as opposed to e.g. the processors.
56 pio->sm[0].pinctrl =
57 (1 << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
58 (0 << PIO_SM0_PINCTRL_SET_BASE_LSB);
59 gpio_set_function(0, pio_get_funcsel(pio));
The system can start and stop each state machine at any time, via the CTRL register. Multiple state machines can be
started simultaneously, and the deterministic nature of PIO means they can stay perfectly synchronised.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.c Lines 63 - 67
63 // Set the state machine running. The PIO CTRL register is global within a
64 // PIO instance, so you can start/stop multiple state machines
65 // simultaneously. We're using the register's hardware atomic set alias to
66 // make one bit high without doing a read-modify-write on the register.
67 hw_set_bits(&pio->ctrl, 1 << (PIO_CTRL_SM_ENABLE_LSB + 0));
RP2350 Datasheet
11.2. Programmer’s model 880
Most instructions are executed from instruction memory, but there are other sources which can be freely mixed:
• Instructions written to a special configuration register (SMx INSTR) are immediately executed, momentarily
interrupting other execution. For example, a JMP instruction written to SMx INSTR causes the state machine to start
executing from a different location.
• Instructions can be executed from a register, using the MOV EXEC instruction.
• Instructions can be executed from the output shifter, using the OUT EXEC instruction
The last of these is particularly versatile: instructions can be embedded in the stream of data passing through the FIFO.
The I2C example uses this to embed e.g. STOP and RESTART line conditions alongside normal data. In the case of MOV and
OUT EXEC, the MOV/OUT itself executes in one cycle, and the executee on the next.
11.2.3. Registers
Each state machine possesses a small number of internal registers. These hold input or output data, and temporary
values such as loop counter variables.
11.2.3.1. Output Shift Register (OSR)
Figure 46. Output Shift
Register (OSR). Data is
parcelled out 1…32
bits at a time, and
unused data is
recycled by a
bidirectional shifter.
Once empty, the OSR
is reloaded from the
TX FIFO.
The Output Shift Register (OSR) holds and shifts output data between the TX FIFO and the pins or other destinations,
such as the scratch registers.
• PULL instructions: remove a 32-bit word from the TX FIFO and place into the OSR.
• OUT instructions shift data from the OSR to other destinations, 1…32 bits at a time.
• The OSR fills with zeroes as data is shifted out
• The state machine will automatically refill the OSR from the FIFO on an OUT instruction, once some total shift count
threshold is reached, if autopull is enabled
• Shift direction can be left/right, configurable by the processor via configuration registers
For example, to stream data through the FIFO and output to the pins at a rate of one byte per two clocks:
1 .program pull_example1
2 loop:
3 out pins, 8
4 public entry_point:
5 pull
6 out pins, 8 [1]
7 out pins, 8 [1]
8 out pins, 8
9 jmp loop
11.2.4. Autopull
Autopull (see Section 11.5.4) allows the hardware to automatically refill the OSR in the majority of cases, with the state
machine stalling if it tries to OUT from an empty OSR. This has two benefits:
RP2350 Datasheet
11.2. Programmer’s model 881
• No instructions spent on explicitly pulling from FIFO at the right time
• Higher throughput: can output up to 32 bits on every single clock cycle, if the FIFO stays topped up
After configuring autopull, the above program can be simplified to the following, which behaves identically:
1 .program pull_example2
2
3 loop:
4 out pins, 8
5 public entry_point:
6 jmp loop
Program wrapping (Section 11.5.2) allows further simplification and, if desired, an output of 1 byte every system clock
cycle.
1 .program pull_example3
2
3 public entry_point:
4 .wrap_target
5 out pins, 8 [1]
6 .wrap
11.2.4.1. Input Shift Register (ISR)
Figure 47. Input Shift
Register (ISR). Data
enters 1…32 bits at a
time, and current
contents is shifted left
or right to make room.
Once full, contents is
written to the RX FIFO.
• IN instructions shift 1…32 bits at a time into the register.
• PUSH instructions write the ISR contents to the RX FIFO.
• The ISR is cleared to all-zeroes when pushed.
• The state machine will automatically push the ISR on an IN instruction, once some shift threshold is reached, if
autopush is enabled.
• Shift direction is configurable by the processor via configuration registers
Some peripherals, like UARTs, must shift in from the left to get correct bit order, since the wire order is LSB-first;
however, the processor may expect the resulting byte to be right-aligned. This is solved by the special null input source,
which allows the programmer to shift some number of zeroes into the ISR, following the data.
11.2.4.2. Shift counters
State machines remember how many bits, in total, have been shifted out of the OSR via OUT instructions, and into the ISR
via IN instructions. This information is tracked at all times by a pair of hardware counters: the output shift counter and
the input shift counter. Each is capable of holding values from 0 to 32 inclusive. With each shift operation, the relevant
counter increments by the shift count, up to the maximum value of 32 (equal to the width of the shift register). The state
machine can be configured to perform certain actions when a counter reaches a configurable threshold:
• The OSR can be automatically refilled once some number of bits have been shifted out (see Section 11.5.4).
• The ISR can be automatically emptied once some number of bits have been shifted in (see Section 11.5.4.
RP2350 Datasheet
11.2. Programmer’s model 882
• PUSH or PULL instructions can be conditioned on the input or output shift counter, respectively.
On PIO reset, or the assertion of CTRL_SM_RESTART, the input shift counter is cleared to 0 (nothing yet shifted in), and the
output shift counter is initialised to 32 (nothing remaining to be shifted out; fully exhausted). Some other instructions
affect the shift counters:
• A successful PULL clears the output shift counter to 0
• A successful PUSH clears the input shift counter to 0
• MOV OSR, … (i.e. any MOV instruction that writes OSR) clears the output shift counter to 0
• MOV ISR, … (i.e. any MOV instruction that writes ISR) clears the input shift counter to 0
• OUT ISR, count sets the input shift counter to count
11.2.4.3. Scratch registers
Each state machine has two 32-bit internal scratch registers, called X and Y.
They are used as:
• Source/destination for IN/OUT/SET/MOV
• Source for branch conditions
For example, suppose we wanted to produce a long pulse for "1" data bits, and a short pulse for "0" data bits:
 1 .program ws2812_led
 2
 3 public entry_point:
 4 pull
 5 set x, 23 ; Loop over 24 bits
 6 bitloop:
 7 set pins, 1 ; Drive pin high
 8 out y, 1 [5] ; Shift 1 bit out, and write it to y
 9 jmp !y skip ; Skip the extra delay if the bit was 0
10 nop [5]
11 skip:
12 set pins, 0 [5]
13 jmp x-- bitloop ; Jump if x nonzero, and decrement x
14 jmp entry_point
Here X is used as a loop counter, and Y is used as a temporary variable for branching on single bits from the OSR. This
program can be used to drive a WS2812 LED interface, although more compact implementations are possible (as few
as 3 instructions).
MOV allows the use of the scratch registers to save/restore the shift registers if, for example, you would like to repeatedly
shift out the same sequence.
 NOTE
A much more compact WS2812 example (4 instructions total) is shown in Section 11.6.2.
11.2.4.4. FIFOs
Each state machine has a pair of 4-word deep FIFOs, one for data transfer from system to state machine (TX), and the
other for state machine to system (RX). The TX FIFO is written to by system bus masters, such as a processor or DMA
controller, and the RX FIFO is written to by the state machine. FIFOs decouple the timing of the PIO state machines and
the system bus, allowing state machines to go for longer periods without processor intervention.
RP2350 Datasheet
11.2. Programmer’s model 883
FIFOs also generate data request (DREQ) signals, which allow a system DMA controller to pace its reads/writes based
on the presence of data in an RX FIFO, or space for new data in a TX FIFO. This allows a processor to set up a long
transaction, potentially involving many kilobytes of data, which will proceed with no further processor intervention.
Often, a state machine only transfers data in one direction. In this case, the SHIFTCTRL_FJOIN option can merge the two
FIFOs into a single 8-entry FIFO that only goes in one direction. This is useful for high-bandwidth interfaces such as DPI.
11.2.5. Stalling
State machines may momentarily pause execution for a number of reasons:
• A WAIT instruction’s condition is not yet met
• A blocking PULL when the TX FIFO is empty, or a blocking PUSH when the RX FIFO is full
• An IRQ WAIT instruction which has set an IRQ flag, and is waiting for it to clear
• An OUT instruction when autopull is enabled, and OSR has already reached its shift threshold
• An IN instruction when autopush is enabled, ISR reaches its shift threshold, and the RX FIFO is full
In this case, the program counter does not advance, and the state machine will continue executing this instruction on
the next cycle. If the instruction specifies some number of delay cycles before the next instruction starts, these do not
begin until after the stall clears.
 NOTE
Side-set (Section 11.5.1) is not affected by stalls, and always takes place on the first cycle of the attached
instruction.
11.2.6. Pin mapping
PIO controls the output level and direction of up to 32 GPIOs, and can observe their input levels. On every system clock
cycle, each state machine may do none, one, or both of the following:
• Change the level or direction of some GPIOs via an OUT or SET instruction, or read some GPIOs via an IN instruction
• Change the level or direction of some GPIOs via a side-set operation
Each of these operations uses one of four contiguous ranges of GPIOs, with the base and count of each range
configured via each state machine’s PINCTRL register. There is a range for each of OUT, SET, IN and side-set operations.
Each range can cover any of the GPIOs accessible to a given PIO block (on RP2350 this is the 30 user GPIOs), and the
ranges can overlap.
For each individual GPIO output (level and direction separately), PIO considers all 8 writes that may have occurred on
that cycle, and applies the write from the highest-numbered state machine. If the same state machine performs a SET
/OUT and a side-set on the same GPIO simultaneously, the side-set is used. If no state machine writes to this GPIO
output, its value does not change from the previous cycle.
Generally each state machine’s outputs are mapped to a distinct group of GPIOs, implementing some peripheral
interface.
11.2.7. IRQ flags
IRQ flags are state bits which can be set or cleared by state machines or the system. There are 8 in total: all 8 are visible
to all state machines, and the lower 4 can also be masked into one of PIO’s interrupt request lines, via the IRQ0_INTE and
IRQ1_INTE control registers.
They have two main uses:
RP2350 Datasheet
11.2. Programmer’s model 884
• Asserting system level interrupts from a state machine program, and optionally waiting for the interrupt to be
acknowledged
• Synchronising execution between two state machines
State machines interact with the flags via the IRQ and WAIT instructions.
11.2.8. Interactions between state machines
Instruction memory is implemented as a 1-write, 4-read register file, allowing all four state machines to read an
instruction on the same cycle without stalling.
There are three ways to apply the multiple state machines:
• Pointing multiple state machines at the same program
• Pointing multiple state machines at different programs
• Using multiple state machines to run different parts of the same interface, e.g. TX and RX side of a UART, or
clock/hsync and pixel data on a DPI display
State machines cannot communicate data, but they can synchronise with one another by using the IRQ flags. There are
8 flags total. Each state machine can set or clear any flag using the IRQ instruction, and can wait for a flag to go high or
low using the WAIT IRQ instruction. This allows cycle-accurate synchronisation between state machines.
11.3. PIO assembler (pioasm)
The PIO Assembler parses a PIO source file and outputs the assembled version ready for inclusion in an RP2350
application. This includes C and C++ applications built against the SDK, and Python programs running on the RP2350
MicroPython port.
This section briefly introduces the directives and instructions that can be used in pioasm input. For a deeper discussion
of how to use pioasm, how it is integrated into the SDK build system, extended features such as code pass through, and
the various output formats it can produce, see Raspberry Pi Pico-series C/C++ SDK.
11.3.1. Directives
The following directives control the assembly of PIO programs:
.define (PUBLIC) <symbol> <value>
Define an integer symbol named <symbol> with the value <value> (see Section 11.3.2). If this .define appears before
the first program in the input file, then this define is global to all programs, otherwise it is local to the program in
which it occurs. If PUBLIC is specified, the symbol will be emitted into the assembled output for use by user code. For
the SDK this takes the following forms:
• #define <program_name> <symbol> value: for program symbols
• #define <symbol> value: for global symbols
.clock_div <divider>
If this directive is present, <divider> is the state machine clock divider for the program. Note, that divider is a floating
point value, but may not currently use arithmetic expressions or defined values. This directive affects the default
state machine configuration for a program. This directive is only valid within a program before the first instruction.
.fifo <fifo_config>
If this directive is present, it is used to specify the FIFO configuration for the program. It affects the default state
machine configuration for a program, but also restricts what instructions may be used (for example PUSH makes
RP2350 Datasheet
11.3. PIO assembler (pioasm) 885
no sense if there is no IN FIFO configured).
This directive supports the following configuration values:
• txrx: 4 FIFO entries for each of TX and RX; this is the default.
• tx: All 8 FIFO entries for TX.
• rx: All 8 FIFO entries for RX.
• txput: 4 FIFO entries for TX, and 4 FIFO entries for mov rxfifo[index], isr aka put. This value is not supported on
PIO version 0.
• txget: 4 FIFO entries for TX, and 4 FIFO entries for mov osr, rxfifo[index] aka get. This value is not supported on
PIO version 0.
• putget: 4 FIFO entries for mov rxfifo[index], isr aka put, and 4 FIFO entries for mov osr, rxfifo[index] aka get.
This value is not supported on PIO version 0.
This directive is only valid within a program before the first instruction.
.mov_status rxfifo < <n>
.mov_status txfifo < <n>
.mov_status irq <(prev|next)> set <n>
This directive configures the source for the mov , STATUS. One of the three syntaxes can be used to set the status
based on the RXFIFO level being below a value N, the TXFIFO level being below a value N, or an IRQ flag N being set
on this PIO instance (or the next lower numbered, or higher numbered PIO instance if prev or next or specified).
Note, that the IRQ option requires PIO version 1.
This directive affects the default state machine configuration for a program. This directive is only valid within a
program before the first instruction.
.in <count> (left|right) (auto) (<threshold>)
If this directive is present, <count> indicates the number of IN bits to be used. 'left' or 'right' if specified, control the
ISR shift direction; 'auto', if present, enables "auto-push"; <threshold>, if present, specifies the "auto-push" threshold.
This directive affects the default state machine configuration for a program.
This directive is only valid within a program before the first instruction. When assembling for PIO version 0, <count>
must be 32.
.program <name>
Start a new program with the name <name>. Note that that name is used in code so should be
alphanumeric/underscore not starting with a digit. The program lasts until another .program directive or the end of
the source file. PIO instructions are only allowed within a program.
.origin <offset>
Optional directive to specify the PIO instruction memory offset at which the program must load. Most commonly
this is used for programs that must load at offset 0, because they use data based JMPs with the (absolute) jmp
target being stored in only a few bits. This directive is invalid outside a program.
.out <count> (left|right) (auto) (<threshold>)
If this directive is present, <count> indicates the number of OUT bits to be used. 'left' or 'right' if specified control the
OSR shift direction; 'auto', if present, enables "auto-pull"; <threshold>, if present, specifies the "auto-pull" threshold.
This directive affects the default state machine configuration for a program. This directive is only valid within a
program before the first instruction.
.pio_version <version>
This directive sets the target PIO hardware version. The version for RP2350 is 1 or RP2350, and is also the default
version number. For backwards compatibility with RP2040, 0 or RP2040 may be used.
If this directive appears before the first program in the input file, then this define is the default for all programs,
otherwise it specifies the version for the program in which it occurs. If specified for a program, it must occur before
the first instruction.
RP2350 Datasheet
11.3. PIO assembler (pioasm) 886
.set <count>
If this directive is present, <count> indicates the number of SET bits to be used. This directive affects the default
state machine configuration for a program. This directive is only valid within a program before the first instruction.
.side_set <count> (opt) (pindirs)
If this directive is present, <count> indicates the number of side-set bits to be used. Additionally, opt may be specified
to indicate that a side <value> is optional for instructions (note this requires stealing an extra bit — in addition to the
<count> bits — from those available for the instruction delay). Finally, pindirs may be specified to indicate that the
side set values should be applied to the PINDIRs and not the PINs. This directive is only valid within a program
before the first instruction.
.wrap_target
Place prior to an instruction, this directive specifies the instruction where execution continues due to program
wrapping. This directive is invalid outside of a program, may only be used once within a program, and if not
specified defaults to the start of the program.
.wrap
Placed after an instruction, this directive specifies the instruction after which, in normal control flow (i.e. jmp with
false condition, or no jmp), the program wraps (to .wrap_target instruction). This directive is invalid outside of a
program, may only be used once within a program, and if not specified defaults to after the last program
instruction.
.lang_opt <lang> <name> <option>
Specifies an option for the program related to a particular language generator. (See Language Generators in
Raspberry Pi Pico-series C/C++ SDK). This directive is invalid outside of a program.
.word <value>
Stores a raw 16-bit value as an instruction in the program. This directive is invalid outside of a program.
11.3.2. Values
The following types of values can be used to define integer numbers or branch targets:
Table 978. Values in
pioasm, i.e. <value>
integer An integer value, e.g. 3 or -7.
hex A hexadecimal value, e.g. 0xf.
binary A binary value, e.g. 0b1001.
symbol A value defined by a .define (see pioasm_define).
<label> The instruction offset of the label within the program. Typically used with a JMP instruction
(see Section 11.4.2).
(<expression>) An expression to be evaluated; see expressions. Note that the parentheses are necessary.
11.3.3. Expressions
Expressions may be freely used within pioasm values.
Table 979.
Expressions in pioasm
i.e. <expression>
<expression> + <expression> The sum of two expressions
<expression> - <expression> The difference of two expressions
<expression> * <expression> The multiplication of two expressions
<expression> / <expression> The integer division of two expressions
- <expression> The negation of another expression
RP2350 Datasheet
11.3. PIO assembler (pioasm) 887
<expression> << <expression> One expression shifted left by another expression
<expression> >> <expression> One expression shifted right by another expression
:: <expression> The bit reverse of another expression
<value> Any value (see Section 11.3.2)
11.3.4. Comments
To create a line comment that ignores all content on a certain line following a certain symbol, use // or ;.
To create a C-style block comment that ignores all content across multiple lines until after a start symbol until an end
symbol appears, use /* to begin the comment and */ to end the comment.
11.3.5. Labels
Labels use the following forms at the start of a line:
<symbol>:
PUBLIC <symbol>:
 TIP
A label is really just an automatic .define with a value set to the current program instruction offset. A PUBLIC label is
exposed to the user code in the same way as a PUBLIC .define.
11.3.6. Instructions
All pioasm instructions follow a common pattern:
<instruction> (side <side_set_value>) ([<delay_value>])
where:
<instruction> An assembly instruction detailed in the following sections. (see Section 11.4)
<side_set_value> A value (see Section 11.3.2) to apply to the side_set pins at the start of the instruction. Note that
the rules for a side-set value via side <side_set_value> are dependent on the .side_set (see
pioasm_side_set) directive for the program. If no .side_set is specified then the side <side_set_value>
is invalid, if an optional number of sideset pins is specified then side <side_set_value> may be
present, and if a non-optional number of sideset pins is specified, then side <side_set_value> is
required. The <side_set_value> must fit within the number of side-set bits specified in the .side_set
directive.
RP2350 Datasheet
11.3. PIO assembler (pioasm) 888
<delay_value> Specifies the number of cycles to delay after the instruction completes. The delay_value is
specified as a value (see Section 11.3.2), and in general is between 0 and 31 inclusive (a 5-bit
value), however the number of bits is reduced when sideset is enabled via the .side_set (see
pioasm_side_set) directive. If the <delay_value> is not present, then the instruction has no delay.
 NOTE
pioasm instruction names, keywords and directives are case insensitive; lower case is used in the Assembly Syntax
sections below, as this is the style used in the SDK.
 NOTE
Commas appear in some Assembly Syntax sections below, but are entirely optional, e.g. out pins, 3 may be written
out pins 3, and jmp x-- label may be written as jmp x--, label. The Assembly Syntax sections below uses the first
style in each case as this is the style used in the SDK.
11.3.7. Pseudo-instructions
pioasm provides aliases for certain instructions, as a convenience:
nop Assembles to mov y, y. No side effect, but a useful vehicle for a side-set operation or an extra delay.
11.4. Instruction Set
11.4.1. Summary
PIO instructions are 16 bits long, and use the following encoding:
Table 980. PIO
instruction encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
JMP 0 0 0 Delay/side-set Condition Address
WAIT 0 0 1 Delay/side-set Pol Source Index
IN 0 1 0 Delay/side-set Source Bit count
OUT 0 1 1 Delay/side-set Destination Bit count
PUSH 1 0 0 Delay/side-set 0 IfF Blk 0 0 0 0 0
MOV 1 0 0 Delay/side-set 0 0 0 1 IdxI 0 Index
PULL 1 0 0 Delay/side-set 1 IfE Blk 0 0 0 0 0
MOV 1 0 0 Delay/side-set 1 0 0 1 IdxI 0 Index
MOV 1 0 1 Delay/side-set Destination Op Source
IRQ 1 1 0 Delay/side-set 0 Clr Wait IdxMode Index
SET 1 1 1 Delay/side-set Destination Data
All PIO instructions execute in one clock cycle.
The function of the 5-bit Delay/side-set field depends on the state machine’s SIDESET_COUNT configuration:
• Up to 5 LSBs (5 minus SIDESET_COUNT) encode a number of idle cycles inserted between this instruction and the next.
RP2350 Datasheet
11.4. Instruction Set 889
• Up to 5 MSBs, set by SIDESET_COUNT, encode a side-set (Section 11.5.1), which can assert a constant onto some
GPIOs, concurrently with main instruction execution.
11.4.2. JMP
11.4.2.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
JMP 0 0 0 Delay/side-set Condition Address
11.4.2.2. Operation
Set program counter to Address if Condition is true, otherwise no operation.
Delay cycles on a JMP always take effect, whether Condition is true or false, and they take place after Condition is
evaluated and the program counter is updated.
• Condition:
◦
000: (no condition): Always
◦
001: !X: scratch X zero
◦
010: X--: scratch X non-zero, prior to decrement
◦
011: !Y: scratch Y zero
◦
100: Y--: scratch Y non-zero, prior to decrement
◦
101: X!=Y: scratch X not equal scratch Y
◦
110: PIN: branch on input pin
◦
111: !OSRE: output shift register not empty
• Address: Instruction address to jump to. In the instruction encoding this is an absolute address within the PIO
instruction memory
JMP PIN branches on the GPIO selected by EXECCTRL_JMP_PIN, a configuration field which selects one out of the maximum
of 32 GPIO inputs visible to a state machine, independently of the state machine’s other input mapping. The branch is
taken if the GPIO is high.
!OSRE compares the bits shifted out since the last PULL with the shift count threshold configured by SHIFTCTRL_PULL_THRESH.
This is the same threshold used by autopull (Section 11.5.4).
JMP X-- and JMP Y-- always decrement scratch register X or Y, respectively. The decrement is not conditional on the
current value of the scratch register. The branch is conditioned on the initial value of the register, i.e. before the
decrement took place: if the register is initially nonzero, the branch is taken.
11.4.2.3. Assembler syntax
jmp (<cond>) <target>
where:
RP2350 Datasheet
11.4. Instruction Set 890
<cond> An optional condition listed above (e.g. !x for scratch X zero). If a condition code is not specified, the
branch is always taken.
<target> A program label or value (see Section 11.3.2) representing instruction offset within the program (the
first instruction being offset 0). Because the PIO JMP instruction uses absolute addresses in the PIO
instruction memory, JMPs need to be adjusted based on the program load offset at runtime. This is
handled for you when loading a program with the SDK, but care should be taken when encoding JMP
instructions for use by OUT EXEC.
11.4.3. WAIT
11.4.3.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
WAIT 0 0 1 Delay/side-set Pol Source Index
11.4.3.2. Operation
Stall until some condition is met.
Like all stalling instructions (Section 11.2.5), delay cycles begin after the instruction completes. That is, if any delay
cycles are present, they do not begin counting until after the wait condition is met.
• Polarity:
◦
1: wait for a 1.
◦
0: wait for a 0.
• Source: what to wait on. Values are:
◦
00: GPIO: System GPIO input selected by Index. This is an absolute GPIO index, and is not affected by the state
machine’s input IO mapping.
◦
01: PIN: Input pin selected by Index. This state machine’s input IO mapping is applied first, and then Index
selects which of the mapped bits to wait on. In other words, the pin is selected by adding Index to the
PINCTRL_IN_BASE configuration, modulo 32.
◦
10: IRQ: PIO IRQ flag selected by Index
◦
11: JMPPIN: wait on the pin indexed by the PINCTRL_JMP_PIN configuration, plus an Index in the range 0-3, all
modulo 32. Other values of Index are reserved.
• Index: which pin or bit to check.
WAIT x IRQ behaves slightly differently from other WAIT sources:
• If Polarity is 1, the selected IRQ flag is cleared by the state machine upon the wait condition being met.
• The flag index is decoded in the same way as the IRQ index field, decoding down from the two MSBs (aligning with
the IRQ instruction IdxMode field):
◦
00: the three LSBs are used directly to index the IRQ flags in this PIO block.
◦
01 (PREV), the instruction references an IRQ from the next-lower-numbered PIO in the system, wrapping to the
highest-numbered PIO if this is PIO0.
◦
10 (REL), the state machine ID (0…3) is added to the IRQ index, by way of modulo-4 addition on the two LSBs.
For example, state machine 2 with a flag value of 0x11 will wait on flag 3, and a flag value of 0x13 will wait on
flag 1. This allows multiple state machines running the same program to synchronise with each other.
RP2350 Datasheet
11.4. Instruction Set 891
◦
11 (NEXT), the instruction references an IRQ from the next-higher-numbered PIO in the system, wrapping to
PIO0 if this is the highest-numbered PIO.
 CAUTION
WAIT 1 IRQ x should not be used with IRQ flags presented to the interrupt controller, to avoid a race condition with a
system interrupt handler
11.4.3.3. Assembler syntax
wait <polarity> gpio <gpio_num>
wait <polarity> pin <pin_num>
wait <polarity> irq (prev | next) <irq_num> (rel)
wait <polarity> jmppin (+ <pin_offset>)
where:
<polarity> A value (see Section 11.3.2) specifying the polarity (either 0 or 1).
<pin_num> A value (see Section 11.3.2) specifying the input pin number (as mapped by the SM input pin
mapping).
<gpio_num> A value (see Section 11.3.2) specifying the actual GPIO pin number.
<irq_num> (rel) A value (see Section 11.3.2) specifying The IRQ number to wait on (0-7). If rel is present, then the
actual IRQ number used is calculating by replacing the low two bits of the IRQ number (irq_num10)
with the low two bits of the sum (irq_num10 + sm_num10) where sm_num10 is the state machine number.
prev To wait on the IRQ on the next lower numbered PIO block instead of on the current PIO block
next To wait on the IRQ on the next higher numbered PIO block instead of on the current PIO block
<pin_offset> A value (see Section 11.3.2) added to the jmp_pin to get the actual pin number.
11.4.4. IN
11.4.4.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
IN 0 1 0 Delay/side-set Source Bit count
RP2350 Datasheet
11.4. Instruction Set 892
11.4.4.2. Operation
Shift Bit count bits from Source into the Input Shift Register (ISR). Shift direction is configured for each state machine by
SHIFTCTRL_IN_SHIFTDIR. Additionally, increase the input shift count by Bit count, saturating at 32.
• Source:
◦
000: PINS
◦
001: X (scratch register X)
◦
010: Y (scratch register Y)
◦
011: NULL (all zeroes)
◦
100: Reserved
◦
101: Reserved
◦
110: ISR
◦
111: OSR
• Bit count: How many bits to shift into the ISR. 1…32 bits, 32 is encoded as 00000
If automatic push is enabled, IN will also push the ISR contents to the RX FIFO if the push threshold is reached
(SHIFTCTRL_PUSH_THRESH). IN still executes in one cycle, whether an automatic push takes place or not. The state machine
will stall if the RX FIFO is full when an automatic push occurs. An automatic push clears the ISR contents to all-zeroes,
and clears the input shift count. See Section 11.5.4.
IN always uses the least significant Bit count bits of the source data. For example, if PINCTRL_IN_BASE is set to 5, the
instruction IN PINS, 3 will take the values of pins 5, 6 and 7, and shift these into the ISR. First the ISR is shifted to the left
or right to make room for the new input data, then the input data is copied into the gap this leaves. The bit order of the
input data is not dependent on the shift direction.
NULL can be used for shifting the ISR’s contents. For example, UARTs receive the LSB first, so must shift to the right.
After 8 IN PINS, 1 instructions, the input serial data will occupy bits 31…24 of the ISR. An IN NULL, 24 instruction will shift
in 24 zero bits, aligning the input data at ISR bits 7…0. Alternatively, the processor or DMA could perform a byte read
from FIFO address + 3, which would take bits 31…24 of the FIFO contents.
11.4.4.3. Assembler syntax
in <source>, <bit_count>
where:
<source> One of the sources specified above.
<bit_count> A value (see Section 11.3.2) specifying the number of bits to shift (valid range 1-32).
11.4.5. OUT
11.4.5.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
OUT 0 1 1 Delay/side-set Destination Bit count
RP2350 Datasheet
11.4. Instruction Set 893
11.4.5.2. Operation
Shift Bit count bits out of the Output Shift Register (OSR), and write those bits to Destination. Additionally, increase the
output shift count by Bit count, saturating at 32.
• Destination:
◦
000: PINS
◦
001: X (scratch register X)
◦
010: Y (scratch register Y)
◦
011: NULL (discard data)
◦
100: PINDIRS
◦
101: PC
◦
110: ISR (also sets ISR shift counter to Bit count)
◦
111: EXEC (Execute OSR shift data as instruction)
• Bit count: how many bits to shift out of the OSR. 1…32 bits, 32 is encoded as 00000
A 32-bit value is written to Destination: the lower Bit count bits come from the OSR, and the remainder are zeroes. This
value is the least significant Bit count bits of the OSR if SHIFTCTRL_OUT_SHIFTDIR is to the right, otherwise it is the most
significant bits.
PINS and PINDIRS use the OUT pin mapping, as described in Section 11.5.6.
If automatic pull is enabled, the OSR is automatically refilled from the TX FIFO if the pull threshold, SHIFTCTRL_PULL_THRESH,
is reached. The output shift count is simultaneously cleared to 0. In this case, the OUT will stall if the TX FIFO is empty,
but otherwise still executes in one cycle. The specifics are given in Section 11.5.4.
OUT EXEC allows instructions to be included inline in the FIFO datastream. The OUT itself executes on one cycle, and the
instruction from the OSR is executed on the next cycle. There are no restrictions on the types of instructions which can
be executed by this mechanism. Delay cycles on the initial OUT are ignored, but the executee may insert delay cycles as
normal.
OUT PC behaves as an unconditional jump to an address shifted out from the OSR.
11.4.5.3. Assembler syntax
out <destination>, <bit_count>
where:
<destination> One of the destinations specified above.
<bit_count> A value (see Section 11.3.2) specifying the number of bits to shift (valid range 1-32).
11.4.6. PUSH
11.4.6.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
PUSH 1 0 0 Delay/side-set 0 IfF Blk 0 0 0 0 0
RP2350 Datasheet
11.4. Instruction Set 894
11.4.6.2. Operation
Push the contents of the ISR into the RX FIFO, as a single 32-bit word. Clear ISR to all-zeroes.
• IfFull: If 1, do nothing unless the total input shift count has reached its threshold, SHIFTCTRL_PUSH_THRESH (the same
as for autopush; see Section 11.5.4).
• Block: If 1, stall execution if RX FIFO is full.
PUSH IFFULL helps to make programs more compact, like autopush. It is useful in cases where the IN would stall at an
inappropriate time if autopush were enabled, e.g. if the state machine is asserting some external control signal at this
point.
The PIO assembler sets the Block bit by default. If the Block bit is not set, the PUSH does not stall on a full RX FIFO, instead
continuing immediately to the next instruction. The FIFO state and contents are unchanged when this happens. The ISR
is still cleared to all-zeroes, and the FDEBUG_RXSTALL flag is set (the same as a blocking PUSH or autopush to a full RX FIFO)
to indicate data was lost.
 NOTE
The operation of the PUSH instruction is undefined when SM0_SHIFTCTRL.FJOIN_RX_PUT or FJOIN_RX_GET is
set — see Section 11.4.8 and Section 11.4.9 for details of the PUT and GET instruction which can be used in this state.
11.4.6.3. Assembler syntax
push (iffull)
push (iffull) block
push (iffull) noblock
where:
iffull Equivalent to IfFull == 1 above. i.e. the default if this is not specified is IfFull == 0.
block Equivalent to Block == 1 above. This is the default if neither block nor noblock is specified.
noblock Equivalent to Block == 0 above.
11.4.7. PULL
11.4.7.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
PULL 1 0 0 Delay/side-set 1 IfE Blk 0 0 0 0 0
RP2350 Datasheet
11.4. Instruction Set 895
11.4.7.2. Operation
Load a 32-bit word from the TX FIFO into the OSR.
• IfEmpty: If 1, do nothing unless the total output shift count has reached its threshold, SHIFTCTRL_PULL_THRESH (the
same as for autopull; see Section 11.5.4).
• Block: If 1, stall if TX FIFO is empty. If 0, pulling from an empty FIFO copies scratch X to OSR.
Some peripherals (UART, SPI, etc.) should halt when no data is available, and pick it up as it comes in; others (I2S)
should clock continuously, and it is better to output placeholder or repeated data than to stop clocking. This can be
achieved with the Block parameter.
A non-blocking PULL on an empty FIFO has the same effect as MOV OSR, X. The program can either preload scratch
register X with a suitable default, or execute a MOV X, OSR after each PULL NOBLOCK, so that the last valid FIFO word will be
recycled until new data is available.
PULL IFEMPTY is useful if an OUT with autopull would stall in an inappropriate location when the TX FIFO is empty. IfEmpty
permits some of the same program simplifications as autopull: for example, the elimination of an outer loop counter.
However, the stall occurs at a controlled point in the program.
 NOTE
When autopull is enabled, any PULL instruction is a no-op when the OSR is full, so that the PULL instruction behaves as
a barrier. OUT NULL, 32 can be used to explicitly discard the OSR contents. See Section 11.5.4.2 for more detail.
11.4.7.3. Assembler syntax
pull (ifempty)
pull (ifempty) block
pull (ifempty) noblock
where:
ifempty Equivalent to IfEmpty == 1 above. i.e. the default if this is not specified is IfEmpty == 0.
block Equivalent to Block == 1 above. This is the default if neither block nor noblock is specified.
noblock Equivalent to Block == 0 above.
11.4.8. MOV (to RX)
11.4.8.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
MOV 1 0 0 Delay/side-set 0 0 0 1 IdxI Index
RP2350 Datasheet
11.4. Instruction Set 896
11.4.8.2. Operation
Write the ISR to a selected RX FIFO entry. The state machine can write the RX FIFO entries in any order, indexed either
by the Y register, or an immediate Index in the instruction. Requires the SHIFTCTRL_FJOIN_RX_PUT configuration field to be
set, otherwise its operation is undefined. The FIFO configuration can be specified for the program via the .fifo directive
(see pioasm_fifo).
If IdxI (index by immediate) is set, the RX FIFO’s registers are indexed by the two least-significant bits of the Index
operand. Otherwise, they are indexed by the two least-significant bits of the Y register. When IdxI is clear, all non-zero
values of Index are reserved encodings, and their operation is undefined.
When only SHIFTCTRL_FJOIN_RX_PUT is set (in SM0_SHIFTCTRL through SM3_SHIFTCTRL), the system can also read the RX
FIFO registers with random access via RXF0_PUTGET0 through RXF0_PUTGET3 (where RXFx indicates which state
machine’s FIFO is being accessed). In this state, the FIFO register storage is repurposed as status registers, which the
state machine can update at any time and the system can read at any time. For example, a quadrature decoder program
could maintain the current step count in a status register at all times, rather than pushing to the RX FIFO and potentially
blocking.
When both SHIFTCTRL_FJOIN_RX_PUT and SHIFTCTRL_FJOIN_RX_GET are set, the system can no longer access the RX FIFO
storage registers, but the state machine can now put/get the registers in arbitrary order, allowing them to be used as
additional scratch storage.
 NOTE
The RX FIFO storage registers have only a single read port and write port, and access through each port is assigned
to only one of (system, state machine) at any time.
11.4.8.3. Assembler syntax
mov rxfifo[y], isr
mov rxfifo[<index>], isr
where:
y The literal token "y", indicating the RX FIFO entry is indexed by the Y register.
<index> A value (see Section 11.3.2) specifying the RX FIFO entry to write (valid range 0-3).
11.4.9. MOV (from RX)
11.4.9.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
MOV 1 0 0 Delay/side-set 1 0 0 1 IdxI Index
RP2350 Datasheet
11.4. Instruction Set 897
11.4.9.2. Operation
Read the selected RX FIFO entry into the OSR. The PIO state machine can read the FIFO entries in any order, indexed
either by the Y register, or an immediate Index in the instruction. Requires the SHIFTCTRL_FJOIN_RX_GET configuration field
to be set, otherwise its operation is undefined.
If IdxI (index by immediate) is set, the RX FIFO’s registers are indexed by the two least-significant bits of the Index
operand. Otherwise, they are indexed by the two least-significant bits of the Y register. When IdxI is clear, all non-zero
values of Index are reserved encodings, and their operation is undefined.
When only SHIFTCTRL_FJOIN_RX_GET is set, the system can also write the RX FIFO registers with random access via
RXF0_PUTGET0 through RXF0_PUTGET3 (where RXFx indicates which state machine’s FIFO is being accessed). In this
state, the RX FIFO register storage is repurposed as additional configuration registers, which the system can update at
any time and the state machine can read at any time. For example, a UART TX program might use these registers to
configure the number of data bits, or the presence of an additional stop bit.
When both SHIFTCTRL_FJOIN_RX_PUT and SHIFTCTRL_FJOIN_RX_GET are set, the system can no longer access the RX FIFO
storage registers, but the state machine can now put/get the registers in arbitrary order, allowing them to be used as
additional scratch storage.
 NOTE
The RX FIFO storage registers have only a single read port and write port, and access through each port is assigned
to only one of (system, state machine) at any time.
11.4.9.3. Assembler syntax
mov osr, rxfifo[y]
mov osr, rxfifo[<index>]
where:
y The literal token "y", indicating the RX FIFO entry is indexed by the Y register.
<index> A value (see Section 11.3.2) specifying the RX FIFO entry to read (valid range 0-3).
11.4.10. MOV
11.4.10.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
MOV 1 0 1 Delay/side-set Destination Op Source
11.4.10.2. Operation
Copy data from Source to Destination.
• Destination:
RP2350 Datasheet
11.4. Instruction Set 898
◦
000: PINS (Uses same pin mapping as OUT)
◦
001: X (Scratch register X)
◦
010: Y (Scratch register Y)
◦
011: PINDIRS (Uses same pin mapping as OUT)
◦
100: EXEC (Execute data as instruction)
◦
101: PC
◦
110: ISR (Input shift counter is reset to 0 by this operation, i.e. empty)
◦
111: OSR (Output shift counter is reset to 0 by this operation, i.e. full)
• Operation:
◦
00: None
◦
01: Invert (bitwise complement)
◦
10: Bit-reverse
◦
11: Reserved
• Source:
◦
000: PINS (Uses same pin mapping as IN)
◦
001: X
◦
010: Y
◦
011: NULL
◦
100: Reserved
◦
101: STATUS
◦
110: ISR
◦
111: OSR
MOV PC causes an unconditional jump. MOV EXEC has the same behaviour as OUT EXEC (Section 11.4.5), and allows register
contents to be executed as an instruction. The MOV itself executes in 1 cycle, and the instruction in Source on the next
cycle. Delay cycles on MOV EXEC are ignored, but the executee may insert delay cycles as normal.
The STATUS source has a value of all-ones or all-zeroes, depending on some state machine status such as FIFO
full/empty, configured by EXECCTRL_STATUS_SEL.
MOV can manipulate the transferred data in limited ways, specified by the Operation argument. Invert sets each bit in
Destination to the logical NOT of the corresponding bit in Source, i.e. 1 bits become 0 bits, and vice versa. Bit reverse sets
each bit n in Destination to bit 31 - n in Source, assuming the bits are numbered 0 to 31.
MOV dst, PINS reads pins using the IN pin mapping, masked to the number of bits specified by SHIFTCTRL_IN_COUNT. The LSB
of the read value is the pin indicated by PINCTRL_IN_BASE, and each successive bit comes from a higher-numbered pin,
wrapping after 31. Result bits greater than the width specified by SHIFTCTRL_IN_COUNT configuration are 0.
MOV PINDIRS, src is not supported on PIO version 0.
11.4.10.3. Assembler syntax
mov <destination>, (op) <source>
where:
RP2350 Datasheet
11.4. Instruction Set 899
<destination> One of the destinations specified above.
op If present, is:
! or ~ for NOT (Note: this is always a bitwise NOT)
:: for bit reverse
<source> One of the sources specified above.
11.4.11. IRQ
11.4.11.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
IRQ 1 1 0 Delay/side-set 0 Clr Wait IdxMode Index
11.4.11.2. Operation
Set or clear the IRQ flag selected by Index argument.
• Clear: if 1, clear the flag selected by Index, instead of raising it. If Clear is set, the Wait bit has no effect.
• Wait: if 1, halt until the raised flag is lowered again, e.g. if a system interrupt handler has acknowledged the flag.
• Index: specifies an IRQ index from 0-7. This IRQ flag will be set/cleared depending on the Clear bit.
• IdxMode: modify the behaviour if the Index field, either modifying the index, or indexing IRQ flags from a different
PIO block:
◦
00: the three LSBs are used directly to index the IRQ flags in this PIO block.
◦
01 (PREV): the instruction references an IRQ flag from the next-lower-numbered PIO in the system, wrapping to
the highest-numbered PIO if this is PIO0.
◦
10 (REL): the state machine ID (0…3) is added to the IRQ flag index, by way of modulo-4 addition on the two
LSBs. For example, state machine 2 with a flag value of '0x11' will wait on flag 3, and a flag value of '0x13' will
wait on flag 1. This allows multiple state machines running the same program to synchronise with each other.
◦
11 (NEXT): the instruction references an IRQ flag from the next-higher-numbered PIO in the system, wrapping to
PIO0 if this is the highest-numbered PIO.
All IRQ flags 0-7 can be routed out to system level interrupts, on either of the PIO’s two external interrupt request lines,
configured by IRQ0_INTE and IRQ1_INTE.
The modulo addition mode (REL) allows relative addressing of 'IRQ' and 'WAIT' instructions, for synchronising state
machines which are running the same program. Bit 2 (the third LSB) is unaffected by this addition.
The NEXT/PREV modes can be used to synchronise between state machines in different PIO blocks. If these state
machines' clocks are divided, their clock dividers must be the same, and must have been synchronised by writing
CTRL.NEXTPREV_CLKDIV_RESTART in addition to the relevant NEXT_PIO_MASK/PREV_PIO_MASK bits. Note that the
cross-PIO connection is severed between PIOs with different accessibility to Non-secure code, as per ACCESSCTRL.
If Wait is set, Delay cycles do not begin until after the wait period elapses.
RP2350 Datasheet
11.4. Instruction Set 900
11.4.11.3. Assembler syntax
irq (prev | next) <irq_num> (rel)
irq (prev | next) set <irq_num> (rel)
irq (prev | next) nowait <irq_num> (rel)
irq (prev | next) wait <irq_num> (rel)
irq (prev | next) clear <irq_num> (rel)
where:
<irq_num> (rel) A value (see Section 11.3.2) specifying the IRQ number to target (0-7). If rel is present, then the
actual IRQ number used is calculated by replacing the low two bits of the IRQ number (irq_num10)
with the low two bits of the sum (irq_num10 + sm_num10) where sm_num10 is the state machine number.
irq Set the IRQ without waiting.
irq set Set the IRQ without waiting.
irq nowait Set the IRQ without waiting.
irq wait Set the IRQ and wait for it to be cleared before proceeding.
irq clear Clear the IRQ.
prev To target the IRQ on the next lower numbered PIO block instead of the current PIO block
next To target the IRQ on the next higher numbered PIO block instead of the current PIO block
11.4.12. SET
11.4.12.1. Encoding
Bit 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
SET 1 1 1 Delay/side-set Destination Data
11.4.12.2. Operation
Write immediate value Data to Destination.
• Destination:
RP2350 Datasheet
11.4. Instruction Set 901
◦
000: PINS
◦
001: X (scratch register X) 5 LSBs are set to Data, all others cleared to 0.
◦
010: Y (scratch register Y) 5 LSBs are set to Data, all others cleared to 0.
◦
011: Reserved
◦
100: PINDIRS
◦
101: Reserved
◦
110: Reserved
◦
111: Reserved
• Data: 5-bit immediate value to drive to pins or register.
This can be used to assert control signals such as a clock or chip select, or to initialise loop counters. As Data is 5 bits in
size, scratch registers can be SET to values from 0-31, which is sufficient for a 32-iteration loop.
The mapping of SET and OUT onto pins is configured independently. They may be mapped to distinct locations, for
example if one pin is to be used as a clock signal, and another for data. They may also be overlapping ranges of pins: a
UART transmitter might use SET to assert start and stop bits, and OUT instructions to shift out FIFO data to the same pins.
11.4.12.3. Assembler syntax
set <destination>, <value>
where:
<destination> Is one of the destinations specified above.
<value> The value (see Section 11.3.2) to set (valid range 0-31).
11.5. Functional details
11.5.1. Side-set
Side-set is a feature that allows state machines to change the level or direction of up to 5 pins, concurrently with the
main execution of the instruction.
One example where this is necessary is a fast SPI interface: here a clock transition (toggling 1→0 or 0→1) must be
simultaneous with a data transition, where a new data bit is shifted from the OSR to a GPIO. In this case an OUT with a
side-set would achieve both of these at once.
This makes the timing of the interface more precise, reduces the overall program size (as a separate SET instruction is
not needed to toggle the clock pin), and also increases the maximum frequency the SPI can run at.
Side-set also makes GPIO mapping much more flexible, as its mapping is independent from SET. The example I2C code
allows SDA and SCL to be mapped to any two arbitrary pins, if clock stretching is disabled. Normally, SCL toggles to
synchronise data transfer, and SDA contains the data bits being shifted out. However, some particular I2C sequences
such as Start and Stop line conditions, need a fixed pattern to be driven on SDA as well as SCL. The mapping I2C uses to
achieve this is:
• Side-set → SCL
RP2350 Datasheet
11.5. Functional details 902
• OUT → SDA
• SET → SDA
This lets the state machine serve the two use cases of data on SDA and clock on SCL, or fixed transitions on both SDA
and SCL, while still allowing SDA and SCL to be mapped to any two GPIOs of choice.
The side-set data is encoded in the Delay/side-set field of each instruction. Any instruction can be combined with sideset, including instructions which write to the pins, such as OUT PINS or SET PINS. Side-set’s pin mapping is independent
from OUT and SET mappings, though it may overlap. If side-set and an OUT or SET write to the same pin simultaneously, the
side-set data is used.
 NOTE
If an instruction stalls, the side-set still takes effect immediately.
1 .program spi_tx_fast
2 .side_set 1
3
4 loop:
5 out pins, 1 side 0
6 jmp loop side 1
The spi_tx_fast example shows two benefits of this: data and clock transitions can be more precisely co-aligned, and
programs can be made faster overall, with an output of one bit per two system clock cycles in this case. Programs can
also be made smaller.
There are four things to configure when using side-set:
• The number of MSBs of the Delay/side-set field to use for side-set rather than delay. This is configured by
PINCTRL_SIDESET_COUNT. If this is set to 5, delay cycles are not available. If set to 0, no side-set will take place.
• Whether to use the most significant of these bits as an enable. Side-set takes place on instructions where the
enable is high. If there is no enable bit, every instruction on that state machine will perform a side-set, if
SIDESET_COUNT is nonzero. This is configured by EXECCTRL_SIDE_EN.
• The GPIO number to map the least-significant side-set bit to. Configured by PINCTRL_SIDESET_BASE.
• Whether side-set writes to GPIO levels or GPIO directions. Configured by EXECCTRL_SIDE_PINDIR
In the above example, we have only one side-set data bit, and every instruction performs a side-set, so no enable bit is
required. SIDESET_COUNT would be 1, SIDE_EN would be false. SIDE_PINDIR would also be false, as we want to drive the clock
high and low, not high- and low-impedance. SIDESET_BASE would select the GPIO the clock is driven from.
11.5.2. Program wrapping
PIO programs often have an "outer loop": they perform the same sequence of steps, repetitively, as they transfer a
stream of data between the FIFOs and the outside world. The square wave program from the introduction is a minimal
example of this:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave.pio Lines 8 - 13
 8 .program squarewave
 9 set pindirs, 1 ; Set pin to output
10 again:
11 set pins, 1 [1] ; Drive pin high and then delay for one cycle
12 set pins, 0 ; Drive pin low
13 jmp again ; Set PC to label `again`
RP2350 Datasheet
11.5. Functional details 903
The main body of the program drives a pin high, and then low, producing one period of a square wave. The entire
program then loops, driving a periodic output. The jump itself takes one cycle, as does each set instruction, so to keep
the high and low periods of the same duration, the set pins, 1 has a single delay cycle added, which makes the state
machine idle for one cycle before executing the set pins, 0 instruction. In total, each loop takes four cycles. There are
two frustrations here:
• The JMP takes up space in the instruction memory that could be used for other programs
• The extra cycle taken to execute the JMP ends up halving the maximum output rate
As the Program Counter (PC) naturally wraps to 0 when incremented past 31, we could solve the second of these by
filling the entire instruction memory with a repeating pattern of set pins, 1 and set pins, 0, but this is wasteful. State
machines have a hardware feature, configured via their EXECCTRL control register, which solves this common case.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave_wrap.pio Lines 12 - 20
12 .program squarewave_wrap
13 ; Like squarewave, but use the state machine's .wrap hardware instead of an
14 ; explicit jmp. This is a free (0-cycle) unconditional jump.
15
16 set pindirs, 1 ; Set pin to output
17 .wrap_target
18 set pins, 1 [1] ; Drive pin high and then delay for one cycle
19 set pins, 0 [1] ; Drive pin low and then delay for one cycle
20 .wrap
After executing an instruction from the program memory, state machines use the following logic to update PC:
1. If the current instruction is a JMP, and the Condition is true, set PC to the Target
2. Otherwise, if PC matches EXECCTRL_WRAP_TOP, set PC to EXECCTRL_WRAP_BOTTOM
3. Otherwise, increment PC, or set to 0 if the current value is 31.
The .wrap_target and .wrap assembly directives in pioasm are essentially labels. They export constants which can be
written to the WRAP_BOTTOM and WRAP_TOP control fields, respectively:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/generated/squarewave_wrap.pio.h
 1 // -------------------------------------------------- //
 2 // This file is autogenerated by pioasm; do not edit! //
 3 // -------------------------------------------------- //
 4
 5 #pragma once
 6
 7 #include "hardware/pio.h"
 8
 9 // --------------- //
10 // squarewave_wrap //
11 // --------------- //
12
13 #define squarewave_wrap_wrap_target 1
14 #define squarewave_wrap_wrap 2
15 #define squarewave_wrap_pio_version 0
16
17 static const uint16_t squarewave_wrap_program_instructions[] = {
18 0xe081, // 0: set pindirs, 1
19 // .wrap_target
20 0xe101, // 1: set pins, 1 [1]
21 0xe100, // 2: set pins, 0 [1]
22 // .wrap
23 };
24
RP2350 Datasheet
11.5. Functional details 904
25 static const struct pio_program squarewave_wrap_program = {
26 .instructions = squarewave_wrap_program_instructions,
27 .length = 3,
28 .origin = -1,
29 .pio_version = squarewave_wrap_pio_version,
30 .used_gpio_ranges = 0x0
31 #endif
32 };
33
34 static inline pio_sm_config squarewave_wrap_program_get_default_config(uint offset) {
35 pio_sm_config c = pio_get_default_sm_config();
36 sm_config_set_wrap(&c, offset + squarewave_wrap_wrap_target, offset +
  squarewave_wrap_wrap);
37 return c;
38 }
This is raw output from the PIO assembler, pioasm, which has created a default pio_sm_config object containing the WRAP
register values from the program listing. The control register fields could also be initialised directly.
 NOTE
WRAP_BOTTOM and WRAP_TOP are absolute addresses in the PIO instruction memory. If a program is loaded at an offset,
the wrap addresses must be adjusted accordingly.
The squarewave_wrap example has delay cycles inserted, so that it behaves identically to the original squarewave program.
Thanks to program wrapping, these can now be removed, so that the output toggles twice as fast, while maintaining an
even balance of high and low periods.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/squarewave/squarewave_fast.pio Lines 12 - 18
12 .program squarewave_fast
13 ; Like squarewave_wrap, but remove the delay cycles so we can run twice as fast.
14 set pindirs, 1 ; Set pin to output
15 .wrap_target
16 set pins, 1 ; Drive pin high
17 set pins, 0 ; Drive pin low
18 .wrap
11.5.3. FIFO joining
By default, each state machine possesses a 4-entry FIFO in each direction: one for data transfer from system to state
machine (TX), the other for the reverse direction (RX). However, many applications do not require bidirectional data
transfer between the system and an individual state machine, but may benefit from deeper FIFOs: in particular, highbandwidth interfaces such as DPI. For these cases, SHIFTCTRL_FJOIN can merge the two 4-entry FIFOs into a single 8-entry
FIFO.
RP2350 Datasheet
11.5. Functional details 905
Figure 48. Joinable
dual FIFO. A pair of
four-entry FIFOs,
implemented with four
data registers, a 1:4
decoder and a 4:1
multiplexer. Additional
multiplexing allows
write data and read
data to cross between
the TX and RX lanes,
so that all 8 entries
are accessible from
both ports
Another example is a UART: because the TX/CTS and RX/RTS parts a of a UART are asynchronous, they are
implemented on two separate state machines. It would be wasteful to leave half of each state machine’s FIFO
resources idle. The ability to join the two halves into just a TX FIFO for the TX/CTS state machine, or just an RX FIFO in
the case of the RX/RTS state machine, allows full utilisation. A UART equipped with an 8-deep FIFO can be left alone for
twice as long between interrupts as one with only a 4-deep FIFO.
When one FIFO is increased in size (from 4 to 8), the other FIFO on that state machine is reduced to zero. For example, if
joining to TX, the RX FIFO is unavailable, and any PUSH instruction will stall. The RX FIFO will appear both RXFULL and
RXEMPTY in the FSTAT register. The converse is true if joining to RX: the TX FIFO is unavailable, and the TXFULL and TXEMPTY
bits for this state machine will both be set in FSTAT. Setting both FJOIN_RX and FJOIN_TX makes both FIFOs unavailable.
8 FIFO entries is sufficient for 1 word per clock through the RP2350 system DMA, provided the DMA is not slowed by
contention with other masters.
 CAUTION
Changing FJOIN discards any data present in the state machine’s FIFOs. If this data is irreplaceable, it must be
drained beforehand.
11.5.4. Autopush and Autopull
With each OUT instruction, the OSR gradually empties, as data is shifted out. Once empty, it must be refilled: for example,
a PULL transfers one word of data from the TX FIFO to the OSR. Similarly, the ISR must be emptied once full. One
approach to this is a loop which performs a PULL after an appropriate amount of data has been shifted:
 1 .program manual_pull
 2 .side_set 1 opt
 3
 4 .wrap_target
 5 set x, 2 ; X = bit count - 2
 6 pull side 1 [1] ; Stall here if no TX data
 7 bitloop:
 8 out pins, 1 side 0 [1] ; Shift out data bit and toggle clock low
 9 jmp x-- bitloop side 1 [1] ; Loop runs 3 times
10 out pins, 1 side 0 ; Shift out last bit before reloading X
11 .wrap
This program shifts out 4 bits from each FIFO word, with an accompanying bit clock, at a constant rate of 1 bit per 4
cycles. When the TX FIFO is empty, it stalls with the clock high (noting that side-set still takes place on cycles where the
RP2350 Datasheet
11.5. Functional details 906
instruction stalls). Figure 49 shows how a state machine would execute this program.
System Clock
32 0 2 3 4
2 1 0 -1 2
1
Instruction
Scratch X
Clock pin (side -set)
OSR shift count
SET PULL OUT JMP OUT JMP OUT JMP OUT SET PULL
Bit 0 Bit 1 Bit 2 Bit 3 Data pin (OUT)
Figure 49. Execution
of manual_pull
program. X is used as
a loop counter. On
each iteration, one
data bit is shifted out,
and the clock is
asserted low, then
high. A delay cycle on
each instruction
brings the total up to
four cycles per
iteration. After the
third loop, a fourth bit
is shifted out, and the
state machine
immediately returns to
the start of the
program to reload the
loop counter and pull
fresh data, while
maintaining the 4
cycles/bit cadence.
This program has some limitations:
• It occupies 5 instruction slots, but only 2 of these are immediately useful (out pins, 1 set 0 and … set 1), for
outputting serial data and a clock.
• Throughput is limited to system clock over 4, due to the extra cycles required to pull in new data, and reload the
loop counter.
This is a common type of problem for PIO, so each state machine has some extra hardware to handle it. State machines
keep track of the total shift count OUT of the OSR and IN to the ISR, and trigger certain actions once these counters reach
a programmable threshold.
• On an OUT instruction which reaches or exceeds the pull threshold, the state machine can simultaneously refill the
OSR from the TX FIFO, if data is available.
• On an IN instruction which reaches or exceeds the push threshold, the state machine can write the shift result
directly to the RX FIFO, and clear the ISR.
The manual_pull example can be rewritten to take advantage of automatic pull (autopull):
1 .program autopull
2 .side_set 1
3
4 .wrap_target
5 out pins, 1 side 0 [1]
6 nop side 1 [1]
7 .wrap
This is shorter and simpler than the original, and can run twice as fast, if the delay cycles are removed, since the
hardware refills the OSR "for free". Note that the program does not determine the total number of bits to be shifted
before the next pull; the hardware automatically pulls once the programmable threshold, SHIFCTRL_PULL_THRESH, is reached,
so the same program could also shift out e.g. 16 or 32 bits from each FIFO word.
Finally, note that the above program is not exactly the same as the original, since it stalls with the clock output low,
rather than high. We can change the location of the stall, using the PULL IFEMPTY instruction, which uses the same
configurable threshold as autopull:
1 .program somewhat_manual_pull
2 .side_set 1
3
4 .wrap_target
5 out pins, 1 side 0 [1]
6 pull ifempty side 1 [1]
7 .wrap
Below is a complete example (PIO program, plus a C program to load and run it) which illustrates autopull and autopush
both enabled on the same state machine. It programs state machine 0 to loopback data from the TX FIFO to the RX
FIFO, with a throughput of one word per two clocks. It also demonstrates how the state machine will stall if it tries to OUT
when both the OSR and TX FIFO are empty.
RP2350 Datasheet
11.5. Functional details 907
1 .program auto_push_pull
2
3 .wrap_target
4 out x, 32
5 in x, 32
6 .wrap
 1 #include "tb.h" // TODO this is built against existing sw tree, so that we get printf etc
 2
 3 #include "platform.h"
 4 #include "pio_regs.h"
 5 #include "system.h"
 6 #include "hardware.h"
 7
 8 #include "auto_push_pull.pio.h"
 9
10 int main()
11 {
12 tb_init();
13
14 // Load program and configure state machine 0 for autopush/pull with
15 // threshold of 32, and wrapping on program boundary. A threshold of 32 is
16 // encoded by a register value of 00000.
17 for (int i = 0; i < count_of(auto_push_pull_program); ++i)
18 mm_pio->instr_mem[i] = auto_push_pull_program[i];
19 mm_pio->sm[0].shiftctrl =
20 (1u << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
21 (1u << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB) |
22 (0u << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB) |
23 (0u << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
24 mm_pio->sm[0].execctrl =
25 (auto_push_pull_wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
26 (auto_push_pull_wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
27
28 // Start state machine 0
29 hw_set_bits(&mm_pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + 0));
30
31 // Push data into TX FIFO, and pop from RX FIFO
32 for (int i = 0; i < 5; ++i)
33 mm_pio->txf[0] = i;
34 for (int i = 0; i < 5; ++i)
35 printf("%d\n", mm_pio->rxf[0]);
36
37 return 0;
38 }
Figure 50 shows how the state machine executes the example program. Initially the OSR is empty, so the state machine
stalls on the first OUT instruction. Once data is available in the TX FIFO, the state machine transfers this into the OSR. On
the next cycle, the OUT can execute using the data in the OSR (in this case, transferring this data to the X scratch
register), and the state machine simultaneously refills the OSR with fresh data from the FIFO. Since every IN instruction
immediately fills the ISR, the ISR remains empty, and IN transfers data directly from scratch X to the RX FIFO.
RP2350 Datasheet
11.5. Functional details 908
clock
32 0 0 0 0 0 32
0 0 0 0 0 0
1 2 3 4 5
Current Instruction
Stall
TX FIFO Empty
TX FIFO Pop
OSR Count (0=full)
RX FIFO Push
ISR Count (0=empty)
RX FIFO Push
OUT IN OUT IN OUT IN OUT IN OUT IN OUT
Figure 50. Execution
of auto_push_pull
program. The state
machine stalls on an
OUT until data has
travelled through the
TX FIFO into the OSR.
Subsequently, the OSR
is refilled
simultaneously with
each OUT operation
(due to bit count of
32), and IN data
bypasses the ISR and
goes straight to the RX
FIFO. The state
machine stalls again
when the FIFO has
drained, and the OSR
is once again empty.
To trigger automatic push or pull at the correct time, the state machine tracks the total shift count of the ISR and OSR,
using a pair of saturating 6-bit counters.
• At reset, or upon CTRL_SM_RESTART assertion, ISR shift counter is set to 0 (nothing shifted in), and OSR to 32 (nothing
left to be shifted out)
• An OUT instruction increases the OSR shift counter by Bit count
• An IN instruction increases the ISR shift counter by Bit count
• A PULL instruction or autopull clears the OSR counter to 0
• A PUSH instruction or autopush clears the ISR counter to 0
• A MOV OSR, x or MOV ISR, x clears the OSR or ISR shift counter to 0, respectively
• A OUT ISR, n instruction sets the ISR shift counter to n
On any OUT or IN instruction, the state machine compares the shift counters to the values of SHIFTCTRL_PULL_THRESH and
SHIFTCTRL_PUSH_THRESH to decide whether action is required. Autopull and autopush are individually enabled by the
SHIFTCTRL_AUTOPULL and SHIFTCTRL_AUTOPUSH fields.
11.5.4.1. Autopush details
Pseudocode for an IN with autopush enabled:
 1 isr = shift_in(isr, input())
 2 isr count = saturate(isr count + in count)
 3
 4 if rx count >= threshold:
 5 if rx fifo is full:
 6 stall
 7 else:
 8 push(isr)
 9 isr = 0
10 isr count = 0
The hardware performs the above steps in a single machine clock cycle, unless there is a stall.
Threshold is configurable from 1 to 32.
RP2350 Datasheet
11.5. Functional details 909
 IMPORTANT
Autopush must not be enabled when SHIFTCTRL_FJOIN_RX_PUT or SHIFTCTRL_FJOIN_RX_GET is set. Its operation in this state
is undefined.
11.5.4.2. Autopull Details
On non-OUT cycles, the hardware performs the equivalent of the following pseudocode:
1 if MOV or PULL:
2 osr count = 0
3
4 if osr count >= threshold:
5 if tx fifo not empty:
6 osr = pull()
7 osr count = 0
An autopull can therefore occur at any point between two OUTs, depending on when the data arrives in the FIFO.
On OUT cycles, the sequence is a little different:
 1 if osr count >= threshold:
 2 if tx fifo not empty:
 3 osr = pull()
 4 osr count = 0
 5 stall
 6 else:
 7 output(osr)
 8 osr = shift(osr, out count)
 9 osr count = saturate(osr count + out count)
10
11 if osr count >= threshold:
12 if tx fifo not empty:
13 osr = pull()
14 osr count = 0
The hardware is capable of refilling the OSR simultaneously with shifting out the last of the shift data, as these two
operations can proceed in parallel. However, it cannot fill an empty OSR and OUT it on the same cycle, due to the long
logic path this would create.
The refill is somewhat asynchronous to your program, but an OUT behaves as a data fence, and the state machine will
never OUT data which you didn’t write into the FIFO.
Note that a MOV from the OSR is undefined whilst autopull is enabled; you will read either any residual data that has not
been shifted out, or a fresh word from the FIFO, depending on a race against system DMA. Likewise, a MOV to the OSR
may overwrite data which has just been autopulled. However, data which you MOV into the OSR will never be overwritten,
since MOV updates the shift counter.
If you do need to read the OSR contents, you should perform an explicit PULL of some kind. The nondeterminism
described above is the cost of the hardware managing pulls automatically. When autopull is enabled, the behaviour of
PULL is altered: it becomes a no-op if the OSR is full. This is to avoid a race condition against the system DMA. It behaves
as a fence: either an autopull has already taken place, in which case the PULL has no effect, or the program will stall on
the PULL until data becomes available in the FIFO.
PULL does not require similar behaviour, because autopush does not have the same nondeterminism.
RP2350 Datasheet
11.5. Functional details 910
11.5.5. Clock Dividers
PIO runs off the system clock, but this is too fast for many interfaces, and the number of Delay cycles which can be
inserted is limited. Some devices, such as UART, require the signalling rate to be precisely controlled and varied, and
ideally multiple state machines can be varied independently while running identical programs. Each state machine is
equipped with a clock divider, for this purpose.
Rather than slowing the system clock itself, the clock divider redefines how many system clock periods are considered
to be "one cycle", for execution purposes. It does this by generating a clock enable signal, which can pause and resume
execution on a per-system-clock-cycle basis. The clock divider generates clock enable pulses at regular intervals, so
that the state machine runs at some steady pace, potentially much slower than the system clock.
Implementing the clock dividers in this way allows interfacing between the state machines and the system to be
simpler, lower-latency, and with a smaller footprint. The state machine is completely idle on cycles where clock enable
is low, though the system can still access the state machine’s FIFOs and change its configuration.
The clock dividers are 16-bit integer, 8-bit fractional, with first-order delta-sigma for the fractional divider. The clock
divisor can vary between 1 and 65536, in increments of .
If the clock divisor is set to 1, the state machine runs on every cycle, i.e. full speed:
System Clock
CLKDIV_INT
CLKDIV_FRAC
Clock Enable
CTRL_SM_ENABLE
1
.0
Figure 51. State
machine operation
with a clock divisor of
1. Once the state
machine is enabled via
the CTRL register, its
clock enable is
asserted on every
cycle.
In general, an integer clock divisor of n will cause the state machine to run 1 cycle in every n, giving an effective clock
speed of .
System Clock
CLKDIV_INT
CLKDIV_FRAC
Clock Enable
CTRL_SM_ENABLE
2
.0
Figure 52. Integer
clock divisors yield a
periodic clock enable.
The clock divider
repeatedly counts
down from n, and
emits an enable pulse
when it reaches 1. Fractional division will maintain a steady state division rate of , where n and f are the integer and fractional
fields of this state machine’s CLKDIV register. It does this by selectively extending some division periods from cycles to
.
System Clock
CLKDIV_INT
CLKDIV_FRAC
Clock Enable
CTRL_SM_ENABLE
2
.5
Figure 53. Fractional
clock division with an
average divisor of 2.5.
The clock divider
maintains a running
total of the fractional
value from each
division period, and
every time this value
wraps through 1, the
integer divisor is
increased by one for
the next division
period.
For small n, the jitter introduced by a fractional divider may be unacceptable. However, for larger values, this effect is
much less apparent.
 NOTE
For fast asynchronous serial, it is recommended to use even divisions or multiples of 1 Mbaud where possible,
rather than the traditional multiples of 300, to avoid unnecessary jitter.
11.5.6. GPIO mapping
Internally, PIO has a 32-bit register for the output levels of each GPIO it can drive, and another register for the output
enables (Hi/Lo-Z). On every system clock cycle, each state machine can write to some or all of the GPIOs in each of
these registers.
RP2350 Datasheet
11.5. Functional details 911
Figure 54. The state
machine has two
independent output
channels, one shared
by OUT/SET, and
another used by sideset (which can happen
at any time). Three
independent mappings
(first GPIO, number of
GPIOs) control which
GPIOs OUT, SET and
side-set are directed
to. Input data is
rotated according to
which GPIO is mapped
to the LSB of the IN
data.
The write data and write masks for the output level and output enable registers come from the following sources:
• An OUT instruction writes to up to 32 bits. Depending on the instruction’s Destination field, this is applied to either
pins or pindirs. The least-significant bit of OUT data is mapped to PINCTRL_OUT_BASE, and this mapping continues for
PINCTRL_OUT_COUNT bits, wrapping after GPIO31.
• A SET instruction writes up to 5 bits. Depending on the instruction’s Destination field, this is applied to either pins or
pindirs. The least-significant bit of SET data is mapped to PINCTRL_SET_BASE, and this mapping continues for
PINCTRL_SET_COUNT bits, wrapping after GPIO31.
• A side-set operation writes up to 5 bits. Depending on the register field EXECCTRL_SIDE_PINDIR, this is applied to either
pins or pindirs. The least-significant bit of side-set data is mapped to PINCTRL_SIDESET_BASE, continuing for
PINCTRL_SIDESET_COUNT pins, minus one if EXECCTRL_SIDE_EN is set.
Each OUT/SET/side-set operation writes to a contiguous range of pins, but each of these ranges is independently sized
and positioned in the 32-bit GPIO space. This is sufficiently flexible for many applications. For example, if one state
machine is implementing some interface such as an SPI on a group of pins, another state machine can run the same
program, mapped to a different group of pins, and provide a second SPI interface.
On any given clock cycle, the state machine may perform an OUT or a SET, and may simultaneously perform a side-set.
The pin mapping logic generates a 32-bit write mask and write data bus for the output level and output enable registers,
based on this request, and the pin mapping configuration.
If a side-set overlaps with an OUT/SET performed by that state machine on the same cycle, the side-set takes precedence
in the overlapping region.
11.5.6.1. Output priority
Figure 55. Per-GPIO
priority select of write
masks from each
state machine. Each
GPIO considers level
and direction writes
from each of the four
state machines, and
applies the value from
the highest-numbered
state machine.
Each state machine may assert an OUT/SET and a side-set through its pin mapping hardware on each cycle. This
generates 32 bits of write data and write mask for the GPIO output level and output enable registers, from each state
machine.
For each GPIO, PIO collates the writes from all four state machines, and applies the write from the highest-numbered
RP2350 Datasheet
11.5. Functional details 912
state machine. This occurs separately for output levels and output values — it is possible for a state machine to change
both the level and direction of the same pin on the same cycle (e.g. via simultaneous SET and side-set), or for one state
machine to change a GPIO’s direction while another changes that GPIO’s level. If no state machine asserts a write to a
GPIO’s level or direction, the value does not change.
11.5.6.2. Input mapping
The data observed by IN instructions is mapped such that the LSB is the GPIO selected by PINCTRL_IN_BASE, and
successively more-significant bits come from successively higher-numbered GPIOs, wrapping after 31.
In other words, the IN bus is a right-rotate of the GPIO input values, by PINCTRL_IN_BASE. If fewer than 32 GPIOs are
present, the PIO input is padded with zeroes up to 32 bits.
Some instructions, such as WAIT GPIO, use an absolute GPIO number, rather than an index into the IN data bus. In this
case, the right-rotate is not applied.
11.5.6.3. Input synchronisers
To protect PIO from metastabilities, each GPIO input is equipped with a standard 2-flipflop synchroniser. This adds two
cycles of latency to input sampling, but the benefit is that state machines can perform an IN PINS at any point, and will
see only a clean high or low level, not some intermediate value that could disturb the state machine circuitry. This is
absolutely necessary for asynchronous interfaces such as UART RX.
It is possible to bypass these synchronisers, on a per-GPIO basis. This reduces input latency, but it is then up to the user
to guarantee that the state machine does not sample its inputs at inappropriate times. Generally this is only possible for
synchronous interfaces such as SPI. Synchronisers are bypassed by setting the corresponding bit in INPUT_SYNC_BYPASS.
 WARNING
Sampling a metastable input can lead to unpredictable state machine behaviour. This should be avoided.
11.5.7. Forced and EXEC’d instructions
Besides the instruction memory, state machines can execute instructions from 3 other sources:
• MOV EXEC which executes an instruction from some register Source
• OUT EXEC which executes data shifted out from the OSR
• The SMx_INSTR control registers, to which the system can write instructions for immediate execution
 1 .program exec_example
 2
 3 hang:
 4 jmp hang
 5 execute:
 6 out exec, 32
 7 jmp execute
 8
 9 .program instructions_to_push
10
11 out x, 32
12 in x, 32
13 push
RP2350 Datasheet
11.5. Functional details 913
 1 #include "tb.h" // TODO this is built against existing sw tree, so that we get printf etc
 2
 3 #include "platform.h"
 4 #include "pio_regs.h"
 5 #include "system.h"
 6 #include "hardware.h"
 7
 8 #include "exec_example.pio.h"
 9
10 int main()
11 {
12 tb_init();
13
14 for (int i = 0; i < count_of(exec_example_program); ++i)
15 mm_pio->instr_mem[i] = exec_example_program[i];
16
17 // Enable autopull, threshold of 32
18 mm_pio->sm[0].shiftctrl = (1u << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB);
19
20 // Start state machine 0 -- will sit in "hang" loop
21 hw_set_bits(&mm_pio->ctrl, 1u << (PIO_CTRL_SM_ENABLE_LSB + 0));
22
23 // Force a jump to program location 1
24 mm_pio->sm[0].instr = 0x0000 | 0x1; // jmp execute
25
26 // Feed a mixture of instructions and data into FIFO
27 mm_pio->txf[0] = instructions_to_push_program[0]; // out x, 32
28 mm_pio->txf[0] = 12345678; // data to be OUTed
29 mm_pio->txf[0] = instructions_to_push_program[1]; // in x, 32
30 mm_pio->txf[0] = instructions_to_push_program[2]; // push
31
32 // The program pushed into TX FIFO will return some data in RX FIFO
33 while (mm_pio->fstat & (1u << PIO_FSTAT_RXEMPTY_LSB))
34 ;
35
36 printf("%d\n", mm_pio->rxf[0]);
37
38 return 0;
39 }
Here we load an example program into the state machine, which does two things:
• Enters an infinite loop
• Enters a loop which repeatedly pulls 32 bits of data from the TX FIFO, and executes the lower 16 bits as an
instruction
The C program sets the state machine running, at which point it enters the hang loop. While the state machine is still
running, the C program forces in a jmp instruction, which causes the state machine to break out of the loop.
When an instruction is written to the INSTR register, the state machine immediately decodes and executes that
instruction, rather than the instruction it would have fetched from the PIO’s instruction memory. The program counter
does not advance, so on the next cycle (assuming the instruction forced into the INSTR interface did not stall) the state
machine continues to execute its current program from the point where it left off, unless the written instruction itself
manipulated PC.
Delay cycles are ignored on instructions written to the INSTR register, and execute immediately, ignoring the state
machine clock divider. This interface is provided for performing initial setup and effecting control flow changes, so it
executes instructions in a timely manner, no matter how the state machine is configured.
Instructions written to the INSTR register are permitted to stall, in which case the state machine will latch this instruction
internally until it completes. This is signified by the EXECCTRL_EXEC_STALLED flag. This can be cleared by restarting the state
RP2350 Datasheet
11.5. Functional details 914
machine, or writing a NOP to INSTR.
In the second phase of the example state machine program, the OUT EXEC instruction is used. The OUT itself occupies one
execution cycle, and the instruction which the OUT executes is on the next execution cycle. Note that one of the
instructions we execute is also an OUT — the state machine is only capable of executing one OUT instruction on any given
cycle.
OUT EXEC works by writing the OUT shift data to an internal instruction latch. On the next cycle, the state machine
remembers it must execute from this latch rather than the instruction memory, and also knows to not advance PC on this
second cycle.
This program will print "12345678" when run.
 CAUTION
If an instruction written to INSTR stalls, it is stored in the same instruction latch used by OUT EXEC and MOV EXEC, and will
overwrite an in-progress instruction there. If EXEC instructions are used, instructions written to INSTR must not stall.
11.6. Examples
These examples illustrate some of PIO’s hardware features, by implementing common I/O interfaces.
 TIP
Raspberry Pi Pico-series C/C++ SDK has a comprehensive PIO chapter that begins with writing and building your
first PIO application. Later chapters walk through some programs line-by-line. Finally, it covers broader topics such
as using PIO with DMA, and how PIO can integrate into your software.
11.6.1. Duplex SPI
RP2350 Datasheet
11.6. Examples 915
Figure 56. In SPI, a
host and device
exchange data over a
bidirectional pair of
serial data lines,
synchronous with a
clock (SCK). Two
flags, CPOL and
CPHA, specify the
clock’s behaviour.
CPOL is the idle state
of the clock: 0 for low,
1 for high. The clock
pulses a number of
times, transferring one
bit in each direction
per pulse, but always
returns to its idle
state. CPHA
determines on which
edge of the clock data
is captured: 0 for
leading edge, and 1 for
trailing edge. The
arrows in the figure
show the clock edge
where data is captured
by both the host and
device.
SPI is a common serial interface with a twisty history. The following program implements full-duplex (i.e. transferring
data in both directions simultaneously) SPI, with a CPHA parameter of 0.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/spi.pio Lines 14 - 32
14 .program spi_cpha0
15 .side_set 1
16
17 ; Pin assignments:
18 ; - SCK is side-set pin 0
19 ; - MOSI is OUT pin 0
20 ; - MISO is IN pin 0
21 ;
22 ; Autopush and autopull must be enabled, and the serial frame size is set by
23 ; configuring the push/pull threshold. Shift left/right is fine, but you must
24 ; justify the data yourself. This is done most conveniently for frame sizes of
25 ; 8 or 16 bits by using the narrow store replication and narrow load byte
26 ; picking behaviour of RP2040's IO fabric.
27
28 ; Clock phase = 0: data is captured on the leading edge of each SCK pulse, and
29 ; transitions on the trailing edge, or some time before the first leading edge.
30
31 out pins, 1 side 0 [1] ; Stall here on empty (sideset proceeds even if
32 in pins, 1 side 1 [1] ; instruction stalls, so we stall with SCK low)
This code uses autopush and autopull to continuously stream data from the FIFOs. The entire program runs once for
every bit that is transferred, and then loops. The state machine tracks how many bits have been shifted in/out, and
automatically pushes/pulls the FIFOs at the correct point. A similar program handles the CPHA=1 case:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/spi.pio Lines 34 - 42
34 .program spi_cpha1
35 .side_set 1
36
37 ; Clock phase = 1: data transitions on the leading edge of each SCK pulse, and
38 ; is captured on the trailing edge.
39
40 out x, 1 side 0 ; Stall here on empty (keep SCK deasserted)
41 mov pins, x side 1 [1] ; Output data, assert SCK (mov pins uses OUT mapping)
42 in pins, 1 side 0 ; Input data, deassert SCK
RP2350 Datasheet
11.6. Examples 916
 NOTE
These programs do not control the chip select line; chip select is often implemented as a software-controlled GPIO,
due to wildly different behaviour between different SPI hardware. The full spi.pio source linked above contains some
examples how PIO can implement a hardware chip select line.
A C helper function configures the state machine, connects the GPIOs, and sets the state machine running. Note that
the SPI frame size — that is, the number of bits transferred for each FIFO record — can be programmed to any value
from 1 to 32, without modifying the program. Once configured, the state machine is set running.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/spi.pio Lines 46 - 72
46 static inline void pio_spi_init(PIO pio, uint sm, uint prog_offs, uint n_bits,
47 float clkdiv, bool cpha, bool cpol, uint pin_sck, uint pin_mosi, uint pin_miso) {
48 pio_sm_config c = cpha ? spi_cpha1_program_get_default_config(prog_offs) :
  spi_cpha0_program_get_default_config(prog_offs);
49 sm_config_set_out_pins(&c, pin_mosi, 1);
50 sm_config_set_in_pins(&c, pin_miso);
51 sm_config_set_sideset_pins(&c, pin_sck);
52 // Only support MSB-first in this example code (shift to left, auto push/pull,
  threshold=nbits)
53 sm_config_set_out_shift(&c, false, true, n_bits);
54 sm_config_set_in_shift(&c, false, true, n_bits);
55 sm_config_set_clkdiv(&c, clkdiv);
56
57 // MOSI, SCK output are low, MISO is input
58 pio_sm_set_pins_with_mask(pio, sm, 0, (1u << pin_sck) | (1u << pin_mosi));
59 pio_sm_set_pindirs_with_mask(pio, sm, (1u << pin_sck) | (1u << pin_mosi), (1u << pin_sck)
  | (1u << pin_mosi) | (1u << pin_miso));
60 pio_gpio_init(pio, pin_mosi);
61 pio_gpio_init(pio, pin_miso);
62 pio_gpio_init(pio, pin_sck);
63
64 // The pin muxes can be configured to invert the output (among other things
65 // and this is a cheesy way to get CPOL=1
66 gpio_set_outover(pin_sck, cpol ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
67 // SPI is synchronous, so bypass input synchroniser to reduce input delay.
68 hw_set_bits(&pio->input_sync_bypass, 1u << pin_miso);
69
70 pio_sm_init(pio, sm, prog_offs, &c);
71 pio_sm_set_enabled(pio, sm, true);
72 }
The state machine will now immediately begin to shift out any data appearing in the TX FIFO, and push received data
into the RX FIFO.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/pio_spi.c Lines 18 - 34
18 void __time_critical_func(pio_spi_write8_blocking)(const pio_spi_inst_t *spi, const uint8_t
  *src, size_t len) {
19 size_t tx_remain = len, rx_remain = len;
20 // Do 8 bit accesses on FIFO, so that write data is byte-replicated. This
21 // gets us the left-justification for free (for MSB-first shift-out)
22 io_rw_8 *txfifo = (io_rw_8 *) &spi->pio->txf[spi->sm];
23 io_rw_8 *rxfifo = (io_rw_8 *) &spi->pio->rxf[spi->sm];
24 while (tx_remain || rx_remain) {
25 if (tx_remain && !pio_sm_is_tx_fifo_full(spi->pio, spi->sm)) {
26 *txfifo = *src++;
27 --tx_remain;
28 }
RP2350 Datasheet
11.6. Examples 917
29 if (rx_remain && !pio_sm_is_rx_fifo_empty(spi->pio, spi->sm)) {
30 (void) *rxfifo;
31 --rx_remain;
32 }
33 }
34 }
Putting this all together, this complete C program will loop back some data through a PIO SPI at 1 MHz, with all four
CPOL/CPHA combinations:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/spi/spi_loopback.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdlib.h>
 8 #include <stdio.h>
 9
10 #include "pico/stdlib.h"
11 #include "pio_spi.h"
12
13 // This program instantiates a PIO SPI with each of the four possible
14 // CPOL/CPHA combinations, with the serial input and output pin mapped to the
15 // same GPIO. Any data written into the state machine's TX FIFO should then be
16 // serialised, deserialised, and reappear in the state machine's RX FIFO.
17
18 #define PIN_SCK 18
19 #define PIN_MOSI 16
20 #define PIN_MISO 16 // same as MOSI, so we get loopback
21
22 #define BUF_SIZE 20
23
24 void test(const pio_spi_inst_t *spi) {
25 static uint8_t txbuf[BUF_SIZE];
26 static uint8_t rxbuf[BUF_SIZE];
27 printf("TX:");
28 for (int i = 0; i < BUF_SIZE; ++i) {
29 txbuf[i] = rand() >> 16;
30 rxbuf[i] = 0;
31 printf(" %02x", (int) txbuf[i]);
32 }
33 printf("\n");
34
35 pio_spi_write8_read8_blocking(spi, txbuf, rxbuf, BUF_SIZE);
36
37 printf("RX:");
38 bool mismatch = false;
39 for (int i = 0; i < BUF_SIZE; ++i) {
40 printf(" %02x", (int) rxbuf[i]);
41 mismatch = mismatch || rxbuf[i] != txbuf[i];
42 }
43 if (mismatch)
44 printf("\nNope\n");
45 else
46 printf("\nOK\n");
47 }
48
49 int main() {
50 stdio_init_all();
RP2350 Datasheet
11.6. Examples 918
51
52 pio_spi_inst_t spi = {
53 .pio = pio0,
54 .sm = 0
55 };
56 float clkdiv = 31.25f; // 1 MHz @ 125 clk_sys
57 uint cpha0_prog_offs = pio_add_program(spi.pio, &spi_cpha0_program);
58 uint cpha1_prog_offs = pio_add_program(spi.pio, &spi_cpha1_program);
59
60 for (int cpha = 0; cpha <= 1; ++cpha) {
61 for (int cpol = 0; cpol <= 1; ++cpol) {
62 printf("CPHA = %d, CPOL = %d\n", cpha, cpol);
63 pio_spi_init(spi.pio, spi.sm,
64 cpha ? cpha1_prog_offs : cpha0_prog_offs,
65 8, // 8 bits per SPI frame
66 clkdiv,
67 cpha,
68 cpol,
69 PIN_SCK,
70 PIN_MOSI,
71 PIN_MISO
72 );
73 test(&spi);
74 sleep_ms(10);
75 }
76 }
77 }
11.6.2. WS2812 LEDs
WS2812 LEDs are driven by a proprietary pulse-width serial format, with a wide positive pulse representing a "1" bit, and
narrow positive pulse a "0". Each LED has a serial input and a serial output; LEDs are connected in a chain, with each
serial input connected to the previous LED’s serial output.
Symbol
Output
1 0 0 1 Latch
Figure 57. WS2812
line format. Wide
positive pulse for 1,
narrow positive pulse
for 0, very long
negative pulse for
latch enable
LEDs consume 24 bits of pixel data, then pass any additional input data on to their output. In this way a single serial
burst can individually program the colour of each LED in a chain. A long negative pulse latches the pixel data into the
LEDs.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812.pio Lines 8 - 31
 8 .program ws2812
 9 .side_set 1
10
11 ; The following constants are selected for broad compatibility with WS2812,
12 ; WS2812B, and SK6812 LEDs. Other constants may support higher bandwidths for
13 ; specific LEDs, such as (7,10,8) for WS2812B LEDs.
14
15 .define public T1 3
16 .define public T2 3
17 .define public T3 4
18
19 .lang_opt python sideset_init = pico.PIO.OUT_HIGH
20 .lang_opt python out_init = pico.PIO.OUT_HIGH
21 .lang_opt python out_shiftdir = 1
22
23 .wrap_target
RP2350 Datasheet
11.6. Examples 919
24 bitloop:
25 out x, 1 side 0 [T3 - 1] ; Side-set still takes place when instruction stalls
26 jmp !x do_zero side 1 [T1 - 1] ; Branch on the bit we shifted out. Positive pulse
27 do_one:
28 jmp bitloop side 1 [T2 - 1] ; Continue driving high, for a long pulse
29 do_zero:
30 nop side 0 [T2 - 1] ; Or drive low, for a short pulse
31 .wrap
This program shifts bits from the OSR into X, and produces a wide or narrow pulse on side-set pin 0, based on the value
of each data bit. Autopull must be configured, with a threshold of 24. Software can then write 24-bit pixel values into the
FIFO, and these will be serialised to a chain of WS2812 LEDs. The .pio file contains a C helper function to set this up:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812.pio Lines 36 - 52
36 static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq,
  bool rgbw) {
37
38 pio_gpio_init(pio, pin);
39 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
40
41 pio_sm_config c = ws2812_program_get_default_config(offset);
42 sm_config_set_sideset_pins(&c, pin);
43 sm_config_set_out_shift(&c, false, true, rgbw ? 32 : 24);
44 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
45
46 int cycles_per_bit = ws2812_T1 + ws2812_T2 + ws2812_T3;
47 float div = clock_get_hz(clk_sys) / (freq * cycles_per_bit);
48 sm_config_set_clkdiv(&c, div);
49
50 pio_sm_init(pio, sm, offset, &c);
51 pio_sm_set_enabled(pio, sm, true);
52 }
Because the shift is MSB-first, and our pixels aren’t a power of two size (so we can’t rely on the narrow write replication
behaviour on RP2350 to fan out the bits for us), we need to preshift the values written to the TX FIFO.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812.c Lines 43 - 45
43 static inline void put_pixel(PIO pio, uint sm, uint32_t pixel_grb) {
44 pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
45 }
To DMA the pixels, we could instead set the autopull threshold to 8 bits, set the DMA transfer size to 8 bits, and write a
byte at a time into the FIFO. Each pixel would be 3 one-byte transfers. Because of how the bus fabric and DMA on
RP2350 work, each byte the DMA transfers will appear replicated four times when written to a 32-bit IO register, so
effectively your data is at both ends of the shift register, and you can shift in either direction without worry.
RP2350 Datasheet
11.6. Examples 920
 TIP
The WS2812 example is the subject of a tutorial in the Raspberry Pi Pico-series C/C++ SDK document, in the PIO
chapter. The tutorial dissects the ws2812 program line by line, traces through how the program executes, and shows
wave diagrams of the GPIO output at every point in the program.
11.6.3. UART TX
Bit Clock
TX
State
0 1 2 3 4 5 6 7
Idle Start Data (LSB first) Stop
Figure 58. UART serial
format. The line is
high when idle. The
transmitter pulls the
line down for one bit
period to signify the
start of a serial frame
(the "start bit"), and a
small, fixed number of
data bits follows. The
line returns to the idle
state for at least one
bit period (the "stop
bit") before the next
serial frame can
begin.
This program implements the transmit component of a universal asynchronous receive/transmit (UART) serial
peripheral. Perhaps it would be more correct to refer to this as a UAT.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_tx/uart_tx.pio Lines 8 - 18
 8 .program uart_tx
 9 .side_set 1 opt
10
11 ; An 8n1 UART transmit program.
12 ; OUT pin 0 and side-set pin 0 are both mapped to UART TX pin.
13
14 pull side 1 [7] ; Assert stop bit, or stall with line in idle state
15 set x, 7 side 0 [7] ; Preload bit counter, assert start bit for 8 clocks
16 bitloop: ; This loop will run 8 times (8n1 UART)
17 out pins, 1 ; Shift 1 bit from OSR to the first OUT pin
18 jmp x-- bitloop [6] ; Each loop iteration is 8 cycles.
As written, it will:
1. Stall with the pin driven high until data appears (noting that side-set takes effect even when the state machine is
stalled)
2. Assert a start bit, for 8 SM execution cycles
3. Shift out 8 data bits, each lasting for 8 cycles
4. Return to the idle line state for at least 8 cycles before asserting the next start bit
If the state machine’s clock divider is configured to run at 8 times the desired baud rate, this program will transmit wellformed UART serial frames, whenever data is pushed to the TX FIFO either by software or the system DMA. To extend
the program to cover different frame sizes (different numbers of data bits), the set x, 7 could be replaced with mov x, y,
so that the y scratch register becomes a per-SM configuration register for UART frame size.
The .pio file in the SDK also contains this function, for configuring the pins and the state machine, once the program
has been loaded into the PIO instruction memory:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_tx/uart_tx.pio Lines 24 - 51
24 static inline void uart_tx_program_init(PIO pio, uint sm, uint offset, uint pin_tx, uint
  baud) {
25 // Tell PIO to initially drive output-high on the selected pin, then map PIO
26 // onto that pin with the IO muxes.
27 pio_sm_set_pins_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
28 pio_sm_set_pindirs_with_mask64(pio, sm, 1ull << pin_tx, 1ull << pin_tx);
29 pio_gpio_init(pio, pin_tx);
30
31 pio_sm_config c = uart_tx_program_get_default_config(offset);
RP2350 Datasheet
11.6. Examples 921
32
33 // OUT shifts to right, no autopull
34 sm_config_set_out_shift(&c, true, false, 32);
35
36 // We are mapping both OUT and side-set to the same pin, because sometimes
37 // we need to assert user data onto the pin (with OUT) and sometimes
38 // assert constant values (start/stop bit)
39 sm_config_set_out_pins(&c, pin_tx, 1);
40 sm_config_set_sideset_pins(&c, pin_tx);
41
42 // We only need TX, so get an 8-deep FIFO!
43 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
44
45 // SM transmits 1 bit per 8 execution cycles.
46 float div = (float)clock_get_hz(clk_sys) / (8 * baud);
47 sm_config_set_clkdiv(&c, div);
48
49 pio_sm_init(pio, sm, offset, &c);
50 pio_sm_set_enabled(pio, sm, true);
51 }
The state machine is configured to shift right in out instructions, because UARTs typically send data LSB-first. Once
configured, the state machine will print any characters pushed to the TX FIFO.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_tx/uart_tx.pio Lines 53 - 55
53 static inline void uart_tx_program_putc(PIO pio, uint sm, char c) {
54 pio_sm_put_blocking(pio, sm, (uint32_t)c);
55 }
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_tx/uart_tx.pio Lines 57 - 60
57 static inline void uart_tx_program_puts(PIO pio, uint sm, const char *s) {
58 while (*s)
59 uart_tx_program_putc(pio, sm, *s++);
60 }
The example program in the SDK will configure one PIO state machine as a UART TX peripheral, and use it to print a
message on GPIO 0 at 115200 baud once per second.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_tx/uart_tx.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include "pico/stdlib.h"
 8 #include "hardware/pio.h"
 9 #include "uart_tx.pio.h"
10
11 // We're going to use PIO to print "Hello, world!" on the same GPIO which we
12 // normally attach UART0 to.
13 #define PIO_TX_PIN 0
14
15 // Check the pin is compatible with the platform
16 #error Attempting to use a pin>=32 on a platform that does not support it
RP2350 Datasheet
11.6. Examples 922
17
18 int main() {
19 // This is the same as the default UART baud rate on Pico
20 const uint SERIAL_BAUD = 115200;
21
22 PIO pio;
23 uint sm;
24 uint offset;
25
26 // This will find a free pio and state machine for our program and load it for us
27 // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
28 // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and
  supported by the hardware
29 bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_tx_program, &pio,
  &sm, &offset, PIO_TX_PIN, 1, true);
30 hard_assert(success);
31
32 uart_tx_program_init(pio, sm, offset, PIO_TX_PIN, SERIAL_BAUD);
33
34 while (true) {
35 uart_tx_program_puts(pio, sm, "Hello, world! (from PIO!)\r\n");
36 sleep_ms(1000);
37 }
38
39 // This will free resources and unload our program
40 pio_remove_program_and_unclaim_sm(&uart_tx_program, pio, sm, offset);
41 }
With the two PIO instances on RP2350, this could be extended to 8 additional UART TX interfaces, on 8 different pins,
with 8 different baud rates.
11.6.4. UART RX
Recalling Figure 58 showing the format of an 8n1 UART:
Bit Clock
TX
State
0 1 2 3 4 5 6 7
Idle Start Data (LSB first) Stop
We can recover the data by waiting for the start bit, sampling 8 times with the correct timing, and pushing the result to
the RX FIFO. Below is possibly the shortest program which can do this:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_rx/uart_rx.pio Lines 8 - 19
 8 .program uart_rx_mini
 9
10 ; Minimum viable 8n1 UART receiver. Wait for the start bit, then sample 8 bits
11 ; with the correct timing.
12 ; IN pin 0 is mapped to the GPIO used as UART RX.
13 ; Autopush must be enabled, with a threshold of 8.
14
15 wait 0 pin 0 ; Wait for start bit
16 set x, 7 [10] ; Preload bit counter, delay until eye of first data bit
17 bitloop: ; Loop 8 times
18 in pins, 1 ; Sample data
19 jmp x-- bitloop [6] ; Each iteration is 8 cycles
This works, but it has some annoying characteristics, like repeatedly outputting NUL characters if the line is stuck low.
RP2350 Datasheet
11.6. Examples 923
Ideally, we would want to drop data that is not correctly framed by a start and stop bit (and set some sticky flag to
indicate this has happened), and pause receiving when the line is stuck low for long periods. We can add these to our
program, at the cost of a few more instructions.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_rx/uart_rx.pio Lines 44 - 63
44 .program uart_rx
45
46 ; Slightly more fleshed-out 8n1 UART receiver which handles framing errors and
47 ; break conditions more gracefully.
48 ; IN pin 0 and JMP pin are both mapped to the GPIO used as UART RX.
49
50 start:
51 wait 0 pin 0 ; Stall until start bit is asserted
52 set x, 7 [10] ; Preload bit counter, then delay until halfway through
53 bitloop: ; the first data bit (12 cycles incl wait, set).
54 in pins, 1 ; Shift data bit into ISR
55 jmp x-- bitloop [6] ; Loop 8 times, each loop iteration is 8 cycles
56 jmp pin good_stop ; Check stop bit (should be high)
57
58 irq 4 rel ; Either a framing error or a break. Set a sticky flag,
59 wait 1 pin 0 ; and wait for line to return to idle state.
60 jmp start ; Don't push data if we didn't see good framing.
61
62 good_stop: ; No delay before returning to start; a little slack is
63 push ; important in case the TX clock is slightly too fast.
The second example does not use autopush (Section 11.5.4), preferring instead to use an explicit push instruction, so
that it can condition the push on whether a correct stop bit is seen. The .pio file includes a helper function which
configures the state machine and connects it to a GPIO with the pull-up enabled:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_rx/uart_rx.pio Lines 67 - 85
67 static inline void uart_rx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud) {
68 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
69 pio_gpio_init(pio, pin);
70 gpio_pull_up(pin);
71
72 pio_sm_config c = uart_rx_program_get_default_config(offset);
73 sm_config_set_in_pins(&c, pin); // for WAIT, IN
74 sm_config_set_jmp_pin(&c, pin); // for JMP
75 // Shift to right, autopush disabled
76 sm_config_set_in_shift(&c, true, false, 32);
77 // Deeper FIFO as we're not doing any TX
78 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
79 // SM transmits 1 bit per 8 execution cycles.
80 float div = (float)clock_get_hz(clk_sys) / (8 * baud);
81 sm_config_set_clkdiv(&c, div);
82
83 pio_sm_init(pio, sm, offset, &c);
84 pio_sm_set_enabled(pio, sm, true);
85 }
To correctly receive data which is sent LSB-first, the ISR is configured to shift to the right. After shifting in 8 bits, this
unfortunately leaves our 8 data bits in bits 31:24 of the ISR, with 24 zeroes in the LSBs. One option here is an in null, 24
instruction to shuffle the ISR contents down to 7:0. Another is to read from the FIFO at an offset of 3 bytes, with an 8-bit
read, so that the processor’s bus hardware (or the DMA’s) picks out the relevant byte for free:
RP2350 Datasheet
11.6. Examples 924
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_rx/uart_rx.pio Lines 87 - 93
87 static inline char uart_rx_program_getc(PIO pio, uint sm) {
88 // 8-bit read from the uppermost byte of the FIFO, as data is left-justified
89 io_rw_8 *rxfifo_shift = (io_rw_8*)&pio->rxf[sm] + 3;
90 while (pio_sm_is_rx_fifo_empty(pio, sm))
91 tight_loop_contents();
92 return (char)*rxfifo_shift;
93 }
An example program shows how this UART RX program can be used to receive characters sent by one of the hardware
UARTs on RP2350. A wire must be connected from GPIO4 to GPIO3 for this program to function. To make the wrangling
of 3 different serial ports a little easier, this program uses core 1 to print out a string on the test UART (UART 1), and the
code running on core 0 will pull out characters from the PIO state machine, and pass them along to the UART used for
the debug console (UART 0). Another approach here would be interrupt-based IO, using PIO’s FIFO IRQs. If the
SM0_RXNEMPTY bit is set in the IRQ0_INTE register, then PIO will raise its first interrupt request line whenever there is a
character in state machine 0’s RX FIFO.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/uart_rx/uart_rx.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdio.h>
 8
 9 #include "pico/stdlib.h"
10 #include "pico/multicore.h"
11 #include "hardware/pio.h"
12 #include "hardware/uart.h"
13 #include "uart_rx.pio.h"
14
15 // This program
16 // - Uses UART1 (the spare UART, by default) to transmit some text
17 // - Uses a PIO state machine to receive that text
18 // - Prints out the received text to the default console (UART0)
19 // This might require some reconfiguration on boards where UART1 is the
20 // default UART.
21
22 #define SERIAL_BAUD PICO_DEFAULT_UART_BAUD_RATE
23 #define HARD_UART_INST uart1
24
25 // You'll need a wire from GPIO4 -> GPIO3
26 #define HARD_UART_TX_PIN 4
27 #define PIO_RX_PIN 3
28
29 // Check the pin is compatible with the platform
30 #error Attempting to use a pin>=32 on a platform that does not support it
31
32 // Ask core 1 to print a string, to make things easier on core 0
33 void core1_main() {
34 const char *s = (const char *) multicore_fifo_pop_blocking();
35 uart_puts(HARD_UART_INST, s);
36 }
37
38 int main() {
39 // Console output (also a UART, yes it's confusing)
40 setup_default_uart();
41 printf("Starting PIO UART RX example\n");
RP2350 Datasheet
11.6. Examples 925
42
43 // Set up the hard UART we're going to use to print characters
44 uart_init(HARD_UART_INST, SERIAL_BAUD);
45 gpio_set_function(HARD_UART_TX_PIN, GPIO_FUNC_UART);
46
47 // Set up the state machine we're going to use to receive them.
48 PIO pio;
49 uint sm;
50 uint offset;
51
52 // This will find a free pio and state machine for our program and load it for us
53 // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
54 // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and
  supported by the hardware
55 bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&uart_rx_program, &pio,
  &sm, &offset, PIO_RX_PIN, 1, true);
56 hard_assert(success);
57
58 uart_rx_program_init(pio, sm, offset, PIO_RX_PIN, SERIAL_BAUD);
59 //uart_rx_mini_program_init(pio, sm, offset, PIO_RX_PIN, SERIAL_BAUD);
60
61 // Tell core 1 to print some text to uart1 as fast as it can
62 multicore_launch_core1(core1_main);
63 const char *text = "Hello, world from PIO! (Plus 2 UARTs and 2 cores, for complex
  reasons)\n";
64 multicore_fifo_push_blocking((uint32_t) text);
65
66 // Echo characters received from PIO to the console
67 while (true) {
68 char c = uart_rx_program_getc(pio, sm);
69 putchar(c);
70 }
71
72 // This will free resources and unload our program
73 pio_remove_program_and_unclaim_sm(&uart_rx_program, pio, sm, offset);
74 }
11.6.5. Manchester serial TX and RX
Figure 59. Manchester
serial line code. Each
data bit is represented
by either a high pulse
followed by a low
pulse (representing a
'0' bit) or a low pulse
followed by a high
pulse (a '1' bit).
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/manchester_encoding/manchester_encoding.pio Lines 8 - 30
 8 .program manchester_tx
 9 .side_set 1 opt
10
11 ; Transmit one bit every 12 cycles. a '0' is encoded as a high-low sequence
12 ; (each part lasting half a bit period, or 6 cycles) and a '1' is encoded as a
13 ; low-high sequence.
14 ;
15 ; Side-set bit 0 must be mapped to the GPIO used for TX.
16 ; Autopull must be enabled -- this program does not care about the threshold.
17 ; The program starts at the public label 'start'.
18
19 .wrap_target
20 do_1:
21 nop side 0 [5] ; Low for 6 cycles (5 delay, +1 for nop)
22 jmp get_bit side 1 [3] ; High for 4 cycles. 'get_bit' takes another 2 cycles
RP2350 Datasheet
11.6. Examples 926
23 do_0:
24 nop side 1 [5] ; Output high for 6 cycles
25 nop side 0 [3] ; Output low for 4 cycles
26 public start:
27 get_bit:
28 out x, 1 ; Always shift out one bit from OSR to X, so we can
29 jmp !x do_0 ; branch on it. Autopull refills the OSR when empty.
30 .wrap
Starting from the label called start, this program shifts one data bit at a time into the X register, so that it can branch on
the value. Depending on the outcome, it uses side-set to drive either a 1-0 or 0-1 sequence onto the chosen GPIO. This
program uses autopull (Section 11.5.4.2) to automatically replenish the OSR from the TX FIFO once a certain amount of
data has been shifted out, without interrupting program control flow or timing. This feature is enabled by a helper
function in the .pio file which configures and starts the state machine:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/manchester_encoding/manchester_encoding.pio Lines 33 - 46
33 static inline void manchester_tx_program_init(PIO pio, uint sm, uint offset, uint pin, float
  div) {
34 pio_sm_set_pins_with_mask(pio, sm, 0, 1u << pin);
35 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
36 pio_gpio_init(pio, pin);
37
38 pio_sm_config c = manchester_tx_program_get_default_config(offset);
39 sm_config_set_sideset_pins(&c, pin);
40 sm_config_set_out_shift(&c, true, true, 32);
41 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
42 sm_config_set_clkdiv(&c, div);
43 pio_sm_init(pio, sm, offset + manchester_tx_offset_start, &c);
44
45 pio_sm_set_enabled(pio, sm, true);
46 }
Another state machine can be programmed to recover the original data from the transmitted signal:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/manchester_encoding/manchester_encoding.pio Lines 49 - 71
49 .program manchester_rx
50
51 ; Assumes line is idle low, first bit is 0
52 ; One bit is 12 cycles
53 ; a '0' is encoded as 10
54 ; a '1' is encoded as 01
55 ;
56 ; Both the IN base and the JMP pin mapping must be pointed at the GPIO used for RX.
57 ; Autopush must be enabled.
58 ; Before enabling the SM, it should be placed in a 'wait 1, pin` state, so that
59 ; it will not start sampling until the initial line idle state ends.
60
61 start_of_0: ; We are 0.25 bits into a 0 - signal is high
62 wait 0 pin 0 ; Wait for the 1->0 transition - at this point we are 0.5 into the bit
63 in y, 1 [8] ; Emit a 0, sleep 3/4 of a bit
64 jmp pin start_of_0 ; If signal is 1 again, it's another 0 bit, otherwise it's a 1
65
66 .wrap_target
67 start_of_1: ; We are 0.25 bits into a 1 - signal is 1
68 wait 1 pin 0 ; Wait for the 0->1 transition - at this point we are 0.5 into the bit
69 in x, 1 [8] ; Emit a 1, sleep 3/4 of a bit
70 jmp pin start_of_0 ; If signal is 0 again, it's another 1 bit otherwise it's a 0
RP2350 Datasheet
11.6. Examples 927
71 .wrap
The main complication here is staying aligned to the input transitions, as the transmitter’s and receiver’s clocks may
drift relative to one another. In Manchester code there is always a transition in the centre of the symbol, and based on
the initial line state (high or low) we know the direction of this transition, so we can use a wait instruction to
resynchronise to the line transitions on every data bit.
This program expects the X and Y registers to be initialised with the values 1 and 0 respectively, so that a constant 1 or
0 can be provided to the in instruction. The code that configures the state machine initialises these registers by
executing some set instructions before setting the program running.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/manchester_encoding/manchester_encoding.pio Lines 74 - 94
74 static inline void manchester_rx_program_init(PIO pio, uint sm, uint offset, uint pin, float
  div) {
75 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
76 pio_gpio_init(pio, pin);
77
78 pio_sm_config c = manchester_rx_program_get_default_config(offset);
79 sm_config_set_in_pins(&c, pin); // for WAIT
80 sm_config_set_jmp_pin(&c, pin); // for JMP
81 sm_config_set_in_shift(&c, true, true, 32);
82 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
83 sm_config_set_clkdiv(&c, div);
84 pio_sm_init(pio, sm, offset, &c);
85
86 // X and Y are set to 0 and 1, to conveniently emit these to ISR/FIFO.
87 pio_sm_exec(pio, sm, pio_encode_set(pio_x, 1));
88 pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
89 // Assume line is idle low, and first transmitted bit is 0. Put SM in a
90 // wait state before enabling. RX will begin once the first 0 symbol is
91 // detected.
92 pio_sm_exec(pio, sm, pio_encode_wait_pin(1, 0) | pio_encode_delay(2));
93 pio_sm_set_enabled(pio, sm, true);
94 }
The example C program in the SDK will transmit Manchester serial data from GPIO2 to GPIO3 at approximately 10 Mb/s
(assuming a system clock of 125 MHz).
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/manchester_encoding/manchester_encoding.c Lines 20 - 43
20 int main() {
21 stdio_init_all();
22
23 PIO pio = pio0;
24 uint sm_tx = 0;
25 uint sm_rx = 1;
26
27 uint offset_tx = pio_add_program(pio, &manchester_tx_program);
28 uint offset_rx = pio_add_program(pio, &manchester_rx_program);
29 printf("Transmit program loaded at %d\n", offset_tx);
30 printf("Receive program loaded at %d\n", offset_rx);
31
32 manchester_tx_program_init(pio, sm_tx, offset_tx, pin_tx, 1.f);
33 manchester_rx_program_init(pio, sm_rx, offset_rx, pin_rx, 1.f);
34
35 pio_sm_set_enabled(pio, sm_tx, false);
36 pio_sm_put_blocking(pio, sm_tx, 0);
37 pio_sm_put_blocking(pio, sm_tx, 0x0ff0a55a);
38 pio_sm_put_blocking(pio, sm_tx, 0x12345678);
RP2350 Datasheet
11.6. Examples 928
39 pio_sm_set_enabled(pio, sm_tx, true);
40
41 for (int i = 0; i < 3; ++i)
42 printf("%08x\n", pio_sm_get_blocking(pio, sm_rx));
43 }
11.6.6. Differential Manchester (BMC) TX and RX
Figure 60. Differential
Manchester serial line
code, also known as
biphase mark code
(BMC). The line
transitions at the start
of every bit period.
The presence of a
transition in the centre
of the bit period
signifies a 1 data bit,
and the absence, a 0
bit. These encoding
rules are the same
whether the line has
an initial high or low
state.
The transmit program is similar to the Manchester example: it repeatedly shifts a bit from the OSR into X (relying on
autopull to refill the OSR in the background), branches, and drives a GPIO up and down based on the value of this bit.
The added complication is that the pattern we drive onto the pin depends not just on the value of the data bit, as with
vanilla Manchester encoding, but also on the state the line was left in at the end of the last bit period. This is illustrated
in Figure 60, where the pattern is inverted if the line is initially high. To cope with this, there are two copies of the testand-drive code, one for each initial line state, and these are linked together in the correct order by a sequence of jumps.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/differential_manchester/differential_manchester.pio Lines 8 - 35
 8 .program differential_manchester_tx
 9 .side_set 1 opt
10
11 ; Transmit one bit every 16 cycles. In each bit period:
12 ; - A '0' is encoded as a transition at the start of the bit period
13 ; - A '1' is encoded as a transition at the start *and* in the middle
14 ;
15 ; Side-set bit 0 must be mapped to the data output pin.
16 ; Autopull must be enabled.
17
18 public start:
19 initial_high:
20 out x, 1 ; Start of bit period: always assert transition
21 jmp !x high_0 side 1 [6] ; Test the data bit we just shifted out of OSR
22 high_1:
23 nop
24 jmp initial_high side 0 [6] ; For `1` bits, also transition in the middle
25 high_0:
26 jmp initial_low [7] ; Otherwise, the line is stable in the middle
27
28 initial_low:
29 out x, 1 ; Always shift 1 bit from OSR to X so we can
30 jmp !x low_0 side 0 [6] ; branch on it. Autopull refills OSR for us.
31 low_1:
32 nop
33 jmp initial_low side 1 [6] ; If there are two transitions, return to
34 low_0:
35 jmp initial_high [7] ; the initial line state is flipped!
The .pio file also includes a helper function to initialise a state machine for differential Manchester TX, and connect it to
a chosen GPIO. We arbitrarily choose a 32-bit frame size and LSB-first serialisation (shift_right is true in
sm_config_set_out_shift), but as the program operates on one bit at a time, we could change this by reconfiguring the
state machine.
RP2350 Datasheet
11.6. Examples 929
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/differential_manchester/differential_manchester.pio Lines 38 - 53
38 static inline void differential_manchester_tx_program_init(PIO pio, uint sm, uint offset,
  uint pin, float div) {
39 pio_sm_set_pins_with_mask(pio, sm, 0, 1u << pin);
40 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
41 pio_gpio_init(pio, pin);
42
43 pio_sm_config c = differential_manchester_tx_program_get_default_config(offset);
44 sm_config_set_sideset_pins(&c, pin);
45 sm_config_set_out_shift(&c, true, true, 32);
46 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
47 sm_config_set_clkdiv(&c, div);
48 pio_sm_init(pio, sm, offset + differential_manchester_tx_offset_start, &c);
49
50 // Execute a blocking pull so that we maintain the initial line state until data is
  available
51 pio_sm_exec(pio, sm, pio_encode_pull(false, true));
52 pio_sm_set_enabled(pio, sm, true);
53 }
The RX program uses the following strategy:
1. Wait until the initial transition at the start of the bit period, so we stay aligned to the transmit clock
2. Then, wait 3/4 of the configured bit period, so that we are centred on the second half-bit-period (see Figure 60)
3. Sample the line at this point to determine whether there are one or two transitions in this bit period
4. Repeat
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/differential_manchester/differential_manchester.pio Lines 55 - 85
55 .program differential_manchester_rx
56
57 ; Assumes line is idle low
58 ; One bit is 16 cycles. In each bit period:
59 ; - A '0' is encoded as a transition at time 0
60 ; - A '1' is encoded as a transition at time 0 and a transition at time T/2
61 ;
62 ; The IN mapping and the JMP pin select must both be mapped to the GPIO used for
63 ; RX data. Autopush must be enabled.
64
65 public start:
66 initial_high: ; Find rising edge at start of bit period
67 wait 1 pin, 0 [11] ; Delay to eye of second half-period (i.e 3/4 of way
68 jmp pin high_0 ; through bit) and branch on RX pin high/low.
69 high_1:
70 in x, 1 ; Second transition detected (a `1` data symbol)
71 jmp initial_high
72 high_0:
73 in y, 1 [1] ; Line still high, no centre transition (data is `0`)
74 ; Fall-through
75
76 .wrap_target
77 initial_low: ; Find falling edge at start of bit period
78 wait 0 pin, 0 [11] ; Delay to eye of second half-period
79 jmp pin low_1
80 low_0:
81 in y, 1 ; Line still low, no centre transition (data is `0`)
82 jmp initial_high
83 low_1: ; Second transition detected (data is `1`)
84 in x, 1 [1]
RP2350 Datasheet
11.6. Examples 930
85 .wrap
This code assumes that X and Y have the values 1 and 0, respectively. This is arranged for by the included C helper
function:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/differential_manchester/differential_manchester.pio Lines 88 - 104
 88 static inline void differential_manchester_rx_program_init(PIO pio, uint sm, uint offset,
  uint pin, float div) {
 89 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
 90 pio_gpio_init(pio, pin);
 91
 92 pio_sm_config c = differential_manchester_rx_program_get_default_config(offset);
 93 sm_config_set_in_pins(&c, pin); // for WAIT
 94 sm_config_set_jmp_pin(&c, pin); // for JMP
 95 sm_config_set_in_shift(&c, true, true, 32);
 96 sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
 97 sm_config_set_clkdiv(&c, div);
 98 pio_sm_init(pio, sm, offset, &c);
 99
100 // X and Y are set to 0 and 1, to conveniently emit these to ISR/FIFO.
101 pio_sm_exec(pio, sm, pio_encode_set(pio_x, 1));
102 pio_sm_exec(pio, sm, pio_encode_set(pio_y, 0));
103 pio_sm_set_enabled(pio, sm, true);
104 }
All the pieces now exist to loopback some serial data over a wire between two GPIOs.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/differential_manchester/differential_manchester.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdio.h>
 8
 9 #include "pico/stdlib.h"
10 #include "hardware/pio.h"
11 #include "differential_manchester.pio.h"
12
13 // Differential serial transmit/receive example
14 // Need to connect a wire from GPIO2 -> GPIO3
15
16 const uint pin_tx = 2;
17 const uint pin_rx = 3;
18
19 int main() {
20 stdio_init_all();
21
22 PIO pio = pio0;
23 uint sm_tx = 0;
24 uint sm_rx = 1;
25
26 uint offset_tx = pio_add_program(pio, &differential_manchester_tx_program);
27 uint offset_rx = pio_add_program(pio, &differential_manchester_rx_program);
28 printf("Transmit program loaded at %d\n", offset_tx);
29 printf("Receive program loaded at %d\n", offset_rx);
30
RP2350 Datasheet
11.6. Examples 931
31 // Configure state machines, set bit rate at 5 Mbps
32 differential_manchester_tx_program_init(pio, sm_tx, offset_tx, pin_tx, 125.f / (16 * 5));
33 differential_manchester_rx_program_init(pio, sm_rx, offset_rx, pin_rx, 125.f / (16 * 5));
34
35 pio_sm_set_enabled(pio, sm_tx, false);
36 pio_sm_put_blocking(pio, sm_tx, 0);
37 pio_sm_put_blocking(pio, sm_tx, 0x0ff0a55a);
38 pio_sm_put_blocking(pio, sm_tx, 0x12345678);
39 pio_sm_set_enabled(pio, sm_tx, true);
40
41 for (int i = 0; i < 3; ++i)
42 printf("%08x\n", pio_sm_get_blocking(pio, sm_rx));
43 }
11.6.7. I2C
Figure 61. A 1-byte I2C
read transfer. In the
idle state, both lines
float high. The initiator
drives SDA low (a
Start condition),
followed by 7 address
bits A6-A0, and a
direction bit
(Read/nWrite). The
target drives SDA low
to acknowledge the
address (ACK). Data
bytes follow. The
target serialises data
on SDA, clocked out
by SCL. Every 9th
clock, the initiator
pulls SDA low to
acknowledge the data,
except on the last
byte, where it leaves
the line high (NAK).
Releasing SDA whilst
SCL is high is a Stop
condition, returning
the bus to idle.
I2C is an ubiquitous serial bus first described in the Dead Sea Scrolls, and later used by Philips Semiconductor. Two
wires with pullup resistors form an open-drain bus, and multiple agents address and signal one another over this bus by
driving the bus lines low, or releasing them to be pulled high. It has a number of unusual attributes:
• SCL can be held low at any time, for any duration, by any member of the bus (not necessarily the target or initiator
of the transfer). This is known as clock stretching. The bus does not advance until all drivers release the clock.
• Members of the bus can be a target of one transfer and initiate other transfers (the master/slave roles are not
fixed). However this is poorly supported by most I2C hardware.
• SCL is not an edge-sensitive clock, rather SDA must be valid the entire time SCL is high.
• In spite of the transparency of SDA against SCL, transitions of SDA whilst SCL is high are used to mark beginning
and end of transfers (Start/Stop), or a new address phase within one (Restart).
The PIO program listed below handles serialisation, clock stretching, and checking of ACKs in the initiator role. It
provides a mechanism for escaping PIO instructions in the FIFO datastream, to issue Start/Stop/Restart sequences at
appropriate times. Provided no unexpected NAKs are received, this can perform long sequences of I2C transfers from a
DMA buffer, without processor intervention.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/i2c.pio Lines 8 - 73
 8 .program i2c
 9 .side_set 1 opt pindirs
10
11 ; TX Encoding:
12 ; | 15:10 | 9 | 8:1 | 0 |
13 ; | Instr | Final | Data | NAK |
14 ;
15 ; If Instr has a value n > 0, then this FIFO word has no
16 ; data payload, and the next n + 1 words will be executed as instructions.
17 ; Otherwise, shift out the 8 data bits, followed by the ACK bit.
18 ;
19 ; The Instr mechanism allows stop/start/repstart sequences to be programmed
20 ; by the processor, and then carried out by the state machine at defined points
21 ; in the datastream.
22 ;
23 ; The "Final" field should be set for the final byte in a transfer.
24 ; This tells the state machine to ignore a NAK: if this field is not
25 ; set, then any NAK will cause the state machine to halt and interrupt.
RP2350 Datasheet
11.6. Examples 932
26 ;
27 ; Autopull should be enabled, with a threshold of 16.
28 ; Autopush should be enabled, with a threshold of 8.
29 ; The TX FIFO should be accessed with halfword writes, to ensure
30 ; the data is immediately available in the OSR.
31 ;
32 ; Pin mapping:
33 ; - Input pin 0 is SDA, 1 is SCL (if clock stretching used)
34 ; - Jump pin is SDA
35 ; - Side-set pin 0 is SCL
36 ; - Set pin 0 is SDA
37 ; - OUT pin 0 is SDA
38 ; - SCL must be SDA + 1 (for wait mapping)
39 ;
40 ; The OE outputs should be inverted in the system IO controls!
41 ; (It's possible for the inversion to be done in this program,
42 ; but costs 2 instructions: 1 for inversion, and one to cope
43 ; with the side effect of the MOV on TX shift counter.)
44
45 do_nack:
46 jmp y-- entry_point ; Continue if NAK was expected
47 irq wait 0 rel ; Otherwise stop, ask for help
48
49 do_byte:
50 set x, 7 ; Loop 8 times
51 bitloop:
52 out pindirs, 1 [7] ; Serialise write data (all-ones if reading)
53 nop side 1 [2] ; SCL rising edge
54 wait 1 pin, 1 [4] ; Allow clock to be stretched
55 in pins, 1 [7] ; Sample read data in middle of SCL pulse
56 jmp x-- bitloop side 0 [7] ; SCL falling edge
57
58 ; Handle ACK pulse
59 out pindirs, 1 [7] ; On reads, we provide the ACK.
60 nop side 1 [7] ; SCL rising edge
61 wait 1 pin, 1 [7] ; Allow clock to be stretched
62 jmp pin do_nack side 0 [2] ; Test SDA for ACK/NAK, fall through if ACK
63
64 public entry_point:
65 .wrap_target
66 out x, 6 ; Unpack Instr count
67 out y, 1 ; Unpack the NAK ignore bit
68 jmp !x do_byte ; Instr == 0, this is a data record.
69 out null, 32 ; Instr > 0, remainder of this OSR is invalid
70 do_exec:
71 out exec, 16 ; Execute one instruction per FIFO word
72 jmp x-- do_exec ; Repeat n + 1 times
73 .wrap
The IO mapping required by the I2C program is quite complex, due to the different ways that the two serial lines must be
driven and sampled. One interesting feature is that state machine must drive the output enable high when the output is
low, since the bus is open-drain, so the sense of the data is inverted. This could be handled in the PIO program (e.g. mov
osr, ~osr), but instead we can use the IO controls on RP2350 to perform this inversion in the GPIO muxes, saving an
instruction.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/i2c.pio Lines 81 - 121
 81 static inline void i2c_program_init(PIO pio, uint sm, uint offset, uint pin_sda, uint
  pin_scl) {
 82 assert(pin_scl == pin_sda + 1);
 83 pio_sm_config c = i2c_program_get_default_config(offset);
RP2350 Datasheet
11.6. Examples 933
 84
 85 // IO mapping
 86 sm_config_set_out_pins(&c, pin_sda, 1);
 87 sm_config_set_set_pins(&c, pin_sda, 1);
 88 sm_config_set_in_pins(&c, pin_sda);
 89 sm_config_set_sideset_pins(&c, pin_scl);
 90 sm_config_set_jmp_pin(&c, pin_sda);
 91
 92 sm_config_set_out_shift(&c, false, true, 16);
 93 sm_config_set_in_shift(&c, false, true, 8);
 94
 95 float div = (float)clock_get_hz(clk_sys) / (32 * 100000);
 96 sm_config_set_clkdiv(&c, div);
 97
 98 // Try to avoid glitching the bus while connecting the IOs. Get things set
 99 // up so that pin is driven down when PIO asserts OE low, and pulled up
100 // otherwise.
101 gpio_pull_up(pin_scl);
102 gpio_pull_up(pin_sda);
103 uint32_t both_pins = (1u << pin_sda) | (1u << pin_scl);
104 pio_sm_set_pins_with_mask(pio, sm, both_pins, both_pins);
105 pio_sm_set_pindirs_with_mask(pio, sm, both_pins, both_pins);
106 pio_gpio_init(pio, pin_sda);
107 gpio_set_oeover(pin_sda, GPIO_OVERRIDE_INVERT);
108 pio_gpio_init(pio, pin_scl);
109 gpio_set_oeover(pin_scl, GPIO_OVERRIDE_INVERT);
110 pio_sm_set_pins_with_mask(pio, sm, 0, both_pins);
111
112 // Clear IRQ flag before starting, and make sure flag doesn't actually
113 // assert a system-level interrupt (we're using it as a status flag)
114 pio_set_irq0_source_enabled(pio, (enum pio_interrupt_source) ((uint) pis_interrupt0 +
  sm), false);
115 pio_set_irq1_source_enabled(pio, (enum pio_interrupt_source) ((uint) pis_interrupt0 +
  sm), false);
116 pio_interrupt_clear(pio, sm);
117
118 // Configure and start SM
119 pio_sm_init(pio, sm, offset + i2c_offset_entry_point, &c);
120 pio_sm_set_enabled(pio, sm, true);
121 }
We can also use the PIO assembler to generate a table of instructions for passing through the FIFO, for
Start/Stop/Restart conditions.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/i2c.pio Lines 126 - 136
126 .program set_scl_sda
127 .side_set 1 opt
128
129 ; Assemble a table of instructions which software can select from, and pass
130 ; into the FIFO, to issue START/STOP/RSTART. This isn't intended to be run as
131 ; a complete program.
132
133 set pindirs, 0 side 0 [7] ; SCL = 0, SDA = 0
134 set pindirs, 1 side 0 [7] ; SCL = 0, SDA = 1
135 set pindirs, 0 side 1 [7] ; SCL = 1, SDA = 0
136 set pindirs, 1 side 1 [7] ; SCL = 1, SDA = 1
The example code does blocking software IO on the state machine’s FIFOs, to avoid the extra complexity of setting up
the system DMA. For example, an I2C start condition is enqueued like so:
RP2350 Datasheet
11.6. Examples 934
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/pio_i2c.c Lines 69 - 74
69 void pio_i2c_start(PIO pio, uint sm) {
70 pio_i2c_put_or_err(pio, sm, 2u << PIO_I2C_ICOUNT_LSB); // Escape
  code for 3 instruction sequence
71 pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC1_SD0]); // We are
  already in idle state, just pull SDA low
72 pio_i2c_put_or_err(pio, sm, set_scl_sda_program_instructions[I2C_SC0_SD0]); // Also
  pull clock low so we can present data
73 pio_i2c_put_or_err(pio, sm, pio_encode_mov(pio_isr, pio_null)); // Ensure
  ISR counter is clear following a write
74 }
Because I2C can go wrong at so many points, we need to be able to check the error flag asserted by the state machine,
clear the halt and restart it, before asserting a Stop condition and releasing the bus.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/pio_i2c.c Lines 15 - 17
15 bool pio_i2c_check_error(PIO pio, uint sm) {
16 return pio_interrupt_get(pio, sm);
17 }
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/pio_i2c.c Lines 19 - 23
19 void pio_i2c_resume_after_error(PIO pio, uint sm) {
20 pio_sm_drain_tx_fifo(pio, sm);
21 pio_sm_exec(pio, sm, (pio->sm[sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >>
  PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
22 pio_interrupt_clear(pio, sm);
23 }
We need some higher-level functions to pass correctly-formatted data though the FIFOs and insert Starts, Stops, NAKs
and so on at the correct points. This is enough to present a similar interface to the other hardware I2Cs on RP2350.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/i2c/i2c_bus_scan.c Lines 13 - 42
13 int main() {
14 stdio_init_all();
15
16 PIO pio = pio0;
17 uint sm = 0;
18 uint offset = pio_add_program(pio, &i2c_program);
19 i2c_program_init(pio, sm, offset, PIN_SDA, PIN_SCL);
20
21 printf("\nPIO I2C Bus Scan\n");
22 printf(" 0 1 2 3 4 5 6 7 8 9 A B C D E F\n");
23
24 for (int addr = 0; addr < (1 << 7); ++addr) {
25 if (addr % 16 == 0) {
26 printf("%02x ", addr);
27 }
28 // Perform a 0-byte read from the probe address. The read function
29 // returns a negative result NAK'd any time other than the last data
30 // byte. Skip over reserved addresses.
31 int result;
32 if (reserved_addr(addr))
33 result = -1;
34 else
RP2350 Datasheet
11.6. Examples 935
35 result = pio_i2c_read_blocking(pio, sm, addr, NULL, 0);
36
37 printf(result < 0 ? "." : "@");
38 printf(addr % 16 == 15 ? "\n" : " ");
39 }
40 printf("Done.\n");
41 return 0;
42 }
11.6.8. PWM
Figure 62. Pulse width
modulation (PWM).
The state machine
outputs positive
voltage pulses at
regular intervals. The
width of these pulses
is controlled, so that
the line is high for
some controlled
fraction of the time
(the duty cycle). One
use of this is to
smoothly vary the
brightness of an LED,
by pulsing it faster
than human
persistence of vision.
This program repeatedly counts down to 0 with the Y register, whilst comparing the Y count to a pulse width held in the
X register. The output is asserted low before counting begins, and asserted high when the value in Y reaches X. Once Y
reaches 0, the process repeats, and the output is once more driven low. The fraction of time that the output is high is
therefore proportional to the pulse width stored in X.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/pwm/pwm.pio Lines 10 - 22
10 .program pwm
11 .side_set 1 opt
12
13 pull noblock side 0 ; Pull from FIFO to OSR if available, else copy X to OSR.
14 mov x, osr ; Copy most-recently-pulled value back to scratch X
15 mov y, isr ; ISR contains PWM period. Y used as counter.
16 countloop:
17 jmp x!=y noset ; Set pin high if X == Y, keep the two paths length matched
18 jmp skip side 1
19 noset:
20 nop ; Single dummy cycle to keep the two paths the same length
21 skip:
22 jmp y-- countloop ; Loop until Y hits 0, then pull a fresh PWM value from FIFO
Often, a PWM can be left at a particular pulse width for thousands of pulses, rather than supplying a new pulse width
each time. This example highlights how a non-blocking PULL (Section 11.4.7) can achieve this: if the TX FIFO is empty, a
non-blocking PULL will copy X to the OSR. After pulling, the program copies the OSR into X, so that it can be compared to
the count value in Y. The net effect is that, if a new duty cycle value has not been supplied through the TX FIFO at the
start of this period, the duty cycle from the previous period (which has been copied from X to OSR via the failed PULL, and
then back to X via the MOV) is reused, for as many periods as necessary.
Another useful technique shown here is using the ISR as a configuration register, if IN instructions are not required.
System software can load an arbitrary 32-bit value into the ISR (by executing instructions directly on the state machine),
and the program will copy this value into Y each time it begins counting. The ISR can be used to configure the range of
PWM counting, and the state machine’s clock divider controls the rate of counting.
To start modulating some pulses, we first need to map the state machine’s side-set pins to the GPIO we want to output
PWM on, and tell the state machine where the program is loaded in the PIO instruction memory:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/pwm/pwm.pio Lines 25 - 31
25 static inline void pwm_program_init(PIO pio, uint sm, uint offset, uint pin) {
26 pio_gpio_init(pio, pin);
27 pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
RP2350 Datasheet
11.6. Examples 936
28 pio_sm_config c = pwm_program_get_default_config(offset);
29 sm_config_set_sideset_pins(&c, pin);
30 pio_sm_init(pio, sm, offset, &c);
31 }
A little footwork is required to load the ISR with the desired counting range:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/pwm/pwm.c Lines 14 - 20
14 void pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
15 pio_sm_set_enabled(pio, sm, false);
16 pio_sm_put_blocking(pio, sm, period);
17 pio_sm_exec(pio, sm, pio_encode_pull(false, false));
18 pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
19 pio_sm_set_enabled(pio, sm, true);
20 }
Once this is done, the state machine can be enabled, and PWM values written directly to its TX FIFO.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/pwm/pwm.c Lines 23 - 25
23 void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
24 pio_sm_put_blocking(pio, sm, level);
25 }
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/pwm/pwm.c Lines 27 - 51
27 int main() {
28 stdio_init_all();
29 #ifndef PICO_DEFAULT_LED_PIN
30 #warning pio/pwm example requires a board with a regular LED
31 puts("Default LED pin was not defined");
32 #else
33
34 // todo get free sm
35 PIO pio = pio0;
36 int sm = 0;
37 uint offset = pio_add_program(pio, &pwm_program);
38 printf("Loaded program at %d\n", offset);
39
40 pwm_program_init(pio, sm, offset, PICO_DEFAULT_LED_PIN);
41 pio_pwm_set_period(pio, sm, (1u << 16) - 1);
42
43 int level = 0;
44 while (true) {
45 printf("Level = %d\n", level);
46 pio_pwm_set_level(pio, sm, level * level);
47 level = (level + 1) % 256;
48 sleep_ms(10);
49 }
50 #endif
51 }
If the TX FIFO is kept topped up with fresh pulse width values, this program will consume a new pulse width for each
pulse. Once the FIFO runs dry, the program will again start reusing the most recently supplied value.
RP2350 Datasheet
11.6. Examples 937
11.6.9. Addition
Although not designed for computation, PIO is quite likely Turing-complete, provided a long enough piece of tape can be
found. It is conjectured that it could run DOOM, given a sufficiently high clock speed.
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/addition/addition.pio Lines 7 - 25
 7 .program addition
 8
 9 ; Pop two 32 bit integers from the TX FIFO, add them together, and push the
10 ; result to the TX FIFO. Autopush/pull should be disabled as we're using
11 ; explicit push and pull instructions.
12 ;
13 ; This program uses the two's complement identity x + y == ~(~x - y)
14
15 pull
16 mov x, ~osr
17 pull
18 mov y, osr
19 jmp test ; this loop is equivalent to the following C code:
20 incr: ; while (y--)
21 jmp x-- test ; x--;
22 test: ; This has the effect of subtracting y from x, eventually.
23 jmp y-- incr
24 mov isr, ~x
25 push
A full 32-bit addition takes only around one minute at 125 MHz. The program pulls two numbers from the TX FIFO and
pushes their sum to the RX FIFO, which is perfect for use either with the system DMA, or directly by the processor:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/pio/addition/addition.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdlib.h>
 8 #include <stdio.h>
 9
10 #include "pico/stdlib.h"
11 #include "hardware/pio.h"
12 #include "addition.pio.h"
13
14 // Pop quiz: how many additions does the processor do when calling this function
15 uint32_t do_addition(PIO pio, uint sm, uint32_t a, uint32_t b) {
16 pio_sm_put_blocking(pio, sm, a);
17 pio_sm_put_blocking(pio, sm, b);
18 return pio_sm_get_blocking(pio, sm);
19 }
20
21 int main() {
22 stdio_init_all();
23
24 PIO pio = pio0;
25 uint sm = 0;
26 uint offset = pio_add_program(pio, &addition_program);
27 addition_program_init(pio, sm, offset);
28
29 printf("Doing some random additions:\n");
RP2350 Datasheet
11.6. Examples 938
30 for (int i = 0; i < 10; ++i) {
31 uint a = rand() % 100;
32 uint b = rand() % 100;
33 printf("%u + %u = %u\n", a, b, do_addition(pio, sm, a, b));
34 }
35 }
11.6.10. Further examples
Raspberry Pi Pico-series C/C++ SDK has a PIO chapter which goes into depth on some software-centric topics not
presented here. It includes a PIO + DMA logic analyser example that can sample every GPIO on every cycle (a bandwidth
of nearly 4Gbps at 125 MHz, although this does fill up RP2350’s RAM somewhat quickly).
There are also further examples in the pio/ directory in the Pico Examples repository.
Some of the more experimental example code, such as DPI and SD card support, is currently located in the Pico Extras
and Pico Playground repositories. The PIO parts of these are functional, but the surrounding software stacks are still in
an experimental state.
11.7. List of registers
The PIO0 and PIO1 registers start at base addresses of 0x50200000 and 0x50300000 respectively (defined as PIO0_BASE
and PIO1_BASE in SDK).
Table 981. List of PIO
registers
Offset Name Info
0x000 CTRL PIO control register
0x004 FSTAT FIFO status register
0x008 FDEBUG FIFO debug register
0x00c FLEVEL FIFO levels
0x010 TXF0 Direct write access to the TX FIFO for this state machine. Each
write pushes one word to the FIFO. Attempting to write to a full
FIFO has no effect on the FIFO state or contents, and sets the
sticky FDEBUG_TXOVER error flag for this FIFO.
0x014 TXF1 Direct write access to the TX FIFO for this state machine. Each
write pushes one word to the FIFO. Attempting to write to a full
FIFO has no effect on the FIFO state or contents, and sets the
sticky FDEBUG_TXOVER error flag for this FIFO.
0x018 TXF2 Direct write access to the TX FIFO for this state machine. Each
write pushes one word to the FIFO. Attempting to write to a full
FIFO has no effect on the FIFO state or contents, and sets the
sticky FDEBUG_TXOVER error flag for this FIFO.
0x01c TXF3 Direct write access to the TX FIFO for this state machine. Each
write pushes one word to the FIFO. Attempting to write to a full
FIFO has no effect on the FIFO state or contents, and sets the
sticky FDEBUG_TXOVER error flag for this FIFO.
RP2350 Datasheet
11.7. List of registers 939
Offset Name Info
0x020 RXF0 Direct read access to the RX FIFO for this state machine. Each
read pops one word from the FIFO. Attempting to read from an
empty FIFO has no effect on the FIFO state, and sets the sticky
FDEBUG_RXUNDER error flag for this FIFO. The data returned to
the system on a read from an empty FIFO is undefined.
0x024 RXF1 Direct read access to the RX FIFO for this state machine. Each
read pops one word from the FIFO. Attempting to read from an
empty FIFO has no effect on the FIFO state, and sets the sticky
FDEBUG_RXUNDER error flag for this FIFO. The data returned to
the system on a read from an empty FIFO is undefined.
0x028 RXF2 Direct read access to the RX FIFO for this state machine. Each
read pops one word from the FIFO. Attempting to read from an
empty FIFO has no effect on the FIFO state, and sets the sticky
FDEBUG_RXUNDER error flag for this FIFO. The data returned to
the system on a read from an empty FIFO is undefined.
0x02c RXF3 Direct read access to the RX FIFO for this state machine. Each
read pops one word from the FIFO. Attempting to read from an
empty FIFO has no effect on the FIFO state, and sets the sticky
FDEBUG_RXUNDER error flag for this FIFO. The data returned to
the system on a read from an empty FIFO is undefined.
0x030 IRQ State machine IRQ flags register. Write 1 to clear. There are eight
state machine IRQ flags, which can be set, cleared, and waited on
by the state machines. There’s no fixed association between
flags and state machines — any state machine can use any flag.
Any of the eight flags can be used for timing synchronisation
between state machines, using IRQ and WAIT instructions. Any
combination of the eight flags can also routed out to either of the
two system-level interrupt requests, alongside FIFO status
interrupts — see e.g. IRQ0_INTE.
0x034 IRQ_FORCE Writing a 1 to each of these bits will forcibly assert the
corresponding IRQ. Note this is different to the INTF register:
writing here affects PIO internal state. INTF just asserts the
processor-facing IRQ signal for testing ISRs, and is not visible to
the state machines.
0x038 INPUT_SYNC_BYPASS There is a 2-flipflop synchronizer on each GPIO input, which
protects PIO logic from metastabilities. This increases input
delay, and for fast synchronous IO (e.g. SPI) these synchronizers
may need to be bypassed. Each bit in this register corresponds
to one GPIO.
0 → input is synchronized (default)
1 → synchronizer is bypassed
If in doubt, leave this register as all zeroes.
0x03c DBG_PADOUT Read to sample the pad output values PIO is currently driving to
the GPIOs. On RP2040 there are 30 GPIOs, so the two most
significant bits are hardwired to 0.
0x040 DBG_PADOE Read to sample the pad output enables (direction) PIO is
currently driving to the GPIOs. On RP2040 there are 30 GPIOs, so
the two most significant bits are hardwired to 0.
RP2350 Datasheet
11.7. List of registers 940
Offset Name Info
0x044 DBG_CFGINFO The PIO hardware has some free parameters that may vary
between chip products.
These should be provided in the chip datasheet, but are also
exposed here.
0x048 INSTR_MEM0 Write-only access to instruction memory location 0
0x04c INSTR_MEM1 Write-only access to instruction memory location 1
0x050 INSTR_MEM2 Write-only access to instruction memory location 2
0x054 INSTR_MEM3 Write-only access to instruction memory location 3
0x058 INSTR_MEM4 Write-only access to instruction memory location 4
0x05c INSTR_MEM5 Write-only access to instruction memory location 5
0x060 INSTR_MEM6 Write-only access to instruction memory location 6
0x064 INSTR_MEM7 Write-only access to instruction memory location 7
0x068 INSTR_MEM8 Write-only access to instruction memory location 8
0x06c INSTR_MEM9 Write-only access to instruction memory location 9
0x070 INSTR_MEM10 Write-only access to instruction memory location 10
0x074 INSTR_MEM11 Write-only access to instruction memory location 11
0x078 INSTR_MEM12 Write-only access to instruction memory location 12
0x07c INSTR_MEM13 Write-only access to instruction memory location 13
0x080 INSTR_MEM14 Write-only access to instruction memory location 14
0x084 INSTR_MEM15 Write-only access to instruction memory location 15
0x088 INSTR_MEM16 Write-only access to instruction memory location 16
0x08c INSTR_MEM17 Write-only access to instruction memory location 17
0x090 INSTR_MEM18 Write-only access to instruction memory location 18
0x094 INSTR_MEM19 Write-only access to instruction memory location 19
0x098 INSTR_MEM20 Write-only access to instruction memory location 20
0x09c INSTR_MEM21 Write-only access to instruction memory location 21
0x0a0 INSTR_MEM22 Write-only access to instruction memory location 22
0x0a4 INSTR_MEM23 Write-only access to instruction memory location 23
0x0a8 INSTR_MEM24 Write-only access to instruction memory location 24
0x0ac INSTR_MEM25 Write-only access to instruction memory location 25
0x0b0 INSTR_MEM26 Write-only access to instruction memory location 26
0x0b4 INSTR_MEM27 Write-only access to instruction memory location 27
0x0b8 INSTR_MEM28 Write-only access to instruction memory location 28
0x0bc INSTR_MEM29 Write-only access to instruction memory location 29
0x0c0 INSTR_MEM30 Write-only access to instruction memory location 30
0x0c4 INSTR_MEM31 Write-only access to instruction memory location 31
RP2350 Datasheet
11.7. List of registers 941
Offset Name Info
0x0c8 SM0_CLKDIV Clock divisor register for state machine 0
Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
0x0cc SM0_EXECCTRL Execution/behavioural settings for state machine 0
0x0d0 SM0_SHIFTCTRL Control behaviour of the input/output shift registers for state
machine 0
0x0d4 SM0_ADDR Current instruction address of state machine 0
0x0d8 SM0_INSTR Read to see the instruction currently addressed by state machine
0’s program counter
Write to execute an instruction immediately (including jumps)
and then resume execution.
0x0dc SM0_PINCTRL State machine pin control
0x0e0 SM1_CLKDIV Clock divisor register for state machine 1
Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
0x0e4 SM1_EXECCTRL Execution/behavioural settings for state machine 1
0x0e8 SM1_SHIFTCTRL Control behaviour of the input/output shift registers for state
machine 1
0x0ec SM1_ADDR Current instruction address of state machine 1
0x0f0 SM1_INSTR Read to see the instruction currently addressed by state machine
1’s program counter
Write to execute an instruction immediately (including jumps)
and then resume execution.
0x0f4 SM1_PINCTRL State machine pin control
0x0f8 SM2_CLKDIV Clock divisor register for state machine 2
Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
0x0fc SM2_EXECCTRL Execution/behavioural settings for state machine 2
0x100 SM2_SHIFTCTRL Control behaviour of the input/output shift registers for state
machine 2
0x104 SM2_ADDR Current instruction address of state machine 2
0x108 SM2_INSTR Read to see the instruction currently addressed by state machine
2’s program counter
Write to execute an instruction immediately (including jumps)
and then resume execution.
0x10c SM2_PINCTRL State machine pin control
0x110 SM3_CLKDIV Clock divisor register for state machine 3
Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
0x114 SM3_EXECCTRL Execution/behavioural settings for state machine 3
0x118 SM3_SHIFTCTRL Control behaviour of the input/output shift registers for state
machine 3
0x11c SM3_ADDR Current instruction address of state machine 3
0x120 SM3_INSTR Read to see the instruction currently addressed by state machine
3’s program counter
Write to execute an instruction immediately (including jumps)
and then resume execution.
RP2350 Datasheet
11.7. List of registers 942
Offset Name Info
0x124 SM3_PINCTRL State machine pin control
0x128 RXF0_PUTGET0 Direct read/write access to entry 0 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x12c RXF0_PUTGET1 Direct read/write access to entry 1 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x130 RXF0_PUTGET2 Direct read/write access to entry 2 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x134 RXF0_PUTGET3 Direct read/write access to entry 3 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x138 RXF1_PUTGET0 Direct read/write access to entry 0 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x13c RXF1_PUTGET1 Direct read/write access to entry 1 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x140 RXF1_PUTGET2 Direct read/write access to entry 2 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x144 RXF1_PUTGET3 Direct read/write access to entry 3 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x148 RXF2_PUTGET0 Direct read/write access to entry 0 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x14c RXF2_PUTGET1 Direct read/write access to entry 1 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x150 RXF2_PUTGET2 Direct read/write access to entry 2 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x154 RXF2_PUTGET3 Direct read/write access to entry 3 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x158 RXF3_PUTGET0 Direct read/write access to entry 0 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x15c RXF3_PUTGET1 Direct read/write access to entry 1 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x160 RXF3_PUTGET2 Direct read/write access to entry 2 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
RP2350 Datasheet
11.7. List of registers 943
Offset Name Info
0x164 RXF3_PUTGET3 Direct read/write access to entry 3 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is
set.
0x168 GPIOBASE Relocate GPIO 0 (from PIO’s point of view) in the system GPIO
numbering, to access more than 32 GPIOs from PIO.
Only the values 0 and 16 are supported (only bit 4 is writable).
0x16c INTR Raw Interrupts
0x170 IRQ0_INTE Interrupt Enable for irq0
0x174 IRQ0_INTF Interrupt Force for irq0
0x178 IRQ0_INTS Interrupt status after masking & forcing for irq0
0x17c IRQ1_INTE Interrupt Enable for irq1
0x180 IRQ1_INTF Interrupt Force for irq1
0x184 IRQ1_INTS Interrupt status after masking & forcing for irq1
PIO: CTRL Register
Offset: 0x000
Description
PIO control register
Table 982. CTRL
Register
Bits Description Type Reset
31:27 Reserved. - -
26 NEXTPREV_CLKDIV_RESTART: Write 1 to restart the clock dividers of state
machines in neighbouring PIO blocks, as specified by NEXT_PIO_MASK and
PREV_PIO_MASK in the same write.
This is equivalent to writing 1 to the corresponding CLKDIV_RESTART bits in
those PIOs' CTRL registers.
SC 0x0
25 NEXTPREV_SM_DISABLE: Write 1 to disable state machines in neighbouring
PIO blocks, as specified by NEXT_PIO_MASK and PREV_PIO_MASK in the
same write.
This is equivalent to clearing the corresponding SM_ENABLE bits in those
PIOs' CTRL registers.
SC 0x0
24 NEXTPREV_SM_ENABLE: Write 1 to enable state machines in neighbouring
PIO blocks, as specified by NEXT_PIO_MASK and PREV_PIO_MASK in the
same write.
This is equivalent to setting the corresponding SM_ENABLE bits in those PIOs'
CTRL registers.
If both OTHERS_SM_ENABLE and OTHERS_SM_DISABLE are set, the disable
takes precedence.
SC 0x0
RP2350 Datasheet
11.7. List of registers 944
Bits Description Type Reset
23:20 NEXT_PIO_MASK: A mask of state machines in the neighbouring highernumbered PIO block in the system (or PIO block 0 if this is the highestnumbered PIO block) to which to apply the operations specified by
NEXTPREV_CLKDIV_RESTART, NEXTPREV_SM_ENABLE, and
NEXTPREV_SM_DISABLE in the same write.
This allows state machines in a neighbouring PIO block to be
started/stopped/clock-synced exactly simultaneously with a write to this PIO
block’s CTRL register.
Note that in a system with two PIOs, NEXT_PIO_MASK and PREV_PIO_MASK
actually indicate the same PIO block. In this case the effects are applied
cumulatively (as though the masks were OR’d together).
Neighbouring PIO blocks are disconnected (status signals tied to 0 and
control signals ignored) if one block is accessible to NonSecure code, and one
is not.
SC 0x0
19:16 PREV_PIO_MASK: A mask of state machines in the neighbouring lowernumbered PIO block in the system (or the highest-numbered PIO block if this
is PIO block 0) to which to apply the operations specified by
OP_CLKDIV_RESTART, OP_ENABLE, OP_DISABLE in the same write.
This allows state machines in a neighbouring PIO block to be
started/stopped/clock-synced exactly simultaneously with a write to this PIO
block’s CTRL register.
Neighbouring PIO blocks are disconnected (status signals tied to 0 and
control signals ignored) if one block is accessible to NonSecure code, and one
is not.
SC 0x0
15:12 Reserved. - -
11:8 CLKDIV_RESTART: Restart a state machine’s clock divider from an initial
phase of 0. Clock dividers are free-running, so once started, their output
(including fractional jitter) is completely determined by the integer/fractional
divisor configured in SMx_CLKDIV. This means that, if multiple clock dividers
with the same divisor are restarted simultaneously, by writing multiple 1 bits to
this field, the execution clocks of those state machines will run in precise
lockstep.
Note that setting/clearing SM_ENABLE does not stop the clock divider from
running, so once multiple state machines' clocks are synchronised, it is safe to
disable/reenable a state machine, whilst keeping the clock dividers in sync.
Note also that CLKDIV_RESTART can be written to whilst the state machine is
running, and this is useful to resynchronise clock dividers after the divisors
(SMx_CLKDIV) have been changed on-the-fly.
SC 0x0
RP2350 Datasheet
11.7. List of registers 945
Bits Description Type Reset
7:4 SM_RESTART: Write 1 to instantly clear internal SM state which may be
otherwise difficult to access and will affect future execution.
Specifically, the following are cleared: input and output shift counters; the
contents of the input shift register; the delay counter; the waiting-on-IRQ state;
any stalled instruction written to SMx_INSTR or run by OUT/MOV EXEC; any
pin write left asserted due to OUT_STICKY.
The contents of the output shift register and the X/Y scratch registers are not
affected.
SC 0x0
3:0 SM_ENABLE: Enable/disable each of the four state machines by writing 1/0 to
each of these four bits. When disabled, a state machine will cease executing
instructions, except those written directly to SMx_INSTR by the system.
Multiple bits can be set/cleared at once to run/halt multiple state machines
simultaneously.
RW 0x0
PIO: FSTAT Register
Offset: 0x004
Description
FIFO status register
Table 983. FSTAT
Register
Bits Description Type Reset
31:28 Reserved. - -
27:24 TXEMPTY: State machine TX FIFO is empty RO 0xf
23:20 Reserved. - -
19:16 TXFULL: State machine TX FIFO is full RO 0x0
15:12 Reserved. - -
11:8 RXEMPTY: State machine RX FIFO is empty RO 0xf
7:4 Reserved. - -
3:0 RXFULL: State machine RX FIFO is full RO 0x0
PIO: FDEBUG Register
Offset: 0x008
Description
FIFO debug register
Table 984. FDEBUG
Register
Bits Description Type Reset
31:28 Reserved. - -
27:24 TXSTALL: State machine has stalled on empty TX FIFO during a blocking
PULL, or an OUT with autopull enabled. Write 1 to clear.
WC 0x0
23:20 Reserved. - -
RP2350 Datasheet
11.7. List of registers 946
Bits Description Type Reset
19:16 TXOVER: TX FIFO overflow (i.e. write-on-full by the system) has occurred.
Write 1 to clear. Note that write-on-full does not alter the state or contents of
the FIFO in any way, but the data that the system attempted to write is
dropped, so if this flag is set, your software has quite likely dropped some data
on the floor.
WC 0x0
15:12 Reserved. - -
11:8 RXUNDER: RX FIFO underflow (i.e. read-on-empty by the system) has
occurred. Write 1 to clear. Note that read-on-empty does not perturb the state
of the FIFO in any way, but the data returned by reading from an empty FIFO is
undefined, so this flag generally only becomes set due to some kind of
software error.
WC 0x0
7:4 Reserved. - -
3:0 RXSTALL: State machine has stalled on full RX FIFO during a blocking PUSH,
or an IN with autopush enabled. This flag is also set when a nonblocking
PUSH to a full FIFO took place, in which case the state machine has dropped
data. Write 1 to clear.
WC 0x0
PIO: FLEVEL Register
Offset: 0x00c
Description
FIFO levels
Table 985. FLEVEL
Register
Bits Description Type Reset
31:28 RX3 RO 0x0
27:24 TX3 RO 0x0
23:20 RX2 RO 0x0
19:16 TX2 RO 0x0
15:12 RX1 RO 0x0
11:8 TX1 RO 0x0
7:4 RX0 RO 0x0
3:0 TX0 RO 0x0
PIO: TXF0, TXF1, TXF2, TXF3 Registers
Offsets: 0x010, 0x014, 0x018, 0x01c
Table 986. TXF0,
TXF1, TXF2, TXF3
Registers
Bits Description Type Reset
31:0 Direct write access to the TX FIFO for this state machine. Each write pushes
one word to the FIFO. Attempting to write to a full FIFO has no effect on the
FIFO state or contents, and sets the sticky FDEBUG_TXOVER error flag for this
FIFO.
WF 0x00000000
PIO: RXF0, RXF1, RXF2, RXF3 Registers
Offsets: 0x020, 0x024, 0x028, 0x02c
RP2350 Datasheet
11.7. List of registers 947
Table 987. RXF0,
RXF1, RXF2, RXF3
Registers
Bits Description Type Reset
31:0 Direct read access to the RX FIFO for this state machine. Each read pops one
word from the FIFO. Attempting to read from an empty FIFO has no effect on
the FIFO state, and sets the sticky FDEBUG_RXUNDER error flag for this FIFO.
The data returned to the system on a read from an empty FIFO is undefined.
RF -
PIO: IRQ Register
Offset: 0x030
Table 988. IRQ
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 State machine IRQ flags register. Write 1 to clear. There are eight state
machine IRQ flags, which can be set, cleared, and waited on by the state
machines. There’s no fixed association between flags and state
machines — any state machine can use any flag.
Any of the eight flags can be used for timing synchronisation between state
machines, using IRQ and WAIT instructions. Any combination of the eight
flags can also routed out to either of the two system-level interrupt requests,
alongside FIFO status interrupts — see e.g. IRQ0_INTE.
WC 0x00
PIO: IRQ_FORCE Register
Offset: 0x034
Table 989. IRQ_FORCE
Register
Bits Description Type Reset
31:8 Reserved. - -
7:0 Writing a 1 to each of these bits will forcibly assert the corresponding IRQ.
Note this is different to the INTF register: writing here affects PIO internal
state. INTF just asserts the processor-facing IRQ signal for testing ISRs, and is
not visible to the state machines.
WF 0x00
PIO: INPUT_SYNC_BYPASS Register
Offset: 0x038
Table 990.
INPUT_SYNC_BYPASS
Register
Bits Description Type Reset
31:0 There is a 2-flipflop synchronizer on each GPIO input, which protects PIO logic
from metastabilities. This increases input delay, and for fast synchronous IO
(e.g. SPI) these synchronizers may need to be bypassed. Each bit in this
register corresponds to one GPIO.
0 → input is synchronized (default)
1 → synchronizer is bypassed
If in doubt, leave this register as all zeroes.
RW 0x00000000
PIO: DBG_PADOUT Register
Offset: 0x03c
RP2350 Datasheet
11.7. List of registers 948
Table 991.
DBG_PADOUT Register
Bits Description Type Reset
31:0 Read to sample the pad output values PIO is currently driving to the GPIOs. On
RP2040 there are 30 GPIOs, so the two most significant bits are hardwired to
0.
RO 0x00000000
PIO: DBG_PADOE Register
Offset: 0x040
Table 992.
DBG_PADOE Register
Bits Description Type Reset
31:0 Read to sample the pad output enables (direction) PIO is currently driving to
the GPIOs. On RP2040 there are 30 GPIOs, so the two most significant bits are
hardwired to 0.
RO 0x00000000
PIO: DBG_CFGINFO Register
Offset: 0x044
Description
The PIO hardware has some free parameters that may vary between chip products.
These should be provided in the chip datasheet, but are also exposed here.
Table 993.
DBG_CFGINFO
Register
Bits Description Type Reset
31:28 VERSION: Version of the core PIO hardware. RO 0x1
Enumerated values:
0x0 → V0: Version 0 (RP2040)
0x1 → V1: Version 1 (RP2350)
27:22 Reserved. - -
21:16 IMEM_SIZE: The size of the instruction memory, measured in units of one
instruction
RO -
15:12 Reserved. - -
11:8 SM_COUNT: The number of state machines this PIO instance is equipped
with.
RO -
7:6 Reserved. - -
5:0 FIFO_DEPTH: The depth of the state machine TX/RX FIFOs, measured in
words.
Joining fifos via SHIFTCTRL_FJOIN gives one FIFO with double
this depth.
RO -
PIO: INSTR_MEM0, INSTR_MEM1, …, INSTR_MEM30, INSTR_MEM31 Registers
Offsets: 0x048, 0x04c, …, 0x0c0, 0x0c4
RP2350 Datasheet
11.7. List of registers 949
Table 994.
INSTR_MEM0,
INSTR_MEM1, …,
INSTR_MEM30,
INSTR_MEM31
Registers
Bits Description Type Reset
31:16 Reserved. - -
15:0 Write-only access to instruction memory location N WO 0x0000
PIO: SM0_CLKDIV, SM1_CLKDIV, SM2_CLKDIV, SM3_CLKDIV Registers
Offsets: 0x0c8, 0x0e0, 0x0f8, 0x110
Description
Clock divisor register for state machine N
Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
Table 995.
SM0_CLKDIV,
SM1_CLKDIV,
SM2_CLKDIV,
SM3_CLKDIV
Registers
Bits Description Type Reset
31:16 INT: Effective frequency is sysclk/(int + frac/256).
Value of 0 is interpreted as 65536. If INT is 0, FRAC must also be 0.
RW 0x0001
15:8 FRAC: Fractional part of clock divisor RW 0x00
7:0 Reserved. - -
PIO: SM0_EXECCTRL, SM1_EXECCTRL, SM2_EXECCTRL, SM3_EXECCTRL
Registers
Offsets: 0x0cc, 0x0e4, 0x0fc, 0x114
Description
Execution/behavioural settings for state machine N
Table 996.
SM0_EXECCTRL,
SM1_EXECCTRL,
SM2_EXECCTRL,
SM3_EXECCTRL
Registers
Bits Description Type Reset
31 EXEC_STALLED: If 1, an instruction written to SMx_INSTR is stalled, and
latched by the state machine. Will clear to 0 once this instruction completes.
RO 0x0
30 SIDE_EN: If 1, the MSB of the Delay/Side-set instruction field is used as sideset enable, rather than a side-set data bit. This allows instructions to perform
side-set optionally, rather than on every instruction, but the maximum possible
side-set width is reduced from 5 to 4. Note that the value of
PINCTRL_SIDESET_COUNT is inclusive of this enable bit.
RW 0x0
29 SIDE_PINDIR: If 1, side-set data is asserted to pin directions, instead of pin
values
RW 0x0
28:24 JMP_PIN: The GPIO number to use as condition for JMP PIN. Unaffected by
input mapping.
RW 0x00
23:19 OUT_EN_SEL: Which data bit to use for inline OUT enable RW 0x00
18 INLINE_OUT_EN: If 1, use a bit of OUT data as an auxiliary write enable
When used in conjunction with OUT_STICKY, writes with an enable of 0 will
deassert the latest pin write. This can create useful masking/override
behaviour
due to the priority ordering of state machine pin writes (SM0 < SM1 < …)
RW 0x0
17 OUT_STICKY: Continuously assert the most recent OUT/SET to the pins RW 0x0
16:12 WRAP_TOP: After reaching this address, execution is wrapped to
wrap_bottom.
If the instruction is a jump, and the jump condition is true, the jump takes
priority.
RW 0x1f
RP2350 Datasheet
11.7. List of registers 950
Bits Description Type Reset
11:7 WRAP_BOTTOM: After reaching wrap_top, execution is wrapped to this
address.
RW 0x00
6:5 STATUS_SEL: Comparison used for the MOV x, STATUS instruction. RW 0x0
Enumerated values:
0x0 → TXLEVEL: All-ones if TX FIFO level < N, otherwise all-zeroes
0x1 → RXLEVEL: All-ones if RX FIFO level < N, otherwise all-zeroes
0x2 → IRQ: All-ones if the indexed IRQ flag is raised, otherwise all-zeroes
4:0 STATUS_N: Comparison level or IRQ index for the MOV x, STATUS instruction.
If STATUS_SEL is TXLEVEL or RXLEVEL, then values of STATUS_N greater
than the current FIFO depth are reserved, and have undefined behaviour.
RW 0x00
Enumerated values:
0x00 → IRQ: Index 0-7 of an IRQ flag in this PIO block
0x08 → IRQ_PREVPIO: Index 0-7 of an IRQ flag in the next lower-numbered PIO
block
0x10 → IRQ_NEXTPIO: Index 0-7 of an IRQ flag in the next higher-numbered
PIO block
PIO: SM0_SHIFTCTRL, SM1_SHIFTCTRL, SM2_SHIFTCTRL, SM3_SHIFTCTRL
Registers
Offsets: 0x0d0, 0x0e8, 0x100, 0x118
Description
Control behaviour of the input/output shift registers for state machine N
Table 997.
SM0_SHIFTCTRL,
SM1_SHIFTCTRL,
SM2_SHIFTCTRL,
SM3_SHIFTCTRL
Registers
Bits Description Type Reset
31 FJOIN_RX: When 1, RX FIFO steals the TX FIFO’s storage, and becomes twice
as deep.
TX FIFO is disabled as a result (always reads as both full and empty).
FIFOs are flushed when this bit is changed.
RW 0x0
30 FJOIN_TX: When 1, TX FIFO steals the RX FIFO’s storage, and becomes twice
as deep.
RX FIFO is disabled as a result (always reads as both full and empty).
FIFOs are flushed when this bit is changed.
RW 0x0
29:25 PULL_THRESH: Number of bits shifted out of OSR before autopull, or
conditional pull (PULL IFEMPTY), will take place.
Write 0 for value of 32.
RW 0x00
24:20 PUSH_THRESH: Number of bits shifted into ISR before autopush, or
conditional push (PUSH IFFULL), will take place.
Write 0 for value of 32.
RW 0x00
19 OUT_SHIFTDIR: 1 = shift out of output shift register to right. 0 = to left. RW 0x1
18 IN_SHIFTDIR: 1 = shift input shift register to right (data enters from left). 0 = to
left.
RW 0x1
RP2350 Datasheet
11.7. List of registers 951
Bits Description Type Reset
17 AUTOPULL: Pull automatically when the output shift register is emptied, i.e. on
or following an OUT instruction which causes the output shift counter to reach
or exceed PULL_THRESH.
RW 0x0
16 AUTOPUSH: Push automatically when the input shift register is filled, i.e. on an
IN instruction which causes the input shift counter to reach or exceed
PUSH_THRESH.
RW 0x0
15 FJOIN_RX_PUT: If 1, disable this state machine’s RX FIFO, make its storage
available for random write access by the state machine (using the put
instruction) and, unless FJOIN_RX_GET is also set, random read access by the
processor (through the RXFx_PUTGETy registers).
If FJOIN_RX_PUT and FJOIN_RX_GET are both set, then the RX FIFO’s
registers can be randomly read/written by the state machine, but are
completely inaccessible to the processor.
Setting this bit will clear the FJOIN_TX and FJOIN_RX bits.
RW 0x0
14 FJOIN_RX_GET: If 1, disable this state machine’s RX FIFO, make its storage
available for random read access by the state machine (using the get
instruction) and, unless FJOIN_RX_PUT is also set, random write access by
the processor (through the RXFx_PUTGETy registers).
If FJOIN_RX_PUT and FJOIN_RX_GET are both set, then the RX FIFO’s
registers can be randomly read/written by the state machine, but are
completely inaccessible to the processor.
Setting this bit will clear the FJOIN_TX and FJOIN_RX bits.
RW 0x0
13:5 Reserved. - -
4:0 IN_COUNT: Set the number of pins which are not masked to 0 when read by an
IN PINS, WAIT PIN or MOV x, PINS instruction.
For example, an IN_COUNT of 5 means that the 5 LSBs of the IN pin group are
visible (bits 4:0), but the remaining 27 MSBs are masked to 0. A count of 32 is
encoded with a field value of 0, so the default behaviour is to not perform any
masking.
Note this masking is applied in addition to the masking usually performed by
the IN instruction. This is mainly useful for the MOV x, PINS instruction, which
otherwise has no way of masking pins.
RW 0x00
PIO: SM0_ADDR, SM1_ADDR, SM2_ADDR, SM3_ADDR Registers
Offsets: 0x0d4, 0x0ec, 0x104, 0x11c
Table 998. SM0_ADDR,
SM1_ADDR,
SM2_ADDR,
SM3_ADDR Registers
Bits Description Type Reset
31:5 Reserved. - -
4:0 Current instruction address of state machine N RO 0x00
PIO: SM0_INSTR, SM1_INSTR, SM2_INSTR, SM3_INSTR Registers
Offsets: 0x0d8, 0x0f0, 0x108, 0x120
RP2350 Datasheet
11.7. List of registers 952
Table 999.
SM0_INSTR,
SM1_INSTR,
SM2_INSTR,
SM3_INSTR Registers
Bits Description Type Reset
31:16 Reserved. - -
15:0 Read to see the instruction currently addressed by state machine N's program
counter.
Write to execute an instruction immediately (including jumps) and then
resume execution.
RW -
PIO: SM0_PINCTRL, SM1_PINCTRL, SM2_PINCTRL, SM3_PINCTRL Registers
Offsets: 0x0dc, 0x0f4, 0x10c, 0x124
Description
State machine pin control
Table 1000.
SM0_PINCTRL,
SM1_PINCTRL,
SM2_PINCTRL,
SM3_PINCTRL
Registers
Bits Description Type Reset
31:29 SIDESET_COUNT: The number of MSBs of the Delay/Side-set instruction field
which are used for side-set. Inclusive of the enable bit, if present. Minimum of
0 (all delay bits, no side-set) and maximum of 5 (all side-set, no delay).
RW 0x0
28:26 SET_COUNT: The number of pins asserted by a SET. In the range 0 to 5
inclusive.
RW 0x5
25:20 OUT_COUNT: The number of pins asserted by an OUT PINS, OUT PINDIRS or
MOV PINS instruction. In the range 0 to 32 inclusive.
RW 0x00
19:15 IN_BASE: The pin which is mapped to the least-significant bit of a state
machine’s IN data bus. Higher-numbered pins are mapped to consecutively
more-significant data bits, with a modulo of 32 applied to pin number.
RW 0x00
14:10 SIDESET_BASE: The lowest-numbered pin that will be affected by a side-set
operation. The MSBs of an instruction’s side-set/delay field (up to 5,
determined by SIDESET_COUNT) are used for side-set data, with the remaining
LSBs used for delay. The least-significant bit of the side-set portion is the bit
written to this pin, with more-significant bits written to higher-numbered pins.
RW 0x00
9:5 SET_BASE: The lowest-numbered pin that will be affected by a SET PINS or
SET PINDIRS instruction. The data written to this pin is the least-significant bit
of the SET data.
RW 0x00
4:0 OUT_BASE: The lowest-numbered pin that will be affected by an OUT PINS,
OUT PINDIRS or MOV PINS instruction. The data written to this pin will always
be the least-significant bit of the OUT or MOV data.
RW 0x00
PIO: RXF0_PUTGET0 Register
Offset: 0x128
Table 1001.
RXF0_PUTGET0
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 0 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF0_PUTGET1 Register
Offset: 0x12c
RP2350 Datasheet
11.7. List of registers 953
Table 1002.
RXF0_PUTGET1
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 1 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF0_PUTGET2 Register
Offset: 0x130
Table 1003.
RXF0_PUTGET2
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 2 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF0_PUTGET3 Register
Offset: 0x134
Table 1004.
RXF0_PUTGET3
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 3 of SM0’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF1_PUTGET0 Register
Offset: 0x138
Table 1005.
RXF1_PUTGET0
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 0 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF1_PUTGET1 Register
Offset: 0x13c
Table 1006.
RXF1_PUTGET1
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 1 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF1_PUTGET2 Register
Offset: 0x140
Table 1007.
RXF1_PUTGET2
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 2 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF1_PUTGET3 Register
Offset: 0x144
RP2350 Datasheet
11.7. List of registers 954
Table 1008.
RXF1_PUTGET3
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 3 of SM1’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF2_PUTGET0 Register
Offset: 0x148
Table 1009.
RXF2_PUTGET0
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 0 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF2_PUTGET1 Register
Offset: 0x14c
Table 1010.
RXF2_PUTGET1
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 1 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF2_PUTGET2 Register
Offset: 0x150
Table 1011.
RXF2_PUTGET2
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 2 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF2_PUTGET3 Register
Offset: 0x154
Table 1012.
RXF2_PUTGET3
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 3 of SM2’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF3_PUTGET0 Register
Offset: 0x158
Table 1013.
RXF3_PUTGET0
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 0 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF3_PUTGET1 Register
Offset: 0x15c
RP2350 Datasheet
11.7. List of registers 955
Table 1014.
RXF3_PUTGET1
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 1 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF3_PUTGET2 Register
Offset: 0x160
Table 1015.
RXF3_PUTGET2
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 2 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: RXF3_PUTGET3 Register
Offset: 0x164
Table 1016.
RXF3_PUTGET3
Register
Bits Description Type Reset
31:0 Direct read/write access to entry 3 of SM3’s RX FIFO, if
SHIFTCTRL_FJOIN_RX_PUT xor SHIFTCTRL_FJOIN_RX_GET is set.
RW 0x00000000
PIO: GPIOBASE Register
Offset: 0x168
Table 1017.
GPIOBASE Register
Bits Description Type Reset
31:5 Reserved. - -
4 Relocate GPIO 0 (from PIO’s point of view) in the system GPIO numbering, to
access more than 32 GPIOs from PIO.
Only the values 0 and 16 are supported (only bit 4 is writable).
RW 0x0
3:0 Reserved. - -
PIO: INTR Register
Offset: 0x16c
Description
Raw Interrupts
Table 1018. INTR
Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RO 0x0
14 SM6 RO 0x0
13 SM5 RO 0x0
12 SM4 RO 0x0
11 SM3 RO 0x0
10 SM2 RO 0x0
9 SM1 RO 0x0
8 SM0 RO 0x0
RP2350 Datasheet
11.7. List of registers 956
Bits Description Type Reset
7 SM3_TXNFULL RO 0x0
6 SM2_TXNFULL RO 0x0
5 SM1_TXNFULL RO 0x0
4 SM0_TXNFULL RO 0x0
3 SM3_RXNEMPTY RO 0x0
2 SM2_RXNEMPTY RO 0x0
1 SM1_RXNEMPTY RO 0x0
0 SM0_RXNEMPTY RO 0x0
PIO: IRQ0_INTE Register
Offset: 0x170
Description
Interrupt Enable for irq0
Table 1019.
IRQ0_INTE Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RW 0x0
14 SM6 RW 0x0
13 SM5 RW 0x0
12 SM4 RW 0x0
11 SM3 RW 0x0
10 SM2 RW 0x0
9 SM1 RW 0x0
8 SM0 RW 0x0
7 SM3_TXNFULL RW 0x0
6 SM2_TXNFULL RW 0x0
5 SM1_TXNFULL RW 0x0
4 SM0_TXNFULL RW 0x0
3 SM3_RXNEMPTY RW 0x0
2 SM2_RXNEMPTY RW 0x0
1 SM1_RXNEMPTY RW 0x0
0 SM0_RXNEMPTY RW 0x0
PIO: IRQ0_INTF Register
Offset: 0x174
Description
Interrupt Force for irq0
RP2350 Datasheet
11.7. List of registers 957
Table 1020.
IRQ0_INTF Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RW 0x0
14 SM6 RW 0x0
13 SM5 RW 0x0
12 SM4 RW 0x0
11 SM3 RW 0x0
10 SM2 RW 0x0
9 SM1 RW 0x0
8 SM0 RW 0x0
7 SM3_TXNFULL RW 0x0
6 SM2_TXNFULL RW 0x0
5 SM1_TXNFULL RW 0x0
4 SM0_TXNFULL RW 0x0
3 SM3_RXNEMPTY RW 0x0
2 SM2_RXNEMPTY RW 0x0
1 SM1_RXNEMPTY RW 0x0
0 SM0_RXNEMPTY RW 0x0
PIO: IRQ0_INTS Register
Offset: 0x178
Description
Interrupt status after masking & forcing for irq0
Table 1021.
IRQ0_INTS Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RO 0x0
14 SM6 RO 0x0
13 SM5 RO 0x0
12 SM4 RO 0x0
11 SM3 RO 0x0
10 SM2 RO 0x0
9 SM1 RO 0x0
8 SM0 RO 0x0
7 SM3_TXNFULL RO 0x0
6 SM2_TXNFULL RO 0x0
5 SM1_TXNFULL RO 0x0
4 SM0_TXNFULL RO 0x0
RP2350 Datasheet
11.7. List of registers 958
Bits Description Type Reset
3 SM3_RXNEMPTY RO 0x0
2 SM2_RXNEMPTY RO 0x0
1 SM1_RXNEMPTY RO 0x0
0 SM0_RXNEMPTY RO 0x0
PIO: IRQ1_INTE Register
Offset: 0x17c
Description
Interrupt Enable for irq1
Table 1022.
IRQ1_INTE Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RW 0x0
14 SM6 RW 0x0
13 SM5 RW 0x0
12 SM4 RW 0x0
11 SM3 RW 0x0
10 SM2 RW 0x0
9 SM1 RW 0x0
8 SM0 RW 0x0
7 SM3_TXNFULL RW 0x0
6 SM2_TXNFULL RW 0x0
5 SM1_TXNFULL RW 0x0
4 SM0_TXNFULL RW 0x0
3 SM3_RXNEMPTY RW 0x0
2 SM2_RXNEMPTY RW 0x0
1 SM1_RXNEMPTY RW 0x0
0 SM0_RXNEMPTY RW 0x0
PIO: IRQ1_INTF Register
Offset: 0x180
Description
Interrupt Force for irq1
Table 1023.
IRQ1_INTF Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RW 0x0
14 SM6 RW 0x0
13 SM5 RW 0x0
RP2350 Datasheet
11.7. List of registers 959
Bits Description Type Reset
12 SM4 RW 0x0
11 SM3 RW 0x0
10 SM2 RW 0x0
9 SM1 RW 0x0
8 SM0 RW 0x0
7 SM3_TXNFULL RW 0x0
6 SM2_TXNFULL RW 0x0
5 SM1_TXNFULL RW 0x0
4 SM0_TXNFULL RW 0x0
3 SM3_RXNEMPTY RW 0x0
2 SM2_RXNEMPTY RW 0x0
1 SM1_RXNEMPTY RW 0x0
0 SM0_RXNEMPTY RW 0x0
PIO: IRQ1_INTS Register
Offset: 0x184
Description
Interrupt status after masking & forcing for irq1
Table 1024.
IRQ1_INTS Register
Bits Description Type Reset
31:16 Reserved. - -
15 SM7 RO 0x0
14 SM6 RO 0x0
13 SM5 RO 0x0
12 SM4 RO 0x0
11 SM3 RO 0x0
10 SM2 RO 0x0
9 SM1 RO 0x0
8 SM0 RO 0x0
7 SM3_TXNFULL RO 0x0
6 SM2_TXNFULL RO 0x0
5 SM1_TXNFULL RO 0x0
4 SM0_TXNFULL RO 0x0
3 SM3_RXNEMPTY RO 0x0
2 SM2_RXNEMPTY RO 0x0
1 SM1_RXNEMPTY RO 0x0
0 SM0_RXNEMPTY RO 0




# Chapter 9. GPIO
 CAUTION
Under certain conditions, pull-down does not function as expected. For more information, see RP2350-E9.
9.1. Overview
RP2350 has up to 54 multi-functional General Purpose Input / Output (GPIO) pins, divided into two banks:
Bank 0
30 user GPIOs in the QFN-60 package (RP2350A), or 48 user GPIOs in the QFN-80 package
Bank 1
six QSPI IOs, and the USB DP/DM pins
You can control each GPIO from software running on the processors, or by a number of other functional blocks. To
meet USB rise and fall specifications, the analogue characteristics of the USB pins differ from the GPIO pads. As a
result, we do not include them in the 54 GPIO total. However, you can still use them for UART, I2C, or processorcontrolled GPIO through the single-cycle IO subsystem (SIO).
In a typical use case, the QSPI IOs are used to execute code from an external flash device, leaving 30 or 48 Bank 0
GPIOs for the programmer to use. The QSPI pins might become available for general purpose use when booting the chip
from internal OTP, or controlling the chip externally through SWD in an IO expander application.
All GPIOs support digital input and output. Several Bank 0 GPIOs can also be used as inputs to the chip’s Analogue to
Digital Converter (ADC):
• GPIOs 26 through 29 inclusive (four total) in the QFN-60 package
• GPIOs 40 through 47 (eight total) in the QFN-80 package
Bank 0 supports the following functions:
• Software control via SIO — Section 3.1.3, “GPIO control”
• Programmable IO (PIO) — Chapter 11, PIO
• 2 × SPI — Section 12.3, “SPI”
• 2 × UART — Section 12.1, “UART”
• 2 × I2C (two-wire serial interface) — Section 12.2, “I2C”
• 8 × two-channel PWM in the QFN-60 package, or 12 × in QFN-80 — Section 12.5, “PWM”
• 2 × external clock inputs — Section 8.1.2.4, “External clocks”
• 4 × general purpose clock output — Section 8.1, “Overview”
• 4 × input to ADC in the QFN-60 package, or 8 × in QFN-80 — Section 12.4, “ADC and Temperature Sensor”
• 1 × HSTX high-speed interface — Section 12.11, “HSTX”
• 1 × auxiliary QSPI chip select, for a second XIP device — Section 12.14, “QSPI memory interface (QMI)”
• CoreSight execution trace output — Section 3.5.7, “Trace”
• USB VBUS management — Section 12.7.3.10, “VBUS control”
• External interrupt requests, level or edge-sensitive — Section 9.5, “Interrupts”
Bank 1 contains the QSPI and USB DP/DM pins and supports the following functions:
RP2350 Datasheet
9.1. Overview 587
• Software control via SIO — Section 3.1.3, “GPIO control”
• Flash execute in place (Section 4.4, “External flash and PSRAM (XIP)”) via QSPI Memory Interface (QMI) — Section
12.14, “QSPI memory interface (QMI)”
• UART — Section 12.1, “UART”
• I2C (two-wire serial interface) — Section 12.2, “I2C”
The logical structure of an example IO is shown in Figure 41.
Figure 41. Logical
structure of a GPIO.
Each GPIO can be
controlled by one of a
number of peripherals,
or by software control
registers in the SIO.
The function select
(FSEL) selects which
peripheral output is in
control of the GPIO’s
direction and output
level, and which
peripheral input can
see this GPIO’s input
level. These three
signals (output level,
output enable, input
level) can also be
inverted or forced high
or low, using the GPIO
control registers.
9.2. Changes from RP2040
RP2350 GPIO differs from RP2040 in the following ways:
• 18 more GPIOs in the QFN-80 package
• Addition of a third PIO to GPIO functions
• USB DP/DM pins can be used as GPIO
• Addition of isolation register to pad registers (preserves pad state while in a low power state, cleared by software
on power up)
• Changed default reset state of pad controls
• Both Secure and Non-secure access to GPIOs (see Section 10.6)
• Double the number of GPIO interrupts to differentiate between Secure and Non-secure
• Interrupt summary registers added so you can quickly see which GPIOs have pending interrupts
9.3. Reset state
At first power up, Bank 0 IOs (GPIOs 0 through 29 in the QFN-60 package, and GPIOs 0 through 47 in the QFN-80
package) assume the following state:
• Output buffer is high-impedance
• Input buffer is disabled
• Pulled low
• Isolation latches are set to latched (Section 9.7)
The pad output disable bit (GPIO0.OD) for each pad is clear at reset, but the IO muxing is reset to the null function,
RP2350 Datasheet
9.2. Changes from RP2040 588
which ensures that the output buffer is high-impedance.
 IMPORTANT
The pad reset state is different from RP2040, which only disables digital inputs on GPIOs 26 through 29 (as of
version B2) and does not have isolation latches. Applications must enable the pad input (GPIO0.IE = 1) and disable
pad isolation latches (GPIO0.ISO = 0) before using the pads for digital I/O. The gpio_set_function() SDK function
performs these tasks automatically.
Bank 1 IOs have the same reset state as Bank 0 GPIOs, except for the input enable (IE) resetting to 1, and different pullup/pull-down states: SCK, SD0 and SD1 are pull-down, but SD2, SD3 and CSn are pull-up.
 NOTE
To use a Bank 0 GPIO as a second chip select, you need an external pull-up to ensure the second QSPI device does
not power up with its chip select asserted.
The pads return to the reset state on any of the following:
• A brownout reset
• Asserting the RUN pin low
• Setting SW-DP CDBGRSTREQ via SWD
• Setting RP-AP rescue reset via SWD
If a pad’s isolation latches are in the latched state (Section 9.7) then resetting the PADS and IO registers does not
physically return the pad to its reset state. The isolation latches prevent upstream signals from propagating to the pad.
Clear the ISO bit to allow signals to propagate.
9.4. Function select
To allocate a function to a GPIO, write to the FUNCSEL field in the CTRL register corresponding to the pin. For a list of GPIOs
and corresponding registers, see Table 645. For an example, see GPIO0_CTRL. The descriptions for the functions listed
in this table can be found in Table 646.
Each GPIO can only select one function at a time. Each peripheral input (e.g. UART0 RX) should only be selected by one
GPIO at a time. If you connect the same peripheral input to multiple GPIOs, the peripheral sees the logical OR of these
GPIO inputs.
RP2350 Datasheet
9.4. Function select 589
Table 645. General
Purpose Input/Output
(GPIO) Bank 0
Functions
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
0 SPI0 RX UART0 TX I2C0 SDA PWM0 A SIO PIO0 PIO1 PIO2 QMI CS1n USB OVCUR DET
1 SPI0 CSn UART0 RX I2C0 SCL PWM0 B SIO PIO0 PIO1 PIO2 TRACECLK USB VBUS DET
2 SPI0 SCK UART0 CTS I2C1 SDA PWM1 A SIO PIO0 PIO1 PIO2 TRACEDATA0 USB VBUS EN UART0 TX
3 SPI0 TX UART0 RTS I2C1 SCL PWM1 B SIO PIO0 PIO1 PIO2 TRACEDATA1 USB OVCUR DET UART0 RX
4 SPI0 RX UART1 TX I2C0 SDA PWM2 A SIO PIO0 PIO1 PIO2 TRACEDATA2 USB VBUS DET
5 SPI0 CSn UART1 RX I2C0 SCL PWM2 B SIO PIO0 PIO1 PIO2 TRACEDATA3 USB VBUS EN
6 SPI0 SCK UART1 CTS I2C1 SDA PWM3 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 TX
7 SPI0 TX UART1 RTS I2C1 SCL PWM3 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 RX
8 SPI1 RX UART1 TX I2C0 SDA PWM4 A SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS EN
9 SPI1 CSn UART1 RX I2C0 SCL PWM4 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
10 SPI1 SCK UART1 CTS I2C1 SDA PWM5 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 TX
11 SPI1 TX UART1 RTS I2C1 SCL PWM5 B SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 RX
12 HSTX SPI1 RX UART0 TX I2C0 SDA PWM6 A SIO PIO0 PIO1 PIO2 CLOCK GPIN0 USB OVCUR DET
13 HSTX SPI1 CSn UART0 RX I2C0 SCL PWM6 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT0 USB VBUS DET
14 HSTX SPI1 SCK UART0 CTS I2C1 SDA PWM7 A SIO PIO0 PIO1 PIO2 CLOCK GPIN1 USB VBUS EN UART0 TX
15 HSTX SPI1 TX UART0 RTS I2C1 SCL PWM7 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT1 USB OVCUR DET UART0 RX
16 HSTX SPI0 RX UART0 TX I2C0 SDA PWM0 A SIO PIO0 PIO1 PIO2 USB VBUS DET
17 HSTX SPI0 CSn UART0 RX I2C0 SCL PWM0 B SIO PIO0 PIO1 PIO2 USB VBUS EN
18 HSTX SPI0 SCK UART0 CTS I2C1 SDA PWM1 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART0 TX
19 HSTX SPI0 TX UART0 RTS I2C1 SCL PWM1 B SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS DET UART0 RX
20 SPI0 RX UART1 TX I2C0 SDA PWM2 A SIO PIO0 PIO1 PIO2 CLOCK GPIN0 USB VBUS EN
21 SPI0 CSn UART1 RX I2C0 SCL PWM2 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT0 USB OVCUR DET
22 SPI0 SCK UART1 CTS I2C1 SDA PWM3 A SIO PIO0 PIO1 PIO2 CLOCK GPIN1 USB VBUS DET UART1 TX
RP2350 Datasheet
9.4. Function select 590
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
23 SPI0 TX UART1 RTS I2C1 SCL PWM3 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT1 USB VBUS EN UART1 RX
24 SPI1 RX UART1 TX I2C0 SDA PWM4 A SIO PIO0 PIO1 PIO2 CLOCK GPOUT2 USB OVCUR DET
25 SPI1 CSn UART1 RX I2C0 SCL PWM4 B SIO PIO0 PIO1 PIO2 CLOCK GPOUT3 USB VBUS DET
26 SPI1 SCK UART1 CTS I2C1 SDA PWM5 A SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 TX
27 SPI1 TX UART1 RTS I2C1 SCL PWM5 B SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 RX
28 SPI1 RX UART0 TX I2C0 SDA PWM6 A SIO PIO0 PIO1 PIO2 USB VBUS DET
29 SPI1 CSn UART0 RX I2C0 SCL PWM6 B SIO PIO0 PIO1 PIO2 USB VBUS EN
GPIOs 30 through 47 are QFN-80 only:
30 SPI1 SCK UART0 CTS I2C1 SDA PWM7 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART0 TX
31 SPI1 TX UART0 RTS I2C1 SCL PWM7 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 RX
32 SPI0 RX UART0 TX I2C0 SDA PWM8 A SIO PIO0 PIO1 PIO2 USB VBUS EN
33 SPI0 CSn UART0 RX I2C0 SCL PWM8 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
34 SPI0 SCK UART0 CTS I2C1 SDA PWM9 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 TX
35 SPI0 TX UART0 RTS I2C1 SCL PWM9 B SIO PIO0 PIO1 PIO2 USB VBUS EN UART0 RX
36 SPI0 RX UART1 TX I2C0 SDA PWM10 A SIO PIO0 PIO1 PIO2 USB OVCUR DET
37 SPI0 CSn UART1 RX I2C0 SCL PWM10 B SIO PIO0 PIO1 PIO2 USB VBUS DET
38 SPI0 SCK UART1 CTS I2C1 SDA PWM11 A SIO PIO0 PIO1 PIO2 USB VBUS EN UART1 TX
39 SPI0 TX UART1 RTS I2C1 SCL PWM11 B SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 RX
40 SPI1 RX UART1 TX I2C0 SDA PWM8 A SIO PIO0 PIO1 PIO2 USB VBUS DET
41 SPI1 CSn UART1 RX I2C0 SCL PWM8 B SIO PIO0 PIO1 PIO2 USB VBUS EN
42 SPI1 SCK UART1 CTS I2C1 SDA PWM9 A SIO PIO0 PIO1 PIO2 USB OVCUR DET UART1 TX
43 SPI1 TX UART1 RTS I2C1 SCL PWM9 B SIO PIO0 PIO1 PIO2 USB VBUS DET UART1 RX
44 SPI1 RX UART0 TX I2C0 SDA PWM10 A SIO PIO0 PIO1 PIO2 USB VBUS EN
RP2350 Datasheet
9.4. Function select 591
GPIO F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
45 SPI1 CSn UART0 RX I2C0 SCL PWM10 B SIO PIO0 PIO1 PIO2 USB OVCUR DET
46 SPI1 SCK UART0 CTS I2C1 SDA PWM11 A SIO PIO0 PIO1 PIO2 USB VBUS DET UART0 TX
47 SPI1 TX UART0 RTS I2C1 SCL PWM11 B SIO PIO0 PIO1 PIO2 QMI CS1n USB VBUS EN UART0 RX
RP2350 Datasheet
9.4. Function select 592
Table 646. GPIO User
Bank function
descriptions
Function Name Description
SPIx Connect one of the internal PL022 SPI peripherals to GPIO.
UARTx Connect one of the internal PL011 UART peripherals to GPIO.
I2Cx Connect one of the internal DW I2C peripherals to GPIO.
PWMx A/B Connect a PWM slice to GPIO. There are twelve PWM slices, each with two output
channels (A/B). The B pin can also be used as an input, for frequency and duty cycle
measurement.
SIO Software control of GPIO from the Single-cycle IO (SIO) block. The SIO function (F5)
must be selected for the processors to drive a GPIO, but the input is always connected,
so software can check the state of GPIOs at any time.
PIOx Connect one of the programmable IO blocks (PIO) to GPIO. PIO can implement a wide
variety of interfaces, and has its own internal pin mapping hardware, allowing flexible
placement of digital interfaces on Bank 0 GPIOs. The PIO function (F6, F7, F8) must be
selected for PIO to drive a GPIO, but the input is always connected, so the PIOs can
always see the state of all pins.
HSTX Connect the high-speed transmit peripheral (HSTX) to GPIO.
CLOCK GPINx General purpose clock inputs. Can be routed to a number of internal clock domains on
RP2350, e.g. to provide a 1Hz clock for the AON Timer, or can be connected to an
internal frequency counter.
CLOCK GPOUTx General purpose clock outputs. Can drive a number of internal clocks (including PLL
outputs) onto GPIOs, with optional integer divide.
TRACECLK, TRACEDATAx CoreSight execution trace output from Cortex-M33 processors (Arm-only).
USB OVCUR DET/VBUS
DET/VBUS EN
USB power control signals to/from the internal USB controller.
QMI CS1n Auxiliary chip select for QSPI bus, to allow execute-in-place from an additional flash or
PSRAM device.
Bank 1 function select operates identically to Bank 0, but its registers are in a different register block, starting with
USBPHY_DP_CTRL.
Table 647. GPIO Bank
1 Functions
Pin F0 F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11
USB DP UART1 TX I2C0 SDA SIO
USB DM UART1 RX I2C0 SCL SIO
QSPI SCK QMI SCK UART1 CTS I2C1 SDA SIO UART1 TX
QSPI CSn QMI CS0n UART1 RTS I2C1 SCL SIO UART1 RX
QSPI SD0 QMI SD0 UART0 TX I2C0 SDA SIO
QSPI SD1 QMI SD1 UART0 RX I2C0 SCL SIO
QSPI SD2 QMI SD2 UART0 CTS I2C1 SDA SIO UART0 TX
QSPI SD3 QMI SD3 UART0 RTS I2C1 SCL SIO UART0 RX
Table 648. GPIO bank
1 function
descriptions
Function Name Description
UARTx Connect one of the internal PL011 UART peripherals to GPIO.
I2Cx Connect one of the internal DW I2C peripherals to GPIO.
RP2350 Datasheet
9.4. Function select 593
Function Name Description
SIO Software control of GPIO, from the single-cycle IO (SIO) block. The SIO function (F5) must be selected
for the processors to drive a GPIO, but the input is always connected, so software can check the state
of GPIOs at any time.
QMI QSPI memory interface peripheral, used for execute-in-place from external QSPI flash or PSRAM
memory devices.
The six QSPI Bank GPIO pins are typically used by the XIP peripheral to communicate with an external flash device.
However, there are two scenarios where the pins can be used as software-controlled GPIOs:
• If a SPI or Dual-SPI flash device is used for execute-in-place, then the SD2 and SD3 pins are not used for flash
access, and can be used for other GPIO functions on the circuit board.
• If RP2350 is used in a flashless configuration (USB and OTP boot only), then all six pins can be used for softwarecontrolled GPIO functions.
9.5. Interrupts
An interrupt can be generated for every GPIO pin in four scenarios:
• Level High: the GPIO pin is a logical 1
• Level Low: the GPIO pin is a logical 0
• Edge High: the GPIO has transitioned from a logical 0 to a logical 1
• Edge Low: the GPIO has transitioned from a logical 1 to a logical 0
The level interrupts are not latched. This means that if the pin is a logical 1 and the level high interrupt is active, it will
become inactive as soon as the pin changes to a logical 0. The edge interrupts are stored in the INTR register and can be
cleared by writing to the INTR register.
There are enable, status, and force registers for three interrupt destinations: proc 0, proc 1, and dormant_wake. For proc
0 the registers are enable (PROC0_INTE0), status (PROC0_INTS0), and force (PROC0_INTF0). Dormant wake is used to
wake the ROSC or XOSC up from dormant mode. See Section 6.5.6.2 for more information on dormant mode.
There is an interrupt output for each combination of IO bank, IRQ destination, and security domain. In total there are
twelve such outputs:
• IO Bank 0 to dormant wake (Secure and Non-secure)
• IO Bank 0 to proc 0 (Secure and Non-secure)
• IO Bank 0 to proc 1 (Secure and Non-secure)
• IO QSPI to dormant wake (Secure and Non-secure)
• IO QSPI to proc 0 (Secure and Non-secure)
• IO QSPI to proc 1 (Secure and Non-secure)
Each interrupt output has its own array of enable registers (INTE) that configures which GPIO events cause the interrupt
to assert. The interrupt asserts when at least one enabled event occurs, and de-asserts when all enabled events have
been acknowledged via the relevant INTR register.
This means the user can watch for several GPIO events at once.
Summary registers can be used to quickly check for pending GPIO interrupts. See IRQSUMMARY_PROC0_NONSECURE0
for an example.
RP2350 Datasheet
9.5. Interrupts 594
9.6. Pads
 CAUTION
Under certain conditions, pull-down does not function as expected. For more information, see RP2350-E9.
Each GPIO is connected off-chip via a pad. Pads are the electrical interface between the chip’s internal logic and
external circuitry. They translate signal voltage levels, support higher currents and offer some protection against
electrostatic discharge (ESD) events. You can adjust pad electrical behaviour to meet the requirements of external
circuitry in the following ways:
• Output drive strength can be set to 2mA, 4mA, 8mA or 12mA.
• Output slew rate can be set to slow or fast.
• Input hysteresis (Schmitt trigger mode) can be enabled.
• A pull-up or pull-down can be enabled, to set the output signal level when the output driver is disabled.
• The input buffer can be disabled, to reduce current consumption when the pad is unused, unconnected or
connected to an analogue signal.
An example pad is shown in Figure 42.
PAD
GPIO
Muxing
Slew Rate
Output Enable
Output Data
Drive Strength
Input Enable
Input Data
Schmitt Trigger
Pull Up / Pull Down
2
2
Figure 42. Diagram of
a single IO pad.
The pad’s Output Enable, Output Data and Input Data ports connect, via the IO mux, to the function controlling the pad.
All other ports are controlled from the pad control register. You can use this register to disable the pad’s output driver by
overriding the Output Enable signal from the function controlling the pad. See GPIO0 for an example of a pad control
register.
Both the output signal level and acceptable input signal level at the pad are determined by the digital IO supply (IOVDD).
IOVDD can be any nominal voltage between 1.8V and 3.3V, but to meet specification when powered at 1.8V, the pad
input thresholds must be adjusted by writing a 1 to the pad VOLTAGE_SELECT registers. By default, the pad input thresholds
are valid for an IOVDD voltage between 2.5V and 3.3V. Using a voltage of 1.8V with the default input thresholds is a safe
operating mode, but it will result in input thresholds that don’t meet specification.
 WARNING
Using IOVDD voltages greater than 1.8V, with the input thresholds set for 1.8V may result in damage to the chip.
Pad input threshold are adjusted on a per bank basis, with separate VOLTAGE_SELECT registers for the pads associated with
the User IO bank (IO Bank 0) and the QSPI IO bank. However, both banks share the same digital IO supply (IOVDD), so
both register should always be set to the same value.
Pad register details are available in Section 9.11.3, “Pad Control - User Bank” and Section 9.11.4, “Pad Control - QSPI
Bank”.
RP2350 Datasheet
9.6. Pads 595
9.6.1. Bus keeper mode
For each pad, only the pull-up or the pull-down resistor can be enabled at any given time. It is impossible to enable both
simultaneously. Instead, if you set both the GPIO0.PDE and GPIO0.PUE bits simultaneously then you enable bus keeper
mode, where the pad is:
• Pulled up when its input is high.
• Pulled down when its input is low.
When the output buffer is disabled, and the pad is not driven by any external source, this mode weakly retains the pad’s
current logical state. The pad does not float to mid-rail.
Bus keeper mode relies on control logic in the switched core domain, so does not function when the core is powered
down. Rather, powering down the core when bus keeper mode is enabled latches the current output controls (pull-up or
pull-down) in the pad isolation latches, as described in Section 9.7.
9.7. Pad isolation latches
RP2350 features extended low-power states that allow all internal logic, with the exception of POWMAN and some
CoreSight debug logic, to fully power down under software control. This includes powering down all peripherals, the IO
muxing, and the pad control registers, which brings with it the risk that pad signals may experience unwanted
transitions when entering and exiting low-power states.
To ensure that pad states are well-defined at all times, all signals passing from the switched core power domain to the
pads pass through isolation latches. In normal operation, the latches are transparent, so the pads are controlled fully by
logic inside the switched core power domain, such as UARTs or the processors. However, when the ISO bit for each pad
is set (e.g. GPIO0.ISO) or the switched core domain is powered down, the control signals currently presented to that pad
are latched until the isolation is disabled. This includes the output enable state, output high/low level, and pull-up/pulldown resistor enable. The input signal from the pad back into the switched core domain is not isolated.
Consequently, when switched core logic is powered down, all Bank 0 and Bank 1 pads maintain the output state they
held immediately before the power down, unless overridden by always-on logic in POWMAN. When the switched core
power domain powers back up, all the GPIO ISO bits reset to 1, so the pre-power down state continues to be maintained
until user software starts up and clears the ISO bit to indicate it is ready to use the pad again. Pads whose IO muxing
has not yet been set up can be left isolated indefinitely, and will maintain their pre-power down state.
when software has finished setting up the IO muxing for a given pad, and the peripheral that is to be muxed in, the ISO
bit should be cleared. At this point the isolation latches will become transparent again: output signals passing through
the IO muxing block are now reflected in the pad output state, so peripherals can communicate with the outside world.
This process allows the switched core domain to be power cycled without causing any transitions on the pad outputs
that may interfere with the operation of external hardware connected to the pads.
 NOTE
Non-SDK applications ported from RP2040 must clear the ISO bit before using a GPIO, as this feature was not
present on RP2040. The SDK automatically clears the ISO bit when gpio_set_function() is called.
The isolation latches themselves are reset by the always-on power domain reset, namely any one of:
• Power-on reset
• Brownout reset
• RUN pin being asserted low
• SW-DP CDBGRSTREQ
• RP-AP rescue reset
The latches reset to the reset value of the signal being isolated. For example, on Bank 0 GPIOs, the input enable control
RP2350 Datasheet
9.7. Pad isolation latches 596
(GPIO0.IE) resets to 0 (input-disabled), so the isolation latches for these signals also take a reset value of 0. Resetting
the isolation latch forces the pad to assume its reset state even if it is currently isolated.
The ISO control bits (e.g. GPIO0.ISO) are reset by the top-level switched core domain isolation signal, which is asserted
by POWMAN before powering down the switched core domain and de-asserted after it is powered up. This means that
entering and exiting a sleep state where the switched core domain is unpowered leaves all GPIOs isolated after power
up; you can then re-engage them individually. The ISO control bits are not reset by the PADS register block reset driven
by the RESETS control registers: resetting the PADS register block returns non-isolated pads to their reset state, but has
no effect on isolated pads.
9.8. Processor GPIO controls (SIO)
The single-cycle IO subsystem (Section 3.1) contains memory-mapped GPIO registers. The processors can use these to
perform input/output operations on GPIOs:
• The GPIO_OUT and GPIO_HI_OUT registers set the output level: 1 = high, 0 = low
• The GPIO_OE and GPIO_HI_OE registers set the output enable: 1 = output, 0 = input
• The GPIO_IN and GPIO_HI_IN registers read the GPIO inputs
These registers are all 32 bits in size. The low registers (e.g. GPIO_OUT) connect to GPIOs 0 through 31, and the high
registers (e.g. GPIO_HI_OUT) connect to GPIOs 32 through 47, the QSPI pads, and the USB DM/DP pads.
For the output and output enable registers to take effect, the SIO function must be selected on each GPIO (function 5).
However, the GPIO input registers read back the GPIO input values even when the SIO function is not selected, so the
processor can always check the input state of any pin.
The SIO GPIO registers are shared between the two processors and between the Secure and Non-secure security
domains. This avoids programming errors introduced by selecting multiple GPIO functions for access from different
contexts.
Non-secure code’s view of the SIO registers is restricted by the Non-secure GPIO mask defined in GPIO_NSMASK0 and
GPIO_NSMASK1. Non-secure writes to Secure GPIOs are ignored. Non-secure reads of Secure GPIOs return 0.
These registers are documented in more detail in the SIO GPIO register section (Section 3.1.3).
The DMA cannot access registers in the SIO subsystem. The recommended method to DMA to GPIOs is a PIO program
that continuously transfers TX FIFO data to the GPIO outputs, which provides more consistent timing than DMA directly
into GPIO registers.
9.9. GPIO coprocessor port
Coprocessor port 0 on each Cortex-M33 processor connects to a GPIO coprocessor interface. These coprocessor
instructions provide fast access to the SIO GPIO registers from Arm software:
• The equivalent of any SIO GPIO register access is a single instruction, without having to materialise a 32-bit
register address beforehand
• An indexed write operation on any single GPIO is a single instruction
• 64 bits can be read/written in a single instruction
This reduces the timing impact of GPIO accesses on surrounding software, for example when GPIO tracing has been
added to interrupt handlers diagnose complex timing issues.
Both Secure and Non-secure code may access the coprocessor. Non-secure code sees a restricted view of the GPIO
registers, defined by ACCESSCTRL GPIO_NSMASK0/1.
The GPIO coprocessor instruction set is documented in Section 3.6.1.
RP2350 Datasheet
9.8. Processor GPIO controls (SIO) 597
9.10. Software examples
9.10.1. Select an IO function
An IO pin can perform many different functions and must be configured before use. For example, you may want it to be
a UART_TX pin, or a PWM output. The SDK provides gpio_set_function for this purpose. Many SDK examples call
gpio_set_function early on to enable printing to a UART.
The SDK starts by defining a structure to represent the registers of IO Bank 0, the User IO bank. Each IO has a status
register, followed by a control register. For N IOs, the SDK instantiates the structure containing a status and control
register as io[N] to repeat it N times.
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2350/hardware_structs/include/hardware/structs/io_bank0.h Lines 179 - 445
179 typedef struct {
180 io_bank0_status_ctrl_hw_t io[48];
181
182 uint32_t _pad0[32];
183
184 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC0_SECURE0
  applies similarly to other array indexes)
185 _REG_(IO_BANK0_IRQSUMMARY_PROC0_SECURE0_OFFSET) // IO_BANK0_IRQSUMMARY_PROC0_SECURE0
186 // 0x80000000 [31] GPIO31 (0)
187 // 0x40000000 [30] GPIO30 (0)
188 // 0x20000000 [29] GPIO29 (0)
189 // 0x10000000 [28] GPIO28 (0)
190 // 0x08000000 [27] GPIO27 (0)
191 // 0x04000000 [26] GPIO26 (0)
192 // 0x02000000 [25] GPIO25 (0)
193 // 0x01000000 [24] GPIO24 (0)
194 // 0x00800000 [23] GPIO23 (0)
195 // 0x00400000 [22] GPIO22 (0)
196 // 0x00200000 [21] GPIO21 (0)
197 // 0x00100000 [20] GPIO20 (0)
198 // 0x00080000 [19] GPIO19 (0)
199 // 0x00040000 [18] GPIO18 (0)
200 // 0x00020000 [17] GPIO17 (0)
201 // 0x00010000 [16] GPIO16 (0)
202 // 0x00008000 [15] GPIO15 (0)
203 // 0x00004000 [14] GPIO14 (0)
204 // 0x00002000 [13] GPIO13 (0)
205 // 0x00001000 [12] GPIO12 (0)
206 // 0x00000800 [11] GPIO11 (0)
207 // 0x00000400 [10] GPIO10 (0)
208 // 0x00000200 [9] GPIO9 (0)
209 // 0x00000100 [8] GPIO8 (0)
210 // 0x00000080 [7] GPIO7 (0)
211 // 0x00000040 [6] GPIO6 (0)
212 // 0x00000020 [5] GPIO5 (0)
213 // 0x00000010 [4] GPIO4 (0)
214 // 0x00000008 [3] GPIO3 (0)
215 // 0x00000004 [2] GPIO2 (0)
216 // 0x00000002 [1] GPIO1 (0)
217 // 0x00000001 [0] GPIO0 (0)
218 io_ro_32 irqsummary_proc0_secure[2];
219
220 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0
  applies similarly to other array indexes)
221 _REG_(IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_PROC0_NONSECURE0
222 // 0x80000000 [31] GPIO31 (0)
RP2350 Datasheet
9.10. Software examples 598
223 // 0x40000000 [30] GPIO30 (0)
224 // 0x20000000 [29] GPIO29 (0)
225 // 0x10000000 [28] GPIO28 (0)
226 // 0x08000000 [27] GPIO27 (0)
227 // 0x04000000 [26] GPIO26 (0)
228 // 0x02000000 [25] GPIO25 (0)
229 // 0x01000000 [24] GPIO24 (0)
230 // 0x00800000 [23] GPIO23 (0)
231 // 0x00400000 [22] GPIO22 (0)
232 // 0x00200000 [21] GPIO21 (0)
233 // 0x00100000 [20] GPIO20 (0)
234 // 0x00080000 [19] GPIO19 (0)
235 // 0x00040000 [18] GPIO18 (0)
236 // 0x00020000 [17] GPIO17 (0)
237 // 0x00010000 [16] GPIO16 (0)
238 // 0x00008000 [15] GPIO15 (0)
239 // 0x00004000 [14] GPIO14 (0)
240 // 0x00002000 [13] GPIO13 (0)
241 // 0x00001000 [12] GPIO12 (0)
242 // 0x00000800 [11] GPIO11 (0)
243 // 0x00000400 [10] GPIO10 (0)
244 // 0x00000200 [9] GPIO9 (0)
245 // 0x00000100 [8] GPIO8 (0)
246 // 0x00000080 [7] GPIO7 (0)
247 // 0x00000040 [6] GPIO6 (0)
248 // 0x00000020 [5] GPIO5 (0)
249 // 0x00000010 [4] GPIO4 (0)
250 // 0x00000008 [3] GPIO3 (0)
251 // 0x00000004 [2] GPIO2 (0)
252 // 0x00000002 [1] GPIO1 (0)
253 // 0x00000001 [0] GPIO0 (0)
254 io_ro_32 irqsummary_proc0_nonsecure[2];
255
256 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC1_SECURE0
  applies similarly to other array indexes)
257 _REG_(IO_BANK0_IRQSUMMARY_PROC1_SECURE0_OFFSET) // IO_BANK0_IRQSUMMARY_PROC1_SECURE0
258 // 0x80000000 [31] GPIO31 (0)
259 // 0x40000000 [30] GPIO30 (0)
260 // 0x20000000 [29] GPIO29 (0)
261 // 0x10000000 [28] GPIO28 (0)
262 // 0x08000000 [27] GPIO27 (0)
263 // 0x04000000 [26] GPIO26 (0)
264 // 0x02000000 [25] GPIO25 (0)
265 // 0x01000000 [24] GPIO24 (0)
266 // 0x00800000 [23] GPIO23 (0)
267 // 0x00400000 [22] GPIO22 (0)
268 // 0x00200000 [21] GPIO21 (0)
269 // 0x00100000 [20] GPIO20 (0)
270 // 0x00080000 [19] GPIO19 (0)
271 // 0x00040000 [18] GPIO18 (0)
272 // 0x00020000 [17] GPIO17 (0)
273 // 0x00010000 [16] GPIO16 (0)
274 // 0x00008000 [15] GPIO15 (0)
275 // 0x00004000 [14] GPIO14 (0)
276 // 0x00002000 [13] GPIO13 (0)
277 // 0x00001000 [12] GPIO12 (0)
278 // 0x00000800 [11] GPIO11 (0)
279 // 0x00000400 [10] GPIO10 (0)
280 // 0x00000200 [9] GPIO9 (0)
281 // 0x00000100 [8] GPIO8 (0)
282 // 0x00000080 [7] GPIO7 (0)
283 // 0x00000040 [6] GPIO6 (0)
284 // 0x00000020 [5] GPIO5 (0)
285 // 0x00000010 [4] GPIO4 (0)
RP2350 Datasheet
9.10. Software examples 599
286 // 0x00000008 [3] GPIO3 (0)
287 // 0x00000004 [2] GPIO2 (0)
288 // 0x00000002 [1] GPIO1 (0)
289 // 0x00000001 [0] GPIO0 (0)
290 io_ro_32 irqsummary_proc1_secure[2];
291
292 // (Description copied from array index 0 register IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0
  applies similarly to other array indexes)
293 _REG_(IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_PROC1_NONSECURE0
294 // 0x80000000 [31] GPIO31 (0)
295 // 0x40000000 [30] GPIO30 (0)
296 // 0x20000000 [29] GPIO29 (0)
297 // 0x10000000 [28] GPIO28 (0)
298 // 0x08000000 [27] GPIO27 (0)
299 // 0x04000000 [26] GPIO26 (0)
300 // 0x02000000 [25] GPIO25 (0)
301 // 0x01000000 [24] GPIO24 (0)
302 // 0x00800000 [23] GPIO23 (0)
303 // 0x00400000 [22] GPIO22 (0)
304 // 0x00200000 [21] GPIO21 (0)
305 // 0x00100000 [20] GPIO20 (0)
306 // 0x00080000 [19] GPIO19 (0)
307 // 0x00040000 [18] GPIO18 (0)
308 // 0x00020000 [17] GPIO17 (0)
309 // 0x00010000 [16] GPIO16 (0)
310 // 0x00008000 [15] GPIO15 (0)
311 // 0x00004000 [14] GPIO14 (0)
312 // 0x00002000 [13] GPIO13 (0)
313 // 0x00001000 [12] GPIO12 (0)
314 // 0x00000800 [11] GPIO11 (0)
315 // 0x00000400 [10] GPIO10 (0)
316 // 0x00000200 [9] GPIO9 (0)
317 // 0x00000100 [8] GPIO8 (0)
318 // 0x00000080 [7] GPIO7 (0)
319 // 0x00000040 [6] GPIO6 (0)
320 // 0x00000020 [5] GPIO5 (0)
321 // 0x00000010 [4] GPIO4 (0)
322 // 0x00000008 [3] GPIO3 (0)
323 // 0x00000004 [2] GPIO2 (0)
324 // 0x00000002 [1] GPIO1 (0)
325 // 0x00000001 [0] GPIO0 (0)
326 io_ro_32 irqsummary_proc1_nonsecure[2];
327
328 // (Description copied from array index 0 register
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0 applies similarly to other array indexes)
329 _REG_(IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_SECURE0
330 // 0x80000000 [31] GPIO31 (0)
331 // 0x40000000 [30] GPIO30 (0)
332 // 0x20000000 [29] GPIO29 (0)
333 // 0x10000000 [28] GPIO28 (0)
334 // 0x08000000 [27] GPIO27 (0)
335 // 0x04000000 [26] GPIO26 (0)
336 // 0x02000000 [25] GPIO25 (0)
337 // 0x01000000 [24] GPIO24 (0)
338 // 0x00800000 [23] GPIO23 (0)
339 // 0x00400000 [22] GPIO22 (0)
340 // 0x00200000 [21] GPIO21 (0)
341 // 0x00100000 [20] GPIO20 (0)
342 // 0x00080000 [19] GPIO19 (0)
343 // 0x00040000 [18] GPIO18 (0)
344 // 0x00020000 [17] GPIO17 (0)
345 // 0x00010000 [16] GPIO16 (0)
RP2350 Datasheet
9.10. Software examples 600
346 // 0x00008000 [15] GPIO15 (0)
347 // 0x00004000 [14] GPIO14 (0)
348 // 0x00002000 [13] GPIO13 (0)
349 // 0x00001000 [12] GPIO12 (0)
350 // 0x00000800 [11] GPIO11 (0)
351 // 0x00000400 [10] GPIO10 (0)
352 // 0x00000200 [9] GPIO9 (0)
353 // 0x00000100 [8] GPIO8 (0)
354 // 0x00000080 [7] GPIO7 (0)
355 // 0x00000040 [6] GPIO6 (0)
356 // 0x00000020 [5] GPIO5 (0)
357 // 0x00000010 [4] GPIO4 (0)
358 // 0x00000008 [3] GPIO3 (0)
359 // 0x00000004 [2] GPIO2 (0)
360 // 0x00000002 [1] GPIO1 (0)
361 // 0x00000001 [0] GPIO0 (0)
362 io_ro_32 irqsummary_dormant_wake_secure[2];
363
364 // (Description copied from array index 0 register
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0 applies similarly to other array indexes)
365 _REG_(IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0_OFFSET) //
  IO_BANK0_IRQSUMMARY_DORMANT_WAKE_NONSECURE0
366 // 0x80000000 [31] GPIO31 (0)
367 // 0x40000000 [30] GPIO30 (0)
368 // 0x20000000 [29] GPIO29 (0)
369 // 0x10000000 [28] GPIO28 (0)
370 // 0x08000000 [27] GPIO27 (0)
371 // 0x04000000 [26] GPIO26 (0)
372 // 0x02000000 [25] GPIO25 (0)
373 // 0x01000000 [24] GPIO24 (0)
374 // 0x00800000 [23] GPIO23 (0)
375 // 0x00400000 [22] GPIO22 (0)
376 // 0x00200000 [21] GPIO21 (0)
377 // 0x00100000 [20] GPIO20 (0)
378 // 0x00080000 [19] GPIO19 (0)
379 // 0x00040000 [18] GPIO18 (0)
380 // 0x00020000 [17] GPIO17 (0)
381 // 0x00010000 [16] GPIO16 (0)
382 // 0x00008000 [15] GPIO15 (0)
383 // 0x00004000 [14] GPIO14 (0)
384 // 0x00002000 [13] GPIO13 (0)
385 // 0x00001000 [12] GPIO12 (0)
386 // 0x00000800 [11] GPIO11 (0)
387 // 0x00000400 [10] GPIO10 (0)
388 // 0x00000200 [9] GPIO9 (0)
389 // 0x00000100 [8] GPIO8 (0)
390 // 0x00000080 [7] GPIO7 (0)
391 // 0x00000040 [6] GPIO6 (0)
392 // 0x00000020 [5] GPIO5 (0)
393 // 0x00000010 [4] GPIO4 (0)
394 // 0x00000008 [3] GPIO3 (0)
395 // 0x00000004 [2] GPIO2 (0)
396 // 0x00000002 [1] GPIO1 (0)
397 // 0x00000001 [0] GPIO0 (0)
398 io_ro_32 irqsummary_dormant_wake_nonsecure[2];
399
400 // (Description copied from array index 0 register IO_BANK0_INTR0 applies similarly to
  other array indexes)
401 _REG_(IO_BANK0_INTR0_OFFSET) // IO_BANK0_INTR0
402 // Raw Interrupts
403 // 0x80000000 [31] GPIO7_EDGE_HIGH (0)
404 // 0x40000000 [30] GPIO7_EDGE_LOW (0)
405 // 0x20000000 [29] GPIO7_LEVEL_HIGH (0)
406 // 0x10000000 [28] GPIO7_LEVEL_LOW (0)
RP2350 Datasheet
9.10. Software examples 601
407 // 0x08000000 [27] GPIO6_EDGE_HIGH (0)
408 // 0x04000000 [26] GPIO6_EDGE_LOW (0)
409 // 0x02000000 [25] GPIO6_LEVEL_HIGH (0)
410 // 0x01000000 [24] GPIO6_LEVEL_LOW (0)
411 // 0x00800000 [23] GPIO5_EDGE_HIGH (0)
412 // 0x00400000 [22] GPIO5_EDGE_LOW (0)
413 // 0x00200000 [21] GPIO5_LEVEL_HIGH (0)
414 // 0x00100000 [20] GPIO5_LEVEL_LOW (0)
415 // 0x00080000 [19] GPIO4_EDGE_HIGH (0)
416 // 0x00040000 [18] GPIO4_EDGE_LOW (0)
417 // 0x00020000 [17] GPIO4_LEVEL_HIGH (0)
418 // 0x00010000 [16] GPIO4_LEVEL_LOW (0)
419 // 0x00008000 [15] GPIO3_EDGE_HIGH (0)
420 // 0x00004000 [14] GPIO3_EDGE_LOW (0)
421 // 0x00002000 [13] GPIO3_LEVEL_HIGH (0)
422 // 0x00001000 [12] GPIO3_LEVEL_LOW (0)
423 // 0x00000800 [11] GPIO2_EDGE_HIGH (0)
424 // 0x00000400 [10] GPIO2_EDGE_LOW (0)
425 // 0x00000200 [9] GPIO2_LEVEL_HIGH (0)
426 // 0x00000100 [8] GPIO2_LEVEL_LOW (0)
427 // 0x00000080 [7] GPIO1_EDGE_HIGH (0)
428 // 0x00000040 [6] GPIO1_EDGE_LOW (0)
429 // 0x00000020 [5] GPIO1_LEVEL_HIGH (0)
430 // 0x00000010 [4] GPIO1_LEVEL_LOW (0)
431 // 0x00000008 [3] GPIO0_EDGE_HIGH (0)
432 // 0x00000004 [2] GPIO0_EDGE_LOW (0)
433 // 0x00000002 [1] GPIO0_LEVEL_HIGH (0)
434 // 0x00000001 [0] GPIO0_LEVEL_LOW (0)
435 io_rw_32 intr[6];
436
437 union {
438 struct {
439 io_bank0_irq_ctrl_hw_t proc0_irq_ctrl;
440 io_bank0_irq_ctrl_hw_t proc1_irq_ctrl;
441 io_bank0_irq_ctrl_hw_t dormant_wake_irq_ctrl;
442 };
443 io_bank0_irq_ctrl_hw_t irq_ctrl[3];
444 };
445 } io_bank0_hw_t;
A similar structure is defined for the pad control registers for IO bank 1. By default, all pads come out of reset ready to
use, with input enabled and output disable set to 0. Regardless, gpio_set_function in the SDK sets the input enable and
clears the output disable to engage the pad’s IO buffers and connect internal signals to the outside world. Finally, the
desired function select is written to the IO control register (see GPIO0_CTRL for an example of an IO control register).
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 36 - 53
36 // Select function for this GPIO, and ensure input/output are enabled at the pad.
37 // This also clears the input/output/irq override bits.
38 void gpio_set_function(uint gpio, gpio_function_t fn) {
39 check_gpio_param(gpio);
40 invalid_params_if(HARDWARE_GPIO, ((uint32_t)fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) &
  ~IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS);
41 // Set input enable on, output disable off
42 hw_write_masked(&pads_bank0_hw->io[gpio],
43 PADS_BANK0_GPIO0_IE_BITS,
44 PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS
45 );
46 // Zero all fields apart from fsel; we want this IO to do what the peripheral tells it.
47 // This doesn't affect e.g. pullup/pulldown, as these are in pad controls.
48 io_bank0_hw->io[gpio].ctrl = fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
49 // Remove pad isolation now that the correct peripheral is in control of the pad
RP2350 Datasheet
9.10. Software examples 602
50 hw_clear_bits(&pads_bank0_hw->io[gpio], PADS_BANK0_GPIO0_ISO_BITS);
51 }
9.10.2. Enable a GPIO interrupt
The SDK provides a method of being interrupted when a GPIO pin changes state:
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 186 - 196
186 void gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled) {
187 // either this call disables the interrupt or callback should already be set.
188 // this protects against enabling the interrupt without callback set
189 assert(!enabled || irq_has_handler(IO_IRQ_BANK0));
190
191 // Separate mask/force/status per-core, so check which core called, and
192 // set the relevant IRQ controls.
193 io_bank0_irq_ctrl_hw_t *irq_ctrl_base = get_core_num() ?
194 &io_bank0_hw->proc1_irq_ctrl : &io_bank0_hw-
  >proc0_irq_ctrl;
195 _gpio_set_irq_enabled(gpio, events, enabled, irq_ctrl_base);
196 }
gpio_set_irq_enabled uses a lower level function _gpio_set_irq_enabled:
SDK: https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_gpio/gpio.c Lines 173 - 184
173 static void _gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled,
  io_bank0_irq_ctrl_hw_t *irq_ctrl_base) {
174 // Clear stale events which might cause immediate spurious handler entry
175 gpio_acknowledge_irq(gpio, events);
176
177 io_rw_32 *en_reg = &irq_ctrl_base->inte[gpio / 8];
178 events <<= 4 * (gpio % 8);
179
180 if (enabled)
181 hw_set_bits(en_reg, events);
182 else
183 hw_clear_bits(en_reg, events);
184 }
The user provides a pointer to a callback function that is called when the GPIO event happens. An example application
that uses this system is hello_gpio_irq:
Pico Examples: https://github.com/raspberrypi/pico-examples/blob/master/gpio/hello_gpio_irq/hello_gpio_irq.c
 1 /**
 2 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 3 *
 4 * SPDX-License-Identifier: BSD-3-Clause
 5 */
 6
 7 #include <stdio.h>
 8 #include "pico/stdlib.h"
 9 #include "hardware/gpio.h"
10
11 #define GPIO_WATCH_PIN 2
12
RP2350 Datasheet
9.10. Software examples 603
13 static char event_str[128];
14
15 void gpio_event_string(char *buf, uint32_t events);
16
17 void gpio_callback(uint gpio, uint32_t events) {
18 // Put the GPIO event(s) that just happened into event_str
19 // so we can print it
20 gpio_event_string(event_str, events);
21 printf("GPIO %d %s\n", gpio, event_str);
22 }
23
24 int main() {
25 stdio_init_all();
26
27 printf("Hello GPIO IRQ\n");
28 gpio_init(GPIO_WATCH_PIN);
29 gpio_set_irq_enabled_with_callback(GPIO_WATCH_PIN, GPIO_IRQ_EDGE_RISE |
  GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
30
31 // Wait forever
32 while (1);
33 }
34
35
36 static const char *gpio_irq_str[] = {
37 "LEVEL_LOW", // 0x1
38 "LEVEL_HIGH", // 0x2
39 "EDGE_FALL", // 0x4
40 "EDGE_RISE" // 0x8
41 };
42
43 void gpio_event_string(char *buf, uint32_t events) {
44 for (uint i = 0; i < 4; i++) {
45 uint mask = (1 << i);
46 if (events & mask) {
47 // Copy this event string into the user string
48 const char *event_str = gpio_irq_str[i];
49 while (*event_str != '\0') {
50 *buf++ = *event_str++;
51 }
52 events &= ~mask;
53
54 // If more events add ", "
55 if (events) {
56 *buf++ = ',';
57 *buf++ = ' ';
58 }
59 }
60 }
61 *buf++ = '\0';
62 }
9.11. List of registers
9.11.1. IO - User Bank
The User Bank IO registers start at a base address of 0x40028000 (defined as IO_BANK0_BASE in SDK).
RP2350 Datasheet
9.11. List of registers 604
Table 649. List of
IO_BANK0 registers
Offset Name Info
0x000 GPIO0_STATUS
0x004 GPIO0_CTRL
0x008 GPIO1_STATUS
0x00c GPIO1_CTRL
0x010 GPIO2_STATUS
0x014 GPIO2_CTRL
0x018 GPIO3_STATUS
0x01c GPIO3_CTRL
0x020 GPIO4_STATUS
0x024 GPIO4_CTRL
0x028 GPIO5_STATUS
0x02c GPIO5_CTRL
0x030 GPIO6_STATUS
0x034 GPIO6_CTRL
0x038 GPIO7_STATUS
0x03c GPIO7_CTRL
0x040 GPIO8_STATUS
0x044 GPIO8_CTRL
0x048 GPIO9_STATUS
0x04c GPIO9_CTRL
0x050 GPIO10_STATUS
0x054 GPIO10_CTRL
0x058 GPIO11_STATUS
0x05c GPIO11_CTRL
0x060 GPIO12_STATUS
0x064 GPIO12_CTRL
0x068 GPIO13_STATUS
0x06c GPIO13_CTRL
0x070 GPIO14_STATUS
0x074 GPIO14_CTRL
0x078 GPIO15_STATUS
0x07c GPIO15_CTRL
0x080 GPIO16_STATUS
0x084 GPIO16_CTRL
0x088 GPIO17_STATUS
0x08c GPIO17_CTRL
RP2350 Datasheet
9.11. List of registers 605
Offset Name Info
0x090 GPIO18_STATUS
0x094 GPIO18_CTRL
0x098 GPIO19_STATUS
0x09c GPIO19_CTRL
0x0a0 GPIO20_STATUS
0x0a4 GPIO20_CTRL
0x0a8 GPIO21_STATUS
0x0ac GPIO21_CTRL
0x0b0 GPIO22_STATUS
0x0b4 GPIO22_CTRL
0x0b8 GPIO23_STATUS
0x0bc GPIO23_CTRL
0x0c0 GPIO24_STATUS
0x0c4 GPIO24_CTRL
0x0c8 GPIO25_STATUS
0x0cc GPIO25_CTRL
0x0d0 GPIO26_STATUS
0x0d4 GPIO26_CTRL
0x0d8 GPIO27_STATUS
0x0dc GPIO27_CTRL
0x0e0 GPIO28_STATUS
0x0e4 GPIO28_CTRL
0x0e8 GPIO29_STATUS
0x0ec GPIO29_CTRL
0x0f0 GPIO30_STATUS
0x0f4 GPIO30_CTRL
0x0f8 GPIO31_STATUS
0x0fc GPIO31_CTRL
0x100 GPIO32_STATUS
0x104 GPIO32_CTRL
0x108 GPIO33_STATUS
0x10c GPIO33_CTRL
0x110 GPIO34_STATUS
0x114 GPIO34_CTRL
0x118 GPIO35_STATUS
0x11c GPIO35_CTRL
RP2350 Datasheet
9.11. List of registers 606
Offset Name Info
0x120 GPIO36_STATUS
0x124 GPIO36_CTRL
0x128 GPIO37_STATUS
0x12c GPIO37_CTRL
0x130 GPIO38_STATUS
0x134 GPIO38_CTRL
0x138 GPIO39_STATUS
0x13c GPIO39_CTRL
0x140 GPIO40_STATUS
0x144 GPIO40_CTRL
0x148 GPIO41_STATUS
0x14c GPIO41_CTRL
0x150 GPIO42_STATUS
0x154 GPIO42_CTRL
0x158 GPIO43_STATUS
0x15c GPIO43_CTRL
0x160 GPIO44_STATUS
0x164 GPIO44_CTRL
0x168 GPIO45_STATUS
0x16c GPIO45_CTRL
0x170 GPIO46_STATUS
0x174 GPIO46_CTRL
0x178 GPIO47_STATUS
0x17c GPIO47_CTRL
0x200 IRQSUMMARY_PROC0_SECURE0
0x204 IRQSUMMARY_PROC0_SECURE1
0x208 IRQSUMMARY_PROC0_NONSECURE0
0x20c IRQSUMMARY_PROC0_NONSECURE1
0x210 IRQSUMMARY_PROC1_SECURE0
0x214 IRQSUMMARY_PROC1_SECURE1
0x218 IRQSUMMARY_PROC1_NONSECURE0
0x21c IRQSUMMARY_PROC1_NONSECURE1
0x220 IRQSUMMARY_COMA_WAKE_SECURE
0
0x224 IRQSUMMARY_COMA_WAKE_SECURE
1
RP2350 Datasheet
9.11. List of registers 607
Offset Name Info
0x228 IRQSUMMARY_COMA_WAKE_NONSE
CURE0
0x22c IRQSUMMARY_COMA_WAKE_NONSE
CURE1
0x230 INTR0 Raw Interrupts
0x234 INTR1 Raw Interrupts
0x238 INTR2 Raw Interrupts
0x23c INTR3 Raw Interrupts
0x240 INTR4 Raw Interrupts
0x244 INTR5 Raw Interrupts
0x248 PROC0_INTE0 Interrupt Enable for proc0
0x24c PROC0_INTE1 Interrupt Enable for proc0
0x250 PROC0_INTE2 Interrupt Enable for proc0
0x254 PROC0_INTE3 Interrupt Enable for proc0
0x258 PROC0_INTE4 Interrupt Enable for proc0
0x25c PROC0_INTE5 Interrupt Enable for proc0
0x260 PROC0_INTF0 Interrupt Force for proc0
0x264 PROC0_INTF1 Interrupt Force for proc0
0x268 PROC0_INTF2 Interrupt Force for proc0
0x26c PROC0_INTF3 Interrupt Force for proc0
0x270 PROC0_INTF4 Interrupt Force for proc0
0x274 PROC0_INTF5 Interrupt Force for proc0
0x278 PROC0_INTS0 Interrupt status after masking & forcing for proc0
0x27c PROC0_INTS1 Interrupt status after masking & forcing for proc0
0x280 PROC0_INTS2 Interrupt status after masking & forcing for proc0
0x284 PROC0_INTS3 Interrupt status after masking & forcing for proc0
0x288 PROC0_INTS4 Interrupt status after masking & forcing for proc0
0x28c PROC0_INTS5 Interrupt status after masking & forcing for proc0
0x290 PROC1_INTE0 Interrupt Enable for proc1
0x294 PROC1_INTE1 Interrupt Enable for proc1
0x298 PROC1_INTE2 Interrupt Enable for proc1
0x29c PROC1_INTE3 Interrupt Enable for proc1
0x2a0 PROC1_INTE4 Interrupt Enable for proc1
0x2a4 PROC1_INTE5 Interrupt Enable for proc1
0x2a8 PROC1_INTF0 Interrupt Force for proc1
0x2ac PROC1_INTF1 Interrupt Force for proc1
RP2350 Datasheet
9.11. List of registers 608
Offset Name Info
0x2b0 PROC1_INTF2 Interrupt Force for proc1
0x2b4 PROC1_INTF3 Interrupt Force for proc1
0x2b8 PROC1_INTF4 Interrupt Force for proc1
0x2bc PROC1_INTF5 Interrupt Force for proc1
0x2c0 PROC1_INTS0 Interrupt status after masking & forcing for proc1
0x2c4 PROC1_INTS1 Interrupt status after masking & forcing for proc1
0x2c8 PROC1_INTS2 Interrupt status after masking & forcing for proc1
0x2cc PROC1_INTS3 Interrupt status after masking & forcing for proc1
0x2d0 PROC1_INTS4 Interrupt status after masking & forcing for proc1
0x2d4 PROC1_INTS5 Interrupt status after masking & forcing for proc1
0x2d8 DORMANT_WAKE_INTE0 Interrupt Enable for dormant_wake
0x2dc DORMANT_WAKE_INTE1 Interrupt Enable for dormant_wake
0x2e0 DORMANT_WAKE_INTE2 Interrupt Enable for dormant_wake
0x2e4 DORMANT_WAKE_INTE3 Interrupt Enable for dormant_wake
0x2e8 DORMANT_WAKE_INTE4 Interrupt Enable for dormant_wake
0x2ec DORMANT_WAKE_INTE5 Interrupt Enable for dormant_wake
0x2f0 DORMANT_WAKE_INTF0 Interrupt Force for dormant_wake
0x2f4 DORMANT_WAKE_INTF1 Interrupt Force for dormant_wake
0x2f8 DORMANT_WAKE_INTF2 Interrupt Force for dormant_wake
0x2fc DORMANT_WAKE_INTF3 Interrupt Force for dormant_wake
0x300 DORMANT_WAKE_INTF4 Interrupt Force for dormant_wake
0x304 DORMANT_WAKE_INTF5 Interrupt Force for dormant_wake
0x308 DORMANT_WAKE_INTS0 Interrupt status after masking & forcing for dormant_wake
0x30c DORMANT_WAKE_INTS1 Interrupt status after masking & forcing for dormant_wake
0x310 DORMANT_WAKE_INTS2 Interrupt status after masking & forcing for dormant_wake
0x314 DORMANT_WAKE_INTS3 Interrupt status after masking & forcing for dormant_wake
0x318 DORMANT_WAKE_INTS4 Interrupt status after masking & forcing for dormant_wake
0x31c DORMANT_WAKE_INTS5 Interrupt status after masking & forcing for dormant_wake
IO_BANK0: GPIO0_STATUS Register
Offset: 0x000
Table 650.
GPIO0_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
RP2350 Datasheet
9.11. List of registers 609
Bits Description Type Reset
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO0_CTRL Register
Offset: 0x004
Table 651.
GPIO0_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
RP2350 Datasheet
9.11. List of registers 610
Bits Description Type Reset
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TCK
0x01 → SPI0_RX
0x02 → UART0_TX
0x03 → I2C0_SDA
0x04 → PWM_A_0
0x05 → SIO_0
0x06 → PIO0_0
0x07 → PIO1_0
0x08 → PIO2_0
0x09 → XIP_SS_N_1
0x0a → USB_MUXING_OVERCURR_DETECT
0x1f → NULL
IO_BANK0: GPIO1_STATUS Register
Offset: 0x008
Table 652.
GPIO1_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO1_CTRL Register
Offset: 0x00c
Table 653.
GPIO1_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
RP2350 Datasheet
9.11. List of registers 611
Bits Description Type Reset
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TMS
0x01 → SPI0_SS_N
0x02 → UART0_RX
0x03 → I2C0_SCL
0x04 → PWM_B_0
RP2350 Datasheet
9.11. List of registers 612
Bits Description Type Reset
0x05 → SIO_1
0x06 → PIO0_1
0x07 → PIO1_1
0x08 → PIO2_1
0x09 → CORESIGHT_TRACECLK
0x0a → USB_MUXING_VBUS_DETECT
0x1f → NULL
IO_BANK0: GPIO2_STATUS Register
Offset: 0x010
Table 654.
GPIO2_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO2_CTRL Register
Offset: 0x014
Table 655.
GPIO2_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
RP2350 Datasheet
9.11. List of registers 613
Bits Description Type Reset
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output from peripheral signal selected by funcsel
0x1 → INVERT: drive output from inverse of peripheral signal selected by
funcsel
0x2 → LOW: drive output low
0x3 → HIGH: drive output high
11:5 Reserved. - -
4:0 FUNCSEL: 0-31 → selects pin function according to the gpio table
31 == NULL
RW 0x1f
Enumerated values:
0x00 → JTAG_TDI
0x01 → SPI0_SCLK
0x02 → UART0_CTS
0x03 → I2C1_SDA
0x04 → PWM_A_1
0x05 → SIO_2
0x06 → PIO0_2
0x07 → PIO1_2
0x08 → PIO2_2
0x09 → CORESIGHT_TRACEDATA_0
0x0a → USB_MUXING_VBUS_EN
0x0b → UART0_TX
0x1f → NULL
IO_BANK0: GPIO3_STATUS Register
Offset: 0x018
RP2350 Datasheet
9.11. List of registers 614
Table 656.
GPIO3_STATUS
Register
Bits Description Type Reset
31:27 Reserved. - -
26 IRQTOPROC: interrupt to processors, after override is applied RO 0x0
25:18 Reserved. - -
17 INFROMPAD: input signal from pad, before filtering and override are applied RO 0x0
16:14 Reserved. - -
13 OETOPAD: output enable to pad after register override is applied RO 0x0
12:10 Reserved. - -
9 OUTTOPAD: output signal to pad after register override is applied RO 0x0
8:0 Reserved. - -
IO_BANK0: GPIO3_CTRL Register
Offset: 0x01c
Table 657.
GPIO3_CTRL Register
Bits Description Type Reset
31:30 Reserved. - -
29:28 IRQOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the interrupt
0x1 → INVERT: invert the interrupt
0x2 → LOW: drive interrupt low
0x3 → HIGH: drive interrupt high
27:18 Reserved. - -
17:16 INOVER RW 0x0
Enumerated values:
0x0 → NORMAL: don’t invert the peri input
0x1 → INVERT: invert the peri input
0x2 → LOW: drive peri input low
0x3 → HIGH: drive peri input high
15:14 OEOVER RW 0x0
Enumerated values:
0x0 → NORMAL: drive output enable from peripheral signal selected by
funcsel
0x1 → INVERT: drive output enable from inverse of peripheral signal selected
by funcsel
0x2 → DISABLE: disable output
0x3 → ENABLE: enable output
13:12 OUTOVER