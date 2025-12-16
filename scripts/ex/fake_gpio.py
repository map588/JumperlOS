"""
RS-485 Terminal with Fake GPIO (Node-Based API)

Full-featured RS-485 bidirectional terminal supporting:
- Transmit and receive with differential signaling via voltage source nodes
- Loopback diagnostic testing
- Full-duplex terminal mode
- Configurable baud rate, thresholds, parity, and stop bits
- Uses new node-based FakeGPIO API (j.TOP_RAIL, j.BOTTOM_RAIL, etc.)
- Type 'config' to change settings, 'exit' to quit
"""

import jumperless as j
import time
import sys

# ============================================================================
# RS-485 Configuration Class
# ============================================================================

class RS485Config:
    """Configuration parameters for RS-485 communication."""
    
    def __init__(self):
        # Hardware nodes
        self.tx_node_a = 20        # Transmit A+ pin
        self.tx_node_b = 21        # Transmit B- pin
        self.rx_node_a = 22        # Receive A+ pin (for full-duplex)
        self.rx_node_b = 23        # Receive B- pin (for full-duplex)
        
        # Signal voltage sources (node-based API)
        # Keep these as actual `node` objects for readability (j.TOP_RAIL, etc.)
        self.high_node = j.TOP_RAIL
        self.low_node = j.BOTTOM_RAIL
        
        # Set power rail voltages (optional - can also use hardware jumpers)
        # j.dac_set() accepts node constants: TOP_RAIL→DAC2, BOTTOM_RAIL→DAC3
        self.rail_voltage_high = 8.0     # Voltage for TOP_RAIL
        self.rail_voltage_low = -8.0     # Voltage for BOTTOM_RAIL
        
        # Receive thresholds
        self.threshold_high = 2.0  # Voltage above = HIGH
        self.threshold_low = 0.8   # Voltage below = LOW
        
        # UART parameters
        self.baud_rate = 300       # Bits per second
        self.data_bits = 8         # 7 or 8
        self.parity = 'N'          # 'N'=none, 'E'=even, 'O'=odd
        self.stop_bits = 1         # 1 or 2
        
        # Calculated
        self.bit_time_us = int(1000000 / self.baud_rate)
    
    def update_baud(self, baud):
        """Update baud rate and recalculate timing."""
        self.baud_rate = baud
        self.bit_time_us = int(1000000 / self.baud_rate)
    
    def show(self):
        """Display current configuration."""
        node_names = {100: 'GND', 101: 'TOP_RAIL', 102: 'BOTTOM_RAIL', 106: 'DAC0', 107: 'DAC1'}
        high_name = node_names.get(int(self.high_node), f'Node {int(self.high_node)}')
        low_name = node_names.get(int(self.low_node), f'Node {int(self.low_node)}')
        
        print("\n" + "="*70)
        print("RS-485 CONFIGURATION")
        print("="*70)
        print(f"  TX Nodes:  A+ = {self.tx_node_a}, B- = {self.tx_node_b}")
        print(f"  RX Nodes:  A+ = {self.rx_node_a}, B- = {self.rx_node_b}")
        print(f"  Volt Src:  HIGH = {high_name}, LOW = {low_name}")
        print(f"  Rail V:    TOP_RAIL = {self.rail_voltage_high}V, BOTTOM_RAIL = {self.rail_voltage_low}V")
        print(f"  RX Thres:  HIGH = {self.threshold_high}V, LOW = {self.threshold_low}V")
        print(f"  UART:      {self.baud_rate} baud, {self.data_bits}{self.parity}{self.stop_bits}")
        print(f"  Timing:    {self.bit_time_us} µs per bit")
        print("="*70 + "\n")

# ============================================================================
# RS-485 Transceiver Class
# ============================================================================

class RS485Terminal:
    """Full-duplex RS-485 terminal with transmit and receive."""
    
    def __init__(self, config):
        self.config = config
        self.tx_enabled = False
        self.rx_enabled = False
        
        # Transmit pins (OUTPUT mode)
        self.tx_a = None
        self.tx_b = None
        
        # Receive pin (INPUT mode - differential read from A pin)
        self.rx_a = None
        
        print("\n→ Initializing RS-485 terminal...")
    
    def init_transmit(self):
        """Initialize transmit pins."""
        if self.tx_enabled:
            return
        
        print(f"  TX: Nodes {self.config.tx_node_a} (A+) and {self.config.tx_node_b} (B-)")
        
        # Pin A: HIGH=TOP_RAIL, LOW=BOTTOM_RAIL (node-based API)
        self.tx_a = j.FakeGpioPin(
            self.config.tx_node_a, 
            j.OUTPUT, 
            self.config.high_node,
            self.config.low_node
        )
        
        # Pin B: HIGH=BOTTOM_RAIL, LOW=TOP_RAIL (inverted for differential)
        self.tx_b = j.FakeGpioPin(
            self.config.tx_node_b, 
            j.OUTPUT, 
            self.config.low_node,
            self.config.high_node
        )
        
        # Start in idle state (both HIGH)
        self.tx_a.on()
        self.tx_b.on()
        
        self.tx_enabled = True
        print("  ✓ Transmit ready")
    
    def init_receive(self):
        """Initialize receive pin."""
        if self.rx_enabled:
            return
        
        print(f"  RX: Node {self.config.rx_node_a} (A+) [differential input]")
        
        # Receive on A pin with configurable thresholds
        self.rx_a = j.FakeGpioPin(
            self.config.rx_node_a,
            j.INPUT,
            self.config.threshold_high,
            self.config.threshold_low
        )
        
        self.rx_enabled = True
        print("  ✓ Receive ready")

    def setup_loopback(self):
        """
        Connect TX nodes to RX nodes internally (software loopback).

        Without this (or physical jumpers), RX will float and often read 0, which
        looks like a permanent start bit and causes framing errors / all-zero bytes.
        """
        print("  Loopback: Connecting TX→RX internally")
        print(f"    {self.config.tx_node_a} (TX A+) → {self.config.rx_node_a} (RX A+)")
        print(f"    {self.config.tx_node_b} (TX B-) → {self.config.rx_node_b} (RX B-)")

        # These accept 2 args in MicroPython; they create the bridge immediately.
        j.connect(self.config.tx_node_a, self.config.rx_node_a)
        j.connect(self.config.tx_node_b, self.config.rx_node_b)

        # Give routing a moment to settle
        time.sleep_ms(50)
    
    def send_bit(self, bit):
        """Send a single bit."""
        if bit:
            self.tx_a.on()
            self.tx_b.on()
        else:
            self.tx_a.off()
            self.tx_b.off()
        time.sleep_us(self.config.bit_time_us)
    
    def calculate_parity(self, byte_val):
        """Calculate parity bit (even or odd)."""
        bits = bin(byte_val).count('1')
        if self.config.parity == 'E':  # Even parity
            return bits % 2  # 1 if odd number of bits
        elif self.config.parity == 'O':  # Odd parity
            return (bits + 1) % 2  # 1 if even number of bits
        return 0  # No parity
    
    def send_byte(self, byte_val):
        """Send one byte with configured format."""
        # Start bit (always 0)
        j.pause_core2(True)
        self.send_bit(0)
        
        # Data bits (LSB first)
        for i in range(self.config.data_bits):
            bit = (byte_val >> i) & 1
            self.send_bit(bit)
        
        # Parity bit (if enabled)
        if self.config.parity != 'N':
            parity_bit = self.calculate_parity(byte_val)
            self.send_bit(parity_bit)
        
        # Stop bit(s) (always 1)
        for _ in range(self.config.stop_bits):
            self.send_bit(1)
        j.pause_core2(False)

    def send_text(self, text):
        """Send a text string."""
        for char in text:
            self.send_byte(ord(char))
    
    def read_bit(self):
        """Read a single bit from receive pin."""
        value = self.rx_a.value()
        time.sleep_us(self.config.bit_time_us)
        return value
    
    def check_for_data(self):
        """
        Non-blocking check for incoming data.
        Returns True if start bit detected, False otherwise.
        """
        if not self.rx_enabled:
            return False
        return self.rx_a.value() == 0  # Start bit is LOW
    
    def receive_byte(self, timeout_ms=100, debug=False):
        """
        Receive one byte with timeout.
        Returns byte value or None if no data.
        """
        if not self.rx_enabled:
            return None
        
        # Wait for start bit (HIGH to LOW transition)
        start_time = time.ticks_ms()
        while self.rx_a.value() == 1:  # Wait for LOW
            if time.ticks_diff(time.ticks_ms(), start_time) > timeout_ms:
                return None  # Timeout
            time.sleep_us(100)
        
        if debug:
            print("\r[RX: Start bit detected]", end='')
        
        # Pause core2 to prevent interference during reception
        j.pause_core2(True)
        
        # Wait half a bit to sample in the middle of the start bit
        time.sleep_us(self.config.bit_time_us // 2)
        
        # Verify start bit
        start_bit = self.rx_a.value()
        if debug:
            print(f"[Start={start_bit}]", end='')
        
        # Read data bits
        byte_val = 0
        bits_str = ""
        for i in range(self.config.data_bits):
            time.sleep_us(self.config.bit_time_us)
            bit = self.rx_a.value()
            byte_val |= (bit << i)
            bits_str += str(bit)
        
        if debug:
            print(f"[Data={bits_str}]", end='')
        
        # Read parity bit (if enabled)
        if self.config.parity != 'N':
            time.sleep_us(self.config.bit_time_us)
            parity_bit = self.rx_a.value()
            expected_parity = self.calculate_parity(byte_val)
            if parity_bit != expected_parity:
                print("\r[PARITY ERROR]", end='')
        
        # Read stop bit(s)
        stop_bits_str = ""
        for _ in range(self.config.stop_bits):
            time.sleep_us(self.config.bit_time_us)
            stop_bit = self.rx_a.value()
            stop_bits_str += str(stop_bit)
            if stop_bit != 1:
                print("\r[FRAMING ERROR]", end='')
        
        if debug:
            print(f"[Stop={stop_bits_str}]", end='')
        
        j.pause_core2(False)
        
        return byte_val

# ============================================================================
# Configuration Menu
# ============================================================================

def configure(config):
    """Interactive configuration menu."""
    print("\n" + "="*70)
    print("CONFIGURATION MENU")
    print("="*70)
    print("  1. Baud rate")
    print("  2. Voltage sources (HIGH/LOW nodes)")
    print("  3. Power rail voltages (TOP_RAIL/BOTTOM_RAIL)")
    print("  4. RX thresholds")
    print("  5. Data bits (7 or 8)")
    print("  6. Parity (N/E/O)")
    print("  7. Stop bits (1 or 2)")
    print("  8. TX/RX nodes")
    print("  0. Show current config")
    print("  q. Return to terminal")
    print("="*70)
    
    choice = input("\nSelect option: ")
    
    if choice == '1':
        baud = int(input("Enter baud rate (e.g. 300, 9600, 115200): "))
        config.update_baud(baud)
        print(f"✓ Baud rate set to {baud} bps")
    
    elif choice == '2':
        print("Available voltage sources:")
        print("  100 = GND, 101 = TOP_RAIL, 102 = BOTTOM_RAIL")
        print("  106 = DAC0, 107 = DAC1")
        high = int(input("Enter HIGH node (e.g. 101 for TOP_RAIL): "))
        low = int(input("Enter LOW node (e.g. 102 for BOTTOM_RAIL): "))
        # Store as node objects for readability everywhere else
        config.high_node = j.node(high)
        config.low_node = j.node(low)
        print(f"✓ Voltage sources set to HIGH=node {high}, LOW=node {low}")
        print("  Note: Restart terminal for changes to take effect")
    
    elif choice == '3':
        print("Set power rail voltages (via DAC channels 2 & 3)")
        print("  j.dac_set(j.TOP_RAIL, voltage) → DAC channel 2")
        print("  j.dac_set(j.BOTTOM_RAIL, voltage) → DAC channel 3")
        v_high = float(input("Enter TOP_RAIL voltage (e.g. 8.0): "))
        v_low = float(input("Enter BOTTOM_RAIL voltage (e.g. -8.0): "))
        config.rail_voltage_high = v_high
        config.rail_voltage_low = v_low
        # Apply immediately
        j.dac_set(j.TOP_RAIL, v_high)
        j.dac_set(j.BOTTOM_RAIL, v_low)
        print(f"✓ Power rails set to TOP={v_high}V, BOTTOM={v_low}V")
    
    elif choice == '4':
        th = float(input("Enter HIGH threshold (e.g. 2.0): "))
        tl = float(input("Enter LOW threshold (e.g. 0.8): "))
        config.threshold_high = th
        config.threshold_low = tl
        print(f"✓ Thresholds set to HIGH={th}V, LOW={tl}V")
        print("  Note: Restart terminal for changes to take effect")
    
    elif choice == '5':
        bits = int(input("Enter data bits (7 or 8): "))
        if bits in [7, 8]:
            config.data_bits = bits
            print(f"✓ Data bits set to {bits}")
        else:
            print("✗ Invalid value")
    
    elif choice == '6':
        parity = input("Enter parity (N=none, E=even, O=odd): ").upper()
        if parity in ['N', 'E', 'O']:
            config.parity = parity
            names = {'N': 'none', 'E': 'even', 'O': 'odd'}
            print(f"✓ Parity set to {names[parity]}")
        else:
            print("✗ Invalid value")
    
    elif choice == '7':
        stop = int(input("Enter stop bits (1 or 2): "))
        if stop in [1, 2]:
            config.stop_bits = stop
            print(f"✓ Stop bits set to {stop}")
        else:
            print("✗ Invalid value")
    
    elif choice == '8':
        print("Current nodes:")
        print(f"  TX: A+={config.tx_node_a}, B-={config.tx_node_b}")
        print(f"  RX: A+={config.rx_node_a}, B-={config.rx_node_b}")
        change = input("Change? (y/n): ")
        if change.lower() == 'y':
            config.tx_node_a = int(input("TX A+ node: "))
            config.tx_node_b = int(input("TX B- node: "))
            config.rx_node_a = int(input("RX A+ node: "))
            config.rx_node_b = int(input("RX B- node: "))
            print("✓ Nodes updated")
            print("  Note: Restart terminal for changes to take effect")
    
    elif choice == '0':
        config.show()
    
    elif choice.lower() == 'q':
        return

# ============================================================================
# Main Terminal
# ============================================================================

def run_terminal():
    """Main terminal loop."""
    print("\n" + "="*70)
    print("RS-485 TERMINAL - Fake GPIO (Node-Based API)")
    print("="*70)
    print("Commands:")
    print("  config  - Open configuration menu")
    print("  exit    - Quit terminal")
    print("="*70)
    
    # Initialize configuration
    config = RS485Config()
    config.show()
    
    # Set power rail voltages
    print("→ Setting power rail voltages...")
    # Use config's selected nodes (node objects). `j.dac_set()` accepts node objects.
    j.dac_set(config.high_node, config.rail_voltage_high)
    j.dac_set(config.low_node, config.rail_voltage_low)
    print(f"  TOP_RAIL = {config.rail_voltage_high}V")
    print(f"  BOTTOM_RAIL = {config.rail_voltage_low}V")
    
    # Ask user for mode
    print("\nSelect mode:")
    print("  1. Loopback test (diagnostic - recommended to run first)")
    print("  2. Full-duplex (TX + RX)")
    mode = input("Mode (1/2): ")
    
    # Clear existing connections
    print("\n→ Clearing connections...")
    j.nodes_clear()
    time.sleep(0.2)
    
    # Create terminal
    terminal = RS485Terminal(config)
    
    # Both modes need transmit and receive
    terminal.init_transmit()
    terminal.init_receive()
    
    print("\n✓ Terminal ready!")
    print(f"   Format: {config.baud_rate} baud, {config.data_bits}{config.parity}{config.stop_bits}")
    
    node_names = {100: 'GND', 101: 'TOP_RAIL', 102: 'BOTTOM_RAIL', 106: 'DAC0', 107: 'DAC1'}
    high_name = node_names.get(int(config.high_node), f'Node {int(config.high_node)}')
    low_name = node_names.get(int(config.low_node), f'Node {int(config.low_node)}')
    print(f"   Voltage sources: {high_name} / {low_name}")
    
    if mode == '1':
        print("\n[Loopback Test Mode]\n")
        # Loopback test should Just Work on a single board.
        terminal.setup_loopback()
        run_loopback_test(terminal, config)
    elif mode == '2':
        print("\n[Full-Duplex Mode] TX and RX active\n")
        # In full-duplex, user might be talking to an external RS-485 device.
        # Offer a prompt to auto-wire internal loopback.
        try:
            resp = input("Auto-connect TX→RX loopback internally? (Y/n): ").strip().lower()
        except Exception:
            resp = "y"
        if resp in ("", "y", "yes"):
            terminal.setup_loopback()
        run_fullduplex_mode(terminal, config)
    else:
        print("Invalid mode selected")

def run_fullduplex_mode(terminal, config):
    """Full-duplex mode: interleaved RX monitoring and TX input."""
    print("\nNote: In full-duplex mode, the terminal alternates between:")
    print("  - Checking for incoming data (500ms)")
    print("  - Allowing you to type and send (press Enter to send)")
    print("\nTip: Keep messages short for better interactivity\n")
    
    # Diagnostic: Check initial RX state
    print(f"DEBUG: Initial RX state = {terminal.rx_a.value()}")
    print(f"DEBUG: TX idle - sending 5 HIGH bits, then 5 LOW bits...")
    for i in range(5):
        terminal.tx_a.on()
        terminal.tx_b.on()
        time.sleep_us(config.bit_time_us)
        print(f"  TX=HIGH, RX reads: {terminal.rx_a.value()}")
    for i in range(5):
        terminal.tx_a.off()
        terminal.tx_b.off()
        time.sleep_us(config.bit_time_us)
        print(f"  TX=LOW, RX reads: {terminal.rx_a.value()}")
    print("DEBUG: Test complete. If RX values don't change, check wiring.\n")
    
    rx_buffer = ""
    
    try:
        while True:
            # Phase 1: Check for incoming data (non-blocking with timeout)
            print("RX> ", end='')
            start_time = time.ticks_ms()
            received_any = False
            
            while time.ticks_diff(time.ticks_ms(), start_time) < 500:  # 500ms window
                byte_val = terminal.receive_byte(timeout_ms=10, debug=True)
                if byte_val is not None:
                    received_any = True
                    if 32 <= byte_val <= 126:  # Printable ASCII
                        print(chr(byte_val), end='')
                        rx_buffer += chr(byte_val)
                    elif byte_val == 13:  # CR
                        print()  # New line
                        if rx_buffer:
                            print(f"    (Received: '{rx_buffer}')")
                            rx_buffer = ""
                        print("RX> ", end='')
                    elif byte_val == 10:  # LF
                        pass  # Ignore
                    else:
                        print(f"[0x{byte_val:02X}]", end='')
            
            if not received_any:
                print("[listening...]")
            else:
                print()  # New line after receive window
            
            # Phase 2: Allow user to type and send
            try:
                line = input("TX> ")
                
                if line.lower() == 'exit':
                    print("→ Exiting...")
                    break
                elif line.lower() == 'config':
                    configure(config)
                    continue
                
                if line:
                    terminal.send_text(line + '\r\n')
                    print(f"  ✓ Sent: '{line}'")
                    
                    # LOOPBACK: Immediately check for echo
                    # At 300 baud, each char takes ~33ms, so we need to start receiving NOW
                    print("    [Checking for loopback echo...]")
                    echo_buffer = ""
                    chars_to_receive = len(line) + 2  # line + CR + LF
                    
                    for _ in range(chars_to_receive):
                        byte_val = terminal.receive_byte(timeout_ms=50)
                        if byte_val is not None:
                            if 32 <= byte_val <= 126:
                                echo_buffer += chr(byte_val)
                            elif byte_val == 13:
                                echo_buffer += '<CR>'
                            elif byte_val == 10:
                                echo_buffer += '<LF>'
                            else:
                                echo_buffer += f'[0x{byte_val:02X}]'
                        else:
                            break  # Timeout, no more data
                    
                    if echo_buffer:
                        print(f"    ✓ Echo received: {echo_buffer}")
                    else:
                        print(f"    [No echo received - check loopback wiring]")
            except KeyboardInterrupt:
                print("\n\n→ Exiting...")
                break
    
    except Exception as e:
        print(f"\nError: {e}")
        # import sys
        # sys.print_exception(e)

def run_loopback_test(terminal, config):
    """Diagnostic mode to test loopback wiring."""
    node_names = {100: 'GND', 101: 'TOP_RAIL', 102: 'BOTTOM_RAIL', 106: 'DAC0', 107: 'DAC1'}
    high_name = node_names.get(int(config.high_node), f'Node {int(config.high_node)}')
    low_name = node_names.get(int(config.low_node), f'Node {int(config.low_node)}')
    
    print("="*70)
    print("LOOPBACK DIAGNOSTIC TEST")
    print("="*70)
    print("\nThis test helps verify your loopback wiring.")
    print(f"Expected wiring: Node {config.tx_node_a} (TX A+) → Node {config.rx_node_a} (RX A+)")
    print("\nTest 1: Static signal test")
    print("-" * 70)
    
    # Test 1: Send static signals and check RX reads them correctly
    print("\nSending HIGH (idle/mark state)...")
    terminal.tx_a.on()
    terminal.tx_b.on()
    time.sleep_us(5000)
    rx_high = terminal.rx_a.value()
    print(f"  TX A+: {high_name} (on)")
    print(f"  RX reads: {rx_high} (expected: 1)")
    
    print("\nSending LOW (space state)...")
    terminal.tx_a.off()
    terminal.tx_b.off()
    time.sleep_us(5000)
    rx_low = terminal.rx_a.value()
    print(f"  TX A+: {low_name} (off)")
    print(f"  RX reads: {rx_low} (expected: 0)")
    
    if rx_high == 1 and rx_low == 0:
        print("\n✓ Static test PASSED - RX correctly reads TX signals")
    else:
        print("\n✗ Static test FAILED - Check wiring!")
        print(f"   Verify Node {config.tx_node_a} is connected to Node {config.rx_node_a}")
        
    
    # Test 2: Manual bit-by-bit loopback
    print("\n" + "="*70)
    print("Test 2: Bit-level loopback test")
    print("-" * 70)
    print("\nSending pattern 0x55 (01010101 binary) and reading each bit back...")
    print("Expected frame: [START=0][01010101][STOP=1]")
    
    # Return to idle
    terminal.tx_a.on()
    terminal.tx_b.on()
    time.sleep_us(config.bit_time_us * 5)
    
    # Manually send 0x55 and read back simultaneously
    byte_to_send = 0x55
    
    print("\nBit-by-bit transmission and readback:")
    
    # Start bit (0)
    j.pause_core2(True)
    terminal.tx_a.off()
    terminal.tx_b.off()
    time.sleep_us(config.bit_time_us // 2)  # Wait half a bit
    rx_start = terminal.rx_a.value()
    time.sleep_us(config.bit_time_us // 2)  # Complete the bit
    print(f"  START bit: TX=0, RX={rx_start} {'✓' if rx_start == 0 else '✗'}")
    
    # Data bits (LSB first)
    rx_byte = 0
    for i in range(8):
        tx_bit = (byte_to_send >> i) & 1
        if tx_bit:
            terminal.tx_a.on()
            terminal.tx_b.on()
        else:
            terminal.tx_a.off()
            terminal.tx_b.off()
        
        time.sleep_us(config.bit_time_us // 2)  # Wait half a bit
        rx_bit = terminal.rx_a.value()
        time.sleep_us(config.bit_time_us // 2)  # Complete the bit
        rx_byte |= (rx_bit << i)
        
        match = '✓' if rx_bit == tx_bit else '✗'
        print(f"  Data bit {i}: TX={tx_bit}, RX={rx_bit} {match}")
    
    # Stop bit (1)
    terminal.tx_a.on()
    terminal.tx_b.on()
    time.sleep_us(config.bit_time_us // 2)
    rx_stop = terminal.rx_a.value()
    time.sleep_us(config.bit_time_us // 2)
    print(f"  STOP bit: TX=1, RX={rx_stop} {'✓' if rx_stop == 1 else '✗'}")
    
    j.pause_core2(False)
    
    print(f"\nResult: Sent 0x{byte_to_send:02X}, Received 0x{rx_byte:02X}")
    if rx_byte == byte_to_send:
        print("✓ BIT-LEVEL TEST PASSED - Perfect loopback!")
    else:
        print(f"✗ BIT-LEVEL TEST FAILED - Bit errors detected")
    
    # Test 3: Byte-level loopback with immediate readback
    print("\n" + "="*70)
    print("Test 3: Byte loopback function test")
    print("-" * 70)
    print("\nTesting complete byte loopback for ASCII 'A' through 'E'...")
    
    def send_and_receive_byte(terminal, byte_val):
        """Send a byte and simultaneously read it back bit-by-bit."""
        # Return to idle
        terminal.tx_a.on()
        terminal.tx_b.on()
        time.sleep_us(config.bit_time_us * 2)
        
        j.pause_core2(True)
        
        # Start bit
        terminal.tx_a.off()
        terminal.tx_b.off()
        time.sleep_us(config.bit_time_us // 2)
        rx_start = terminal.rx_a.value()
        time.sleep_us(config.bit_time_us // 2)
        
        if rx_start != 0:
            j.pause_core2(False)
            return None  # Start bit failed
        
        # Data bits
        rx_byte = 0
        for i in range(8):
            tx_bit = (byte_val >> i) & 1
            if tx_bit:
                terminal.tx_a.on()
                terminal.tx_b.on()
            else:
                terminal.tx_a.off()
                terminal.tx_b.off()
            
            time.sleep_us(config.bit_time_us // 2)
            rx_bit = terminal.rx_a.value()
            time.sleep_us(config.bit_time_us // 2)
            rx_byte |= (rx_bit << i)
        
        # Stop bit
        terminal.tx_a.on()
        terminal.tx_b.on()
        time.sleep_us(config.bit_time_us // 2)
        rx_stop = terminal.rx_a.value()
        time.sleep_us(config.bit_time_us // 2)
        
        j.pause_core2(False)
        
        if rx_stop != 1:
            return None  # Framing error
        
        return rx_byte
    
    success_count = 0
    for char in ['A', 'B', 'C', 'D', 'E']:
        byte_val = ord(char)
        received = send_and_receive_byte(terminal, byte_val)
        
        if received == byte_val:
            print(f"  '{char}' (0x{byte_val:02X}): ✓ MATCH")
            success_count += 1
        elif received is not None:
            print(f"  '{char}' (0x{byte_val:02X}): ✗ Got 0x{received:02X}")
        else:
            print(f"  '{char}' (0x{byte_val:02X}): ✗ FRAMING ERROR")
    
    print("\n" + "="*70)
    print(f"Results: {success_count}/5 successful")
    
    if success_count == 5:
        print("✓ LOOPBACK TEST PASSED - System is working perfectly!")
        print("\nYour RS-485 loopback is working correctly.")
        print("The issue with full-duplex terminal mode is likely due to")
        print("timing constraints of single-threaded synchronous TX+RX.")
    elif success_count > 0:
        print("⚠ PARTIAL SUCCESS - Some bit errors detected")
    else:
        print("✗ LOOPBACK TEST FAILED")
    
    print("="*70)
    input("\nPress Enter to return to menu...")

# ============================================================================
# Run
# ============================================================================

run_terminal()
