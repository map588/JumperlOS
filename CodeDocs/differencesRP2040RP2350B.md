Changes from RP2040
Register field types are unchanged.
Standard types
RW:
• Read/Write
• Read operation returns the register value
• Write operation updates the register value
RO:
• Read-only
• Read operation returns the register value
• Write operations are ignored
WO:
• Write-only
• Read operation returns 0
• Write operation updates the register value
Clear types
SC:
• Self-Clearing
• Writing a 1 to a bit in an SC field will trigger an event, once the event is triggered the bit clears automatically
• Writing a 0 to a bit in an SC field does nothing
WC:
• Write-Clear
• Writing a 1 to a bit in a WC field will write that bit to 0
RP2350 Datasheet
Changes from RP2040 1349
• Writing a 0 to a bit in a WC field does nothing
• Read operation returns the register value
FIFO types
These fields are used for reading and writing data to and from FIFOs. Accompanying registers provide FIFO control and
status. There is no fixed format for the control and status registers, as they are specific to each FIFO interface.
RWF:
• Read/Write FIFO
• Reading this field returns data from a FIFO
◦ When the read is complete, the data value is removed from the FIFO
◦
If the FIFO is empty, a default value will be returned; the default value is specific to each FIFO interface
• Data written to this field is pushed to a FIFO, Behaviour when the FIFO is full is specific to each FIFO interface
• Read and write operations may access different FIFOs
RF:
• Read FIFO
• Functions the same as RWF, but read-only
WF:
• Write FIFO
• Functions the same as RWF, but write-only




7.2. Changes from RP2040
RP2350 retains all RP2040 chip-level reset features.
RP2350 adds the following features:
• new chip reset sources:
◦
glitch detector
◦
watchdog
◦
debugger
• new destinations:
◦
new power management components
RP2350 makes the following modifications to existing features:
• Modified the CHIP_RESET register, which records the source of the last chip level reset. In RP2040, CHIP_RESET was
stored in the LDO_POR register block. In RP2350, CHIP_RESET was extended and moved to the POWMAN register block,
which is in the new always-on power domain (AON).
• Renamed the brownout reset (BOR) registers to brownout detect (BOD), added functionality, and moved them to the
new POWlMAN register block.
• Added more system reset stages. To support this, added additional Power-on State Machine fields and rearranged
the existing fields.
• Added additional RESETS registers and rearranged the existing fields.
• Extended watchdog options to enable triggers for new resets.
RP2350 Datasheet
7.1. Overview 494
 NOTE
Watchdog scratch registers are not preserved when the watchdog triggers a chip-level reset. However, watchdog
scratch registers are preserved after a system or subsystem reset. For general purpose scratch registers that do not
reset after a chip-level reset, see the POWMAN register block Section 6.4, “Power management (POWMAN) registers”.



8.2.2. Changes from RP2040
• Maximum crystal frequency increased from 15 MHz to 50 MHz, when appropriate range is selected in
CTRL.FREQ_RANGE
 NOTE
The above change applies when using the XOSC as a crystal oscillator, with a crystal connected between the XIN and
XOUT pins. When using the XOSC XIN pin as a CMOS clock input from an external oscillator, the maximum is always
50 MHz. You do not have to configure CTRL.FREQ_RANGE for the CMOS input case. The CMOS input behaviour is
the same as RP2040.
 NOTE
The maximum clk_ref frequency is 25 MHz. If you use a >25 MHz crystal as the source of clk_ref, you must divide
the XOSC output using the clk_ref divider.






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





12.3.1. Changes from RP2040
The output enable of the SSPTXD data output (connecting to pins listed as SPI0 TX and SPI1 TX in the GPIO function
tables) is controlled by the SPI peripheral nSSPOE signal. The peripheral automatically tristates its output when
deselected in slave mode. This makes software control of the output enable unnecessary even when multiple slaves
share the data lines





12.4.1. Changes from RP2040
• Removed spikes in differential nonlinearity at codes 0x200, 0x600, 0xa00 and 0xe00, as documented by erratum
RP2040-E11, improving the ADC’s precision by around 0.5 ENOB.
RP2350 Datasheet
12.4. ADC and Temperature Sensor 1068
• Increased the number of external ADC input channels from 4 to 8 channels, in the QFN-80 package only.



12.6.1. Changes from RP2040
The following new features have been added:
• Increased the number of DMA channels from 12 to 16.
• Increased the number of shared IRQ outputs from 2 to 4.
• Channels can be assigned to security domains using SECCFG_CH0 through SECCFG_CH15.
• The DMA now filters bus accesses using the built-in memory protection unit (Section 12.6.6.3).
• Interrupts can be assigned to security domains using SECCFG_IRQ0 through SECCFG_IRQ3.
• Pacing timers and the CRC sniffer can be assigned to security domains using the SECCFG_MISC register.
• The four most-significant bits of TRANS_COUNT (CH0_TRANS_COUNT) are redefined as the MODE field, which defines
what happens when TRANS_COUNT reaches zero:
RP2350 Datasheet
12.6. DMA 1095
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





12.9.2. Changes from RP2040
On RP2040, the watchdog contained a tick generator used to generate a 1μs tick for the watchdog. This was also
distributed to the system timer. On RP2350, the watchdog instead takes a tick input from the system-level ticks block.
See Section 8.5.
As on RP2040 the watchdog can trigger a PSM (Power-on State Machine) sequence to reset system components or it
can be used to reset selected subsystem components. On RP2350, the watchdog can also trigger a chip level reset.



12.10.2. Changes from RP2040
The RP2040 Real Time Clock (RTC) is not used in RP2350. Instead, RP2350 has a timer in the Always-On power domain
which is used for scheduling power-up events and can also be used as a real-time counter. The AON Timer works
differently from the RP2040 RTC. It counts milliseconds to 64 bits and this value can be used to calculate the date and
time in software if required.