// SPDX-License-Identifier: MIT

#include "ArduinoStuff.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "SerialUART.h"
#include "config.h"

#include "AsyncPassthrough.h"
#include "CH446Q.h"
#include "Commands.h"
#include "FileParsing.h"
#include "Graphics.h"
#include "NetManager.h"
#include "config.h"
#include "configManager.h"
#include "hardware/uart.h"
#include "usb_interface_config.h"
#include "States.h"
#include "Jerial.h"

#include "hardware/structs/io_bank0.h"
#include "hardware/structs/xip.h"

extern void tud_task(void);  // TinyUSB task processing

// #include "SerialWrapper.h"

// #include <SoftwareSerial.h>

// #if USB_CDC_ENABLE_COUNT >= 2
Adafruit_USBD_CDC USBSer1;
// #endif

// #if USB_CDC_ENABLE_COUNT >= 3
Adafruit_USBD_CDC USBSer2;
// #endif

// #if USB_CDC_ENABLE_COUNT >= 4
Adafruit_USBD_CDC USBSer3;
// #endif

// #if USB_CDC_ENABLE_COUNT >= 5
// Adafruit_USBD_CDC Serial4;
// #endif

// general debug printing
#define ARDUINO_DEBUG_PRINTLN( x )                 \
    if ( debugArduino > 0 ) {                      \
        changeTerminalColor( 38, true, &Serial );  \
        Serial.println( x );                       \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG_PRINT( x )                   \
    if ( debugArduino > 0 ) {                      \
        changeTerminalColor( 38, true, &Serial );  \
        Serial.print( x );                         \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG_PRINTF( x, ... )             \
    if ( debugArduino > 0 ) {                      \
        changeTerminalColor( 38, true, &Serial );  \
        Serial.printf( x, __VA_ARGS__ );           \
        changeTerminalColor( -1, false, &Serial ); \
    }
// we'll use this for sending data to the USBSer1
#define ARDUINO_DEBUG2_PRINTLN( x )                \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 43, true, &Serial );  \
        Serial.println( x );                       \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG2_PRINT( x )                  \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 43, true, &Serial );  \
        Serial.print( x );                         \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG2_PRINTF( x, ... )            \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 43, true, &Serial );  \
        Serial.printf( x, __VA_ARGS__ );           \
        changeTerminalColor( -1, false, &Serial ); \
    }

// we'll use this for sending data to the Arduino
#define ARDUINO_DEBUG3_PRINTLN( x )                \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 117, true, &Serial ); \
        Serial.println( x );                       \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG3_PRINT( x )                  \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 117, true, &Serial ); \
        Serial.print( x );                         \
        changeTerminalColor( -1, false, &Serial ); \
    }
#define ARDUINO_DEBUG3_PRINTF( x, ... )            \
    if ( debugArduino > 1 ) {                      \
        changeTerminalColor( 117, true, &Serial ); \
        Serial.printf( x, __VA_ARGS__ );           \
        changeTerminalColor( -1, false, &Serial ); \
    }

#define ARDUINO_DEBUG_FLUSH( ) \
    if ( debugArduino > 0 ) {  \
        Serial.flush( );       \
    }

bool arduinoDTR[ 3 ] = { false, false, false };

bool arduinoDTRpulse = false;

int debugArduino = 0;
int connectOnBoot1 = 0;
int connectOnBoot2 = 0;
int lockConnection1 = 0;
int lockConnection2 = 0;

int baudRateUSBSer1 = 115200; // for Arduino-Serial
int baudRateUSBSer2 = 115200; // for Routable Serial

volatile int backpowered = 1;

volatile bool flashingArduino = false;



void initArduino( void ) // if the UART is set up, the Arduino won't flash from
                         // it's own USB port
{

    // Serial1.setRX(17);

    // Serial1.setTX(16);

    // Serial1.begin(115200);

    // delay(1);

    pinMode( 18, INPUT );
    SetArduinoResetLine(LOW, 1);


    USBSer3.begin(115200);
   // pinMode( 19, INPUT );
}

bool ManualArduinoReset = false;
bool LastArduinoDTR = true;
bool LastRoutableDTR = true;
uint8_t numbitsUSBSer1 = 8;
uint8_t paritytypeUSBSer1 = 0;
uint8_t stopbitsUSBSer1 = 0;
uint8_t numbitsUSBSer2 = 8;
uint8_t paritytypeUSBSer2 = 0;
uint8_t stopbitsUSBSer2 = 0;

unsigned long FirstDTRTime = 5000;
bool FirstDTR = true;
bool ESPBoot = false;
unsigned long ESPBootTime = 5000;

unsigned long microsPerByteSerial1 = ( 1000000 / 115200 + 1 ) * ( 8 + 0 + 1 );
unsigned long microsPerByteSerial2 = ( 1000000 / 115200 + 1 ) * ( 8 + 0 + 1 );

// volatile char serialCommandBuffer[512];
volatile int serialCommandBufferIndex = 0;

int serConfigChangedUSBSer1 = 0;
int serConfigChangedUSBSer2 = 0;

// SerialPIO SerialPIO1(0, 1, 1024);

void initSecondSerial( void ) {
    // Initialize USB CDC interfaces
    // Note: AsyncPassthrough uses pico-sdk UART directly, NOT Arduino Serial1
    // So we don't start Serial1 at all - AsyncPassthrough::begin() handles UART init

#if USB_CDC_ENABLE_COUNT >= 2
    // USBSer1 maps to CDC interface 1 (Arduino Serial passthrough)
    if ( jumperlessConfig.serial_1.function != 0 ) {
        USBSer1.begin( baudRateUSBSer1 );
        // Don't start Arduino Serial1 - AsyncPassthrough uses pico-sdk UART directly
        // This avoids conflicts between Arduino's UART handling and pico-sdk IRQ-based handling
    }
#endif

#if USB_CDC_ENABLE_COUNT >= 3
    // USBSer2 maps to CDC interface 2 (Routable Serial) - conditionally
    // if ( jumperlessConfig.serial_2.function != 0 ) {
    USBSer2.begin( baudRateUSBSer2, makeSerialConfig( 8, 0, 1 ) );
    // Serial.println("  USBSer2 (Routable) initialized");
    // Serial2.begin( baudRateUSBSer2, makeSerialConfig( 8, 0, 0 ) );
    // } else {
    // Serial.println("  USBSer2 disabled by config");
    // }
#endif

#if USB_CDC_ENABLE_COUNT >= 4
    // USBSer3 maps to CDC interface 3 (Debug Serial)

    USBSer3.begin( 115200 );
    // delay(3000);//!son of a bitch
    //  Ser3.println("Serial3 initialized");

    // Serial.println("  USBSer3 (Debug) initialized");
#endif

// Give time for USB enumeration
// delay(200);

// Serial.println("Enabled USB interfaces with dynamic naming:");
// Serial.println("  Interface 0: Jumperless Main (this Serial)");
#define PRINT_SERIAL_INFO_AT_STARTUP 0
#if PRINT_SERIAL_INFO_AT_STARTUP
#if USB_CDC_ENABLE_COUNT >= 2
    Serial.print( "  Interface 1: " );
    const char* func1_name =
        getStringFromTable( jumperlessConfig.serial_1.function, uartFunctionTable );
    if ( func1_name && strcmp( func1_name, "off" ) != 0 &&
         strcmp( func1_name, "disable" ) != 0 ) {
        Serial.print( "JL " );
        // Print with first letter capitalized and underscores as spaces
        char c = func1_name[ 0 ];
        if ( c >= 'a' && c <= 'z' )
            c = c - 'a' + 'A';
        Serial.print( c );
        for ( int i = 1; func1_name[ i ]; i++ ) {
            Serial.print( func1_name[ i ] == '_' ? ' ' : func1_name[ i ] );
        }
        // Serial.println(" (USBSer1)");
    } else {
        // Serial.println("Jumperless Serial 1 (USBSer1)");
    }
#endif
#if USB_CDC_ENABLE_COUNT >= 3
    if ( jumperlessConfig.serial_2.function != 0 ) {
        Serial.print( "  Interface 2: " );
        const char* func2_name = getStringFromTable(
            jumperlessConfig.serial_2.function, uartFunctionTable );
        if ( func2_name && strcmp( func2_name, "off" ) != 0 &&
             strcmp( func2_name, "disable" ) != 0 ) {
            Serial.print( "JL " );
            // Print with first letter capitalized and underscores as spaces
            char c = func2_name[ 0 ];
            if ( c >= 'a' && c <= 'z' )
                c = c - 'a' + 'A';
            Serial.print( c );
            for ( int i = 1; func2_name[ i ]; i++ ) {
                Serial.print( func2_name[ i ] == '_' ? ' ' : func2_name[ i ] );
            }
            // Serial.println(" (USBSer2)");
        } else {
            // Serial.println("Jumperless Serial 2 (USBSer2)");
        }
    }
#endif
#if USB_CDC_ENABLE_COUNT >= 4
    // Serial.println("  Interface 3: Jumperless Debug (USBSer3)");
#endif

#if USB_MSC_ENABLE
    Serial.println( "  MSC: Mass Storage" );
#endif
#if USB_HID_ENABLE_COUNT > 0
    Serial.print( "  HID: " );
    Serial.print( USB_HID_ENABLE_COUNT );
    Serial.println( " interface(s)" );
#endif
#if USB_MIDI_ENABLE
    Serial.println( "  MIDI: MIDI Device" );
#endif
#if USB_VENDOR_ENABLE
    Serial.println( "  Vendor: Custom Interface" );
#endif

// Serial.println("\nNote: Interface names are generated dynamically based on
// your serial function configuration."); Serial.println("serial_1.function = "
// + String(jumperlessConfig.serial_1.function) +
//                ", serial_2.function = " +
//                String(jumperlessConfig.serial_2.function));
#endif
}

// Function to print current USB interface naming (can be called anytime)
void printUSBInterfaceNames( void ) {
    // Serial.println("\n=== Current USB Interface Names ===");
    Serial.println( "Interface 0: Jumperless Main" );

#if USB_CDC_ENABLE_COUNT >= 2
    Serial.print( "Interface 1: " );
    const char* func1_name =
        getStringFromTable( jumperlessConfig.serial_1.function, uartFunctionTable );
    if ( func1_name && strcmp( func1_name, "off" ) != 0 &&
         strcmp( func1_name, "disable" ) != 0 ) {
        Serial.print( "JL " );
        // Print with first letter capitalized and underscores as spaces
        char c = func1_name[ 0 ];
        if ( c >= 'a' && c <= 'z' )
            c = c - 'a' + 'A';
        Serial.print( c );
        for ( int i = 1; func1_name[ i ]; i++ ) {
            Serial.print( func1_name[ i ] == '_' ? ' ' : func1_name[ i ] );
        }
        Serial.println( );
    } else {
        Serial.println( "Jumperless Serial 1" );
    }
#endif

#if USB_CDC_ENABLE_COUNT >= 3
    if ( jumperlessConfig.serial_2.function != 0 ) {
        Serial.print( "Interface 2: " );
        const char* func2_name = getStringFromTable(
            jumperlessConfig.serial_2.function, uartFunctionTable );
        if ( func2_name && strcmp( func2_name, "off" ) != 0 &&
             strcmp( func2_name, "disable" ) != 0 ) {
            Serial.print( "JL " );
            // Print with first letter capitalized and underscores as spaces
            char c = func2_name[ 0 ];
            if ( c >= 'a' && c <= 'z' )
                c = c - 'a' + 'A';
            Serial.print( c );
            for ( int i = 1; func2_name[ i ]; i++ ) {
                Serial.print( func2_name[ i ] == '_' ? ' ' : func2_name[ i ] );
            }
            Serial.println( );
        } else {
            Serial.println( "Jumperless Serial 2" );
        }
    } else {
        Serial.println( "Interface 2: Disabled by config" );
    }
#endif

#if USB_CDC_ENABLE_COUNT >= 4
    Serial.println( "Interface 3: Jumperless TUI" );
#endif

#if USB_MSC_ENABLE
    Serial.println( "MSC: JL Mass Storage" );
#endif

    // Serial.println("=====================================\n");
}

unsigned long serial1LEDTimer = 0;
unsigned long lastSerial1TxRead = 0;
unsigned long lastSerial1RxRead = 0;
unsigned long lastSerial2TxRead = 0;
unsigned long lastSerial2RxRead = 0;
volatile int arduinoInReset = 0;
unsigned long lastTimeResetArduino = 0;

int printSerial1Passthrough = 0;
int printSerial2Passthrough = 0;

int flashArduinoNextLoop = 0;

int arduinoConnected = 0;

unsigned long lastSerial1Check = 5000;
unsigned long lastSerial2Check = 5000;

unsigned long lastSerialPassthrough = millis( );
int serialPassthroughStatus = 0;
int serialPassthroughStatusTimeout = 50;

int secondSerialHandler( void ) {
    // AsyncPassthrough handles all serial bridging via asyncPassthroughService (CRITICAL priority)
    // This function now only handles DTR pulse detection for Arduino flashing

   if( USBSer3.available() > 0 ) {
        char c = USBSer3.read();
     //   USBSer2.write(c);
        USBSer3.println("JL TUI: " + String(c));
        USBSer3.flush();
       
     //   Serial.print(c);
    }


    #if ASYNC_PASSTHROUGH_ENABLED == 1
    // DTR checking happens automatically in AsyncPassthrough::task()
    // Just check if a pulse was detected and handle Arduino reset
    if ( AsyncPassthrough::wasDTRPulseDetected() ) {
        // Flash loop now uses STK500 sniffing + inactivity detection to determine
        // when avrdude is done, instead of a fixed timeout. The timeoutTime parameter
        // is unused in async mode (kept for non-async fallback compatibility).
        flashArduino( 0 );
        AsyncPassthrough::clearDTRPulse();
    }
    // Normal passthrough happens automatically in AsyncPassthrough::task()
    #endif

    return 0;
}

char arduinoCommandStrings[ 10 ][ 50 ] = {
    // commands to sniff from the Arduino
    "jumperlessConfig.serial_1.function",
    "jumperlessConfig.serial_1.connect_on_boot",
    "jumperlessConfig.serial_1.lock_connection",

};



void flashArduino( unsigned long timeoutTime ) {
    // =========================================================================
    // ARDUINO FLASH SEQUENCE
    // All heavy operations happen here (NOT in checkDTRState) so we can
    // service USB between steps and prevent device disconnects.
    // =========================================================================

    ARDUINO_DEBUG_PRINTLN( "Arduino DTR pulse - starting flash sequence" );

    // Step 1: Disable tag parsing so binary flash data isn't parsed as commands
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    if (asyncPassthroughEnabled) {
        AsyncPassthrough::disableTagParsingWithInactivityTimeout(8000, 2000);
    }
    AsyncPassthrough::clearTagParserState();
    #endif

    // checkForConfigChangesUSBSer1(1); // Check for any pending config changes before flashing

    // Service USB between steps
    tud_task();

    // Step 2: Auto-connect UART if not already connected
    arduinoConnected = checkIfArduinoIsConnected( );
    if ( arduinoConnected == 0 && jumperlessConfig.serial_1.autoconnect_flashing == 1 ) {
        Serial.println( "Arduino not connected - connecting UART automatically" );
        connectArduino( 1, 1 );
        arduinoConnected = checkIfArduinoIsConnected( );
    }

    // Service USB after potentially heavy connectArduino
    tud_task();

    // Step 3: Reset the Arduino (or enter ESP32 bootloader)
    // flash_reset_type: 0 = none, 1 = AVR, 2 = ESP32 (B1 then RST sequence)
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    AsyncPassthrough::suspendUARTRxIRQ();
    #endif
    {
        int resetType = jumperlessConfig.serial_1.flash_reset_type;
        if ( resetType == 1 ) {
            // AVR: pulse reset line (e.g. bottom slot = 0)
            unsigned long resetStart = micros();
            SetArduinoResetLine( LOW, 0 );
            while ( micros() - resetStart < 12000 ) {
                tud_task();
                delayMicroseconds( 100 );
            }
            SetArduinoResetLine( HIGH, 0 );
        } else if ( resetType == 2 ) {
            // ESP32 (e.g. Nano ESP32): B1 low then pulse RST
            ESPReset();
        }
        // resetType == 0: no reset
    }

    // Drain any phantom bytes that entered before IRQ was suspended,
    // then re-enable the IRQ for bootloader responses
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    AsyncPassthrough::drainUARTRxBuffers();  // This also re-enables the IRQ
    #endif

    // Service USB after reset
    tud_task();

    // Step 4: Brief delay for bootloader to initialize (~5ms)
    {
        unsigned long bootDelay = micros();
        while ( micros() - bootDelay < 5000 ) {
            tud_task();
            delayMicroseconds( 100 );
        }
    }

    flashingArduino = true;

    // Step 5: Enter flash detection mode — sniffs for STK_LEAVE_PROGMODE
    // and tracks USB→UART data inactivity to detect when avrdude is done.
    // This replaces the fixed timeout with smart completion detection.
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    AsyncPassthrough::enterFlashMode();
    #endif

    // Step 6: Service the passthrough actively during the flash window
    // Bridges USB↔UART data between avrdude and the Arduino bootloader.
    //
    // IMPORTANT: We monitor DTR during the loop and re-reset the Arduino
    // on any new DTR transition. This handles the case where the
    // Jumperless App's port accessibility check (open→close) triggered
    // this flash sequence BEFORE avrdude actually runs. When avrdude
    // later opens the port and does its own DTR toggle, we catch it
    // here and re-reset, so the bootloader starts fresh with correct
    // timing relative to avrdude's first sync byte.
    bool last_dtr_in_loop = USBSer1.dtr();

    // AsyncPassthrough::processPendingLineCoding(); // Ensure any pending line coding changes are applied before flashing
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    // Loop until flash completion is detected (STK500 sniffing / inactivity / hard timeout)
    while ( !AsyncPassthrough::checkFlashDone() ) {
        AsyncPassthrough::task();

        // Check for new DTR transition (avrdude's reset request)
        bool current_dtr = USBSer1.dtr();
        if (current_dtr != last_dtr_in_loop) {
            // Only re-reset if no flash data has been exchanged yet.
            // Before data flows: this DTR is likely the app closing or avrdude opening
            //   → re-reset so bootloader is fresh for avrdude's sync.
            // After data flows: this DTR is avrdude closing the port after flashing
            //   → do NOT re-reset (avrdude is done, nobody will flash again).
            if ( !AsyncPassthrough::hasFlashDataBeenSeen() ) {
                ARDUINO_DEBUG_PRINTLN("Re-reset: DTR transition before flash data (pre-avrdude)");
                AsyncPassthrough::suspendUARTRxIRQ();
                {
                    int resetType = jumperlessConfig.serial_1.flash_reset_type;
                    if ( resetType == 1 ) {
                        unsigned long resetStart = micros();
                        SetArduinoResetLine( LOW, 0 );
                        while ( micros() - resetStart < 12000 ) {
                            tud_task();
                            delayMicroseconds( 100 );
                        }
                        SetArduinoResetLine( HIGH, 0 );
                    } else if ( resetType == 2 ) {
                        ESPReset();
                    }
                }
                AsyncPassthrough::drainUARTRxBuffers();
                tud_task();
                {
                    unsigned long bootDelay = micros();
                    while ( micros() - bootDelay < 5000 ) {
                        tud_task();
                        delayMicroseconds( 100 );
                    }
                }
                // Reset STK byte scanner for new bootloader session, but keep
                // timestamps and data-seen flag so timeouts still work correctly
                AsyncPassthrough::resetFlashSTKDetection();
            }
        }
        last_dtr_in_loop = current_dtr;
    }
    #endif

    // Flash complete — clean up
    #if ASYNC_PASSTHROUGH_ENABLED == 1
    // Release both reset lines so ESP32 B1 is released and board can boot
    if ( jumperlessConfig.serial_1.flash_reset_type == 2 ) {
        SetArduinoResetLine( HIGH, 2 );
    }
    AsyncPassthrough::exitFlashMode();
    // Suppress DTR detection for 2s to prevent port-close from re-triggering
    AsyncPassthrough::setDTRLockout(2000);

    // Post-flash settle: suspend UART RX during Arduino bootloader→app transition.
    // After the last STK500 command, optiboot's watchdog fires (~500ms–1s) causing
    // a hard reset. During reset the ATmega TX line goes tri-state, generating noise
    // that the UART receiver captures as garbage bytes. By suspending the IRQ here
    // and waiting for the Arduino to fully boot into the user app, we avoid
    // forwarding that noise into the ring buffer.
    AsyncPassthrough::suspendUARTRxIRQ();
    {
        unsigned long settleStart = millis();
        while ( millis() - settleStart < 1200 ) {
            tud_task();
        }
    }
    // Drain any noise that entered the HW FIFO before we suspended, then re-enable
    AsyncPassthrough::drainUARTRxBuffers();
    #endif

    flashingArduino = false;
}

char commandStartString[] = "`[";

char commandString[ 256 ];

int USBSer1Available = 0;
int Serial1Available = 0;
int countCheck = 0;

// REMOVED: handleSerialPassthrough() - deprecated, async passthrough is now always used
// Data bridging is handled by AsyncPassthrough::task() via asyncPassthroughService
void resetArduino( int topBottomBoth, unsigned long holdMicroseconds ) {
    SetArduinoResetLine( LOW, topBottomBoth );
    delayMicroseconds( holdMicroseconds );
    SetArduinoResetLine( HIGH, topBottomBoth );
}

void printMicrosPerByte( void ) {
    Serial.println( );
    checkForConfigChangesUSBSer1( 2 );
    Serial.println( "uS per byte    = (1000000 /  baud  + 1)     * (numbits + "
                    "stopbits + paritybits)" );

    Serial.print( "uS per byte    = (1000000 / " );
    Serial.print( baudRateUSBSer1 );
    Serial.print( " + 1 = " );
    Serial.print( 1000000 / baudRateUSBSer1 + 1 );
    Serial.print( ") * (   " );
    Serial.print( numbitsUSBSer1 );
    Serial.print( "     +    " );
    Serial.print( stopbitsUSBSer1 );
    Serial.print( "    +     " );
    Serial.print( paritytypeUSBSer1 == 0 ? 0 : 1 );
    Serial.print( "      = " );
    Serial.print( numbitsUSBSer1 + stopbitsUSBSer1 +
                  ( paritytypeUSBSer1 == 0 ? 0 : 1 ) );
    Serial.print( ")  =  " );
    Serial.println( microsPerByteSerial1 );

    Serial.println( );
    Serial.print( "microsPerByteSerial1: " );
    Serial.println( microsPerByteSerial1 );
    Serial.println( );

    // Serial.print("microsPerByteSerial2: ");
    // Serial.println(microsPerByteSerial2);
}

// void toggleArduinoResetLine(void){
//   pinMode(ARDUINO_RESET_0_PIN, OUTPUT_12MA);
//   pinMode(ARDUINO_RESET_1_PIN, OUTPUT_12MA);
//   digitalWrite(ARDUINO_RESET_0_PIN, HIGH);
//   digitalWrite(ARDUINO_RESET_1_PIN, HIGH);
//   delay(1);
//   digitalWrite(ARDUINO_RESET_0_PIN, LOW);
//   digitalWrite(ARDUINO_RESET_1_PIN, LOW);
// }

void connectArduino( int flashOrLocal, int refreshConnections ) {

    // Use RAM-based state system
    // CRITICAL: Pass autoRefresh=false to avoid triggering refreshLocalConnections()
    // on EACH addBridgeToState call. Without this, 3 sequential refreshes happen
    // (2 auto + 1 explicit), each with zero tud_task() during compute = 30-150ms
    // of USB starvation that causes macOS to disconnect the CDC device.
    addBridgeToState( RP_UART_RX, NANO_D1 , 0, false);
    addBridgeToState( RP_UART_TX, NANO_D0, 0, false);
    // refresh( flashOrLocal, -1, 1, 0 );  // Single combined refresh for both bridges
    refreshLocalConnections(-1, 1);

    // CRITICAL FIX: Reduced timeout and service AsyncPassthrough during wait
    // The original 2-second timeout was blocking AsyncPassthrough::task()
    // which prevented data bridging during Arduino flashing
    unsigned long connectTimeout = millis();
    const unsigned long CONNECT_TIMEOUT_MS = 500;  // Reduced to 500ms
    
    while ( checkIfArduinoIsConnected( ) == 0 ) {
        // CRITICAL: Service AsyncPassthrough to keep data bridging active
        // Without this, avrdude's sync bytes don't reach Arduino during flashing
        #if ASYNC_PASSTHROUGH_ENABLED == 1
        AsyncPassthrough::task();
        #endif
        
        // Also service USB directly
        #ifdef USE_TINYUSB
        extern void tud_task(void);
        tud_task();
        #endif
        
        // Timeout check to prevent infinite loop
        if (millis() - connectTimeout > CONNECT_TIMEOUT_MS) {
            // Timeout - Arduino connection state not detected, but bridges are added
            // Continue anyway - the hardware connection should still work
            break;
        }
        
        delayMicroseconds(50);  // Small yield
    }
}

void disconnectArduino( int flashOrLocal ) {

    // Use RAM-based state system
    removeBridgeFromState( NANO_D1, RP_UART_RX );
    removeBridgeFromState( NANO_D0, RP_UART_TX );
    // State functions already refresh, but do explicit refresh for safety
    refreshLocalConnections(-1, 1);
    
    // if (flashOrLocal == 1) {
    //   refreshLocalConnections(1, 0);
    //   } else {
    //   refreshConnections(1, 0);
    //   }
    
    // refreshBlind(1, 0);
    // sendPaths();
    // waitCore2();
    // sendPaths();
}

int checkIfArduinoIsConnected( void ) {
    

        int connected = globalState.hasConnection( NANO_D1, RP_UART_RX );
    connected += globalState.hasConnection( NANO_D0, RP_UART_TX );
    // Serial.println("connected: " + String(connected));
    if ( connected == 2 ) {
        return 1;
    }
    return 0;
}


int arduinoPresence = 0;

int checkArduinoResetPin0( void ) {
    // Arduino reset pin 0 should be pulled high when Arduino is present
    pinMode( ARDUINO_RESET_0_PIN, INPUT );
    delayMicroseconds( 10 ); // Brief delay for pin to stabilize
    return digitalRead( ARDUINO_RESET_0_PIN );
}

int checkArduinoResetPin1( void ) {
    // Skip if PSRAM mod is installed - GPIO 19 is used for PSRAM CS
    if ( jumperlessConfig.hardware.psram_installed ) {
        return -1; // Return -1 to indicate pin not available
    }
    // Arduino reset pin 1 should be pulled high when Arduino is present
    pinMode( ARDUINO_RESET_1_PIN, INPUT );
    delayMicroseconds( 10 ); // Brief delay for pin to stabilize
    return digitalRead( ARDUINO_RESET_1_PIN );
}

int checkArduinoPresence( void ) {
    // Check if either Arduino (top or bottom slot) is present
    // Returns: 1 if Arduino detected, 0 if not
    int pin0 = checkArduinoResetPin0( );
    int pin1 = checkArduinoResetPin1( ); // Returns -1 if PSRAM mod installed
    
    // Arduino pulls reset line high via internal pullup
    // If both are low, no Arduino is present
    // pin1 == -1 means PSRAM mod installed, only check pin0
    if ( pin0 == HIGH || ( pin1 != -1 && pin1 == HIGH ) ) {
        
        return 1;
    }
    return 0;
}

void autoConnectArduino( void ) {
    if ( checkArduinoPresence( ) == 1 && arduinoPresence == 0 ) {
        arduinoPresence = 1;
        connectArduino( 1, 0 );
    }
}

void SetArduinoResetLine( bool state, int topBottomBoth ) {
    if ( state == LOW ) {
        // Serial.println("Setting Arduino Reset Line to LOW");
        if ( topBottomBoth == 1 || topBottomBoth == 2 ) {
            pinMode( ARDUINO_RESET_0_PIN, OUTPUT_12MA );
            digitalWrite( ARDUINO_RESET_0_PIN, LOW );
            rstColors[ 1 ] = 0x002a10;
        }
        // Skip RESET_1 if PSRAM mod installed - GPIO 19 is used for PSRAM CS
        if ( !jumperlessConfig.hardware.psram_installed && ( topBottomBoth == 0 || topBottomBoth == 2 ) ) {
            pinMode( ARDUINO_RESET_1_PIN, OUTPUT_12MA );
            digitalWrite( ARDUINO_RESET_1_PIN, LOW );
            rstColors[ 0 ] = 0x002a10;
        }

        showLEDsCore2 = 2;

        delayMicroseconds( 1000 );

    } else if ( state == HIGH ) {
        // Serial.println("Setting Arduino Reset Line to HIGH");
        //  digitalWrite(ARDUINO_RESET_0_PIN, HIGH);
        //  digitalWrite(ARDUINO_RESET_1_PIN, HIGH);

        if ( topBottomBoth == 1 || topBottomBoth == 2 ) {
            pinMode( ARDUINO_RESET_0_PIN, INPUT );
        }
        // Skip RESET_1 if PSRAM mod installed - GPIO 19 is used for PSRAM CS
        if ( !jumperlessConfig.hardware.psram_installed && ( topBottomBoth == 0 || topBottomBoth == 2 ) ) {
            pinMode( ARDUINO_RESET_1_PIN, INPUT );
        }
        // headerColors[0] = 0x2000b9;
        // headerColors[1] = 0x0020f9;
        // showLEDsCore2 = 2;
    }
}

// Arduino Nano ESP32 bootloader sequence: B1 (boot strap) to GND first, then
// pulse RST. RESET_1 = B1, RESET_0 = RST. B1 must stay low so chip enters
// download mode on reset; release RST after ~50 ms.
void ESPReset( ) {
    ARDUINO_DEBUG_PRINTLN( "ESP32 bootloader: B1 low, pulse RST" );
    // Skip RESET_1 (B1) if PSRAM mod installed - GPIO 19 is used for PSRAM CS
    if ( !jumperlessConfig.hardware.psram_installed ) {
        pinMode( ARDUINO_RESET_1_PIN, OUTPUT_12MA );
        digitalWrite( ARDUINO_RESET_1_PIN, LOW );  // B1 = GND (boot strap, hold)
        rstColors[ 0 ] = 0x002a10;
    }
    pinMode( ARDUINO_RESET_0_PIN, OUTPUT_12MA );
    digitalWrite( ARDUINO_RESET_0_PIN, LOW );     // RST low
    rstColors[ 1 ] = 0x002a10;
    showLEDsCore2 = 2;
    delay( 50 );   // Hold RST low ~50 ms
    pinMode( ARDUINO_RESET_0_PIN, INPUT );        // Release RST; B1 stays low → download mode
    // RESET_1 (B1) left LOW until flash is done (released in flashArduino cleanup)
}

void applyPsramModeChange( int psramEnabled ) {
    // Called when psram_installed config changes at runtime
    // GPIO 19 is shared between NANO_RESET_1 and PSRAM_CS
    
    if ( psramEnabled ) {
        // PSRAM mode enabled - release GPIO 19 from reset line duty
        // Set to INPUT (high-Z) to avoid interfering with PSRAM chip select
        // The PSRAM hardware will take control of this pin
        //pinMode( ARDUINO_RESET_1_PIN, INPUT );
        gpio_set_function( ARDUINO_RESET_1_PIN, GPIO_FUNC_XIP_CS1 );
        xip_ctrl_hw->ctrl|=XIP_CTRL_WRITABLE_M1_BITS;
        
        Serial.println( "PSRAM mode enabled - GPIO 19 released for PSRAM CS" );
        Serial.println( "Note: Top Arduino slot reset (NANO_RESET_1) is now disabled" );
    } else {
        // PSRAM mode disabled - reconfigure GPIO 19 as reset line
        // Set to INPUT initially (high-Z, reset line pulled high by Arduino)
        pinMode( ARDUINO_RESET_1_PIN, INPUT );
        Serial.println( "PSRAM mode disabled - GPIO 19 restored as NANO_RESET_1" );
        Serial.println( "Note: Top Arduino slot reset is now available" );
    }
}

void setBaudRate( int baudRate ) {}

void arduinoPrint( void ) {}

void uploadArduino( void ) {}

uint16_t makeSerialConfig( uint8_t numbits, uint8_t paritytype,
                           uint8_t stopbits ) {
    uint16_t config = 0;

    //   #define SERIAL_PARITY_EVEN   (0x1ul)
    // #define SERIAL_PARITY_ODD    (0x2ul)
    // #define SERIAL_PARITY_NONE   (0x3ul)
    // #define SERIAL_PARITY_MARK   (0x4ul)
    // #define SERIAL_PARITY_SPACE  (0x5ul)
    // #define SERIAL_PARITY_MASK   (0xFul)

    // #define SERIAL_STOP_BIT_1    (0x10ul)
    // #define SERIAL_STOP_BIT_1_5  (0x20ul)
    // #define SERIAL_STOP_BIT_2    (0x30ul)
    // #define SERIAL_STOP_BIT_MASK (0xF0ul)

    // #define SERIAL_DATA_5        (0x100ul)
    // #define SERIAL_DATA_6        (0x200ul)
    // #define SERIAL_DATA_7        (0x300ul)
    // #define SERIAL_DATA_8        (0x400ul)
    // #define SERIAL_DATA_MASK     (0xF00ul)
    // #define SERIAL_5N1           (SERIAL_STOP_BIT_1 | SERIAL_PARITY_NONE  |
    // SERIAL_DATA_5)

    unsigned long parity = 0x3ul;
    unsigned long stop = 0x10ul;
    unsigned long data = 0x400ul;

    switch ( numbits ) {
    case 5:
        data = 0x100ul;
        break;
    case 6:
        data = 0x200ul;
        break;
    case 7:
        data = 0x300ul;
        break;
    case 8:
        data = 0x400ul;
        break;
    default:
        data = 0x400ul;
        break;
    }

    switch ( paritytype ) {
    case 0:
        parity = 0x3ul;
        break;
    case 2:
        parity = 0x1ul;
        break;
    case 1:
        parity = 0x2ul;
        break;
    case 3:
        parity = 0x3ul;
        break;
    case 4:
        parity = 0x4ul;
        break;
    case 5:
        parity = 0x5ul;
        break;
    default:
        parity = 0x3ul;
        break;
    }

    switch ( stopbits ) {
    case 1:
        stop = 0x10ul;
        break;
    case 2:
        stop = 0x30ul;
        break;
    default:
        stop = 0x10ul;
        // stopbits = 1; // default to 1 stop bit
        break;
    }

    config = data | parity | stop;

    return config;
}

uint16_t getSerial1Config( void ) {

    uint8_t numbits = USBSer1.numbits( );
    uint8_t paritytype = USBSer1.paritytype( );
    uint8_t stopbits = USBSer1.stopbits( );

    return makeSerialConfig( numbits, paritytype, stopbits );
}

uint16_t getSerial2Config( void ) {

    uint8_t numbits = USBSer2.numbits( );
    uint8_t paritytype = USBSer2.paritytype( );
    uint8_t stopbits = USBSer2.stopbits( );

    return makeSerialConfig( numbits, paritytype, stopbits );
}

void checkForConfigChangesUSBSer1( int print ) {

    if ( USBSer1.numbits( ) != numbitsUSBSer1 ) {
        numbitsUSBSer1 = USBSer1.numbits( );
        serConfigChangedUSBSer1 = 1;
    }

    if ( USBSer1.paritytype( ) != paritytypeUSBSer1 ) {
        paritytypeUSBSer1 = USBSer1.paritytype( );
        serConfigChangedUSBSer1 = 1;
    }

    if ( USBSer1.stopbits( ) + 1 != stopbitsUSBSer1 ) {
        stopbitsUSBSer1 = USBSer1.stopbits( ) + 1;
        serConfigChangedUSBSer1 = 1;
    }

    if ( USBSer1.baud( ) != baudRateUSBSer1 ) {
        baudRateUSBSer1 = USBSer1.baud( );
        // microsPerByteSerial1 = (1000000 / baudRateUSBSer1 + 1) * (numbitsUSBSer1
        // + stopbitsUSBSer1 + paritytypeUSBSer1==0?0:1);
        //  USBSer1.begin(baudRate);
        serConfigChangedUSBSer1 = 1;
    }

    if ( serConfigChangedUSBSer1 == 1 && jumperlessConfig.serial_1.function != 0 ) {
        USBSer1.begin(
            baudRateUSBSer1,
            makeSerialConfig( numbitsUSBSer1, paritytypeUSBSer1, stopbitsUSBSer1 ) );
        Serial1.begin(
            baudRateUSBSer1,
            makeSerialConfig( numbitsUSBSer1, paritytypeUSBSer1, stopbitsUSBSer1 ) );
        microsPerByteSerial1 =
            ( 1000000 / baudRateUSBSer1 + 1 ) *
            ( numbitsUSBSer1 + stopbitsUSBSer1 + ( paritytypeUSBSer1 == 0 ? 0 : 1 ) );
        serConfigChangedUSBSer1 = 0;

        if ( print > 0 && millis( ) > 4000 ) {
            if ( print == 1 ) {
                ARDUINO_DEBUG_PRINT( "Serial1 config changed " );
                if ( debugArduino > 1 ) {
                    ARDUINO_DEBUG_PRINT( "to " );
                }
            } else if ( print == 2 ) {
                ARDUINO_DEBUG_PRINT( "Serial1 config = " );
            }

            ARDUINO_DEBUG2_PRINTF( "%d ", baudRateUSBSer1 );
            // ARDUINO_DEBUG_PRINT( " " );

            ARDUINO_DEBUG2_PRINTF( "%d", numbitsUSBSer1 );
            switch ( paritytypeUSBSer1 ) {
            case 0:
                ARDUINO_DEBUG2_PRINT( "N" );
                break;
            case 1:
                ARDUINO_DEBUG2_PRINT( "O" );
                break;
            case 2:
                ARDUINO_DEBUG2_PRINT( "E" );
                break;
            case 3:
                ARDUINO_DEBUG2_PRINT( "M" );
                break;
            case 4:
                ARDUINO_DEBUG2_PRINT( "S" );
                break;
            default:
                ARDUINO_DEBUG2_PRINT( "N" );
                break;
            }

            ARDUINO_DEBUG2_PRINTF( "%d\n\r", stopbitsUSBSer1 );
            ARDUINO_DEBUG_FLUSH( );
        }

        // delay(1);
        // } else if (serConfigChangedUSBSer1 == 1) {
        //   serConfigChangedUSBSer1 = 2;
        //   delay(1);
        //   } else if (serConfigChangedUSBSer1 == 2) {
        //     serConfigChangedUSBSer1 = 3;
        //     delay(1);
    } else if ( print == 2 ) {
        ARDUINO_DEBUG_PRINT( "Serial1 config = " );

        ARDUINO_DEBUG3_PRINTF( "%d ", baudRateUSBSer1 );
        // ARDUINO_DEBUG_PRINT( " " );

        ARDUINO_DEBUG3_PRINTF( "%d", numbitsUSBSer1 );
        switch ( paritytypeUSBSer1 ) {
        case 0:
            ARDUINO_DEBUG3_PRINT( "N" );
            break;
        case 1:
            ARDUINO_DEBUG3_PRINT( "O" );
            break;
        case 2:
            ARDUINO_DEBUG3_PRINT( "E" );
            break;
        case 3:
            ARDUINO_DEBUG3_PRINT( "M" );
            break;
        case 4:
            ARDUINO_DEBUG3_PRINT( "S" );
            break;
        default:
            ARDUINO_DEBUG3_PRINT( "N" );
            break;
        }

        ARDUINO_DEBUG3_PRINTF( "%d\n\r", stopbitsUSBSer1 );
        ARDUINO_DEBUG_FLUSH( );
    }
}

void checkForConfigChangesUSBSer2( int print ) {

    if ( USBSer2.numbits( ) != numbitsUSBSer2 ) {
        numbitsUSBSer2 = USBSer2.numbits( );
        serConfigChangedUSBSer2 = 1;
    }

    if ( USBSer2.paritytype( ) != paritytypeUSBSer2 ) {
        paritytypeUSBSer2 = USBSer2.paritytype( );
        serConfigChangedUSBSer2 = 1;
    }

    if ( USBSer2.stopbits( ) != stopbitsUSBSer2 ) {
        stopbitsUSBSer2 = USBSer2.stopbits( );
        serConfigChangedUSBSer2 = 1;
    }

    if ( USBSer2.baud( ) != baudRateUSBSer2 ) {
        baudRateUSBSer2 = USBSer2.baud( );
        // microsPerByteSerial2 = 1000000 / baudRateUSBSer2 + 1;
        //  USBSer1.begin(baudRate);
        serConfigChangedUSBSer2 = 1;
    }

    if ( serConfigChangedUSBSer2 == 1 && jumperlessConfig.serial_2.function != 0 ) {
        USBSer2.begin(
            baudRateUSBSer2,
            makeSerialConfig( numbitsUSBSer2, paritytypeUSBSer2, stopbitsUSBSer2 ) );
        Serial2.begin(
            baudRateUSBSer2,
            makeSerialConfig( numbitsUSBSer2, paritytypeUSBSer2, stopbitsUSBSer2 ) );
        microsPerByteSerial2 =
            ( 1000000 / baudRateUSBSer2 + 1 ) *
            ( numbitsUSBSer2 + stopbitsUSBSer2 + ( paritytypeUSBSer2 == 0 ? 0 : 1 ) );
        serConfigChangedUSBSer2 = 0;

        if ( print > 0 && millis( ) > 2000 ) {
            ARDUINO_DEBUG_PRINT( "Serial2 config changed " );
            if ( debugArduino > 1 ) {
                ARDUINO_DEBUG_PRINT( "to " );
            }
            ARDUINO_DEBUG2_PRINTF( "%d ", baudRateUSBSer2 );
            // ARDUINO_DEBUG_PRINT( " " );

            ARDUINO_DEBUG2_PRINTF( "%d ", numbitsUSBSer2 );
            switch ( paritytypeUSBSer2 ) {
            case 0:
                ARDUINO_DEBUG2_PRINT( "N" );
                break;
            case 1:
                ARDUINO_DEBUG2_PRINT( "O" );
                break;
            case 2:
                ARDUINO_DEBUG2_PRINT( "E" );
                break;
            case 3:
                ARDUINO_DEBUG2_PRINT( "M" );
                break;
            case 4:
                ARDUINO_DEBUG2_PRINT( "S" );
                break;
            default:
                ARDUINO_DEBUG2_PRINT( "N" );
                break;
            }
            ARDUINO_DEBUG2_PRINTF( "%d\n\r", stopbitsUSBSer2 );
            ARDUINO_DEBUG_FLUSH( );
        }
        /// delay(10);
    } else if ( serConfigChangedUSBSer2 == 1 ) {
        serConfigChangedUSBSer2 = 2;
        /// delay(10);
    } else if ( serConfigChangedUSBSer2 == 2 ) {
        serConfigChangedUSBSer2 = 3;
        /// delay(10);
    }
}

void replyWithSerialInfo( int force ) {

    // if (flashingArduino == true) {
    //   return;
    // }

    // Check main Serial (CDC 0) for ENQ character - responds for ALL ports
    if ( Serial.available( ) > 0 || force == 1 ) {
        char c = Serial.peek( ); // Look at the character without removing it
        
        // Handle DC4 (0x14) for Arduino presence check - fast response
        if ( c == 0x14 ) {       // DC4 (Device Control 4) character
            Serial.read( );      // Remove the DC4 character from buffer
            
            // Quick response for Arduino presence check
            int isConnected = checkIfArduinoIsConnected();
            int isPresent = checkArduinoPresence();
            
            Serial.print(isConnected ? "Y," : "n,");
            Serial.println(isPresent ? "Y" : "n");
            Serial.flush();
            return;
        }
        
        if ( c == 0x05 ) {       // ENQ character
            Serial.read( );      // Remove the ENQ character from buffer

            // Report all enabled serial ports
            Serial.println( "CDC0: Jumperless Main" );

#if USB_CDC_ENABLE_COUNT >= 2
            if ( jumperlessConfig.serial_1.function != 0 ) {
                const char* func1_name = getStringFromTable(
                    jumperlessConfig.serial_1.function, uartFunctionTable );
                if ( func1_name && strcmp( func1_name, "off" ) != 0 &&
                     strcmp( func1_name, "disable" ) != 0 ) {
                    Serial.print( "CDC1: JL " );
                    // Print with first letter capitalized and underscores as spaces
                    char c = func1_name[ 0 ];
                    if ( c >= 'a' && c <= 'z' )
                        c = c - 'a' + 'A';
                    Serial.print( c );
                    for ( int i = 1; func1_name[ i ]; i++ ) {
                        Serial.print( func1_name[ i ] == '_' ? ' ' : func1_name[ i ] );
                    }
                    Serial.println( );
                } else {
                    Serial.println( "CDC1: Jumperless Serial 1" );
                }
            } else {
                Serial.println( "CDC1: Disabled" );
            }
#endif

#if USB_CDC_ENABLE_COUNT >= 3
            if ( jumperlessConfig.serial_2.function != 0 ) {
                const char* func2_name = getStringFromTable(
                    jumperlessConfig.serial_2.function, uartFunctionTable );
                if ( func2_name && strcmp( func2_name, "off" ) != 0 &&
                     strcmp( func2_name, "disable" ) != 0 ) {
                    Serial.print( "CDC2: JL " );
                    // Print with first letter capitalized and underscores as spaces
                    char c = func2_name[ 0 ];
                    if ( c >= 'a' && c <= 'z' )
                        c = c - 'a' + 'A';
                    Serial.print( c );
                    for ( int i = 1; func2_name[ i ]; i++ ) {
                        Serial.print( func2_name[ i ] == '_' ? ' ' : func2_name[ i ] );
                    }
                    Serial.println( );
                } else {
                    Serial.println( "CDC2: Jumperless Serial 2" );
                }
            } else {
                Serial.println( "CDC2: Disabled" );
            }

#endif

#if USB_CDC_ENABLE_COUNT >= 4
            Serial.println( "CDC3: Jumperless TUI" );
#endif
            Serial.flush( );
        }
    }
#if USB_CDC_ENABLE_COUNT >= 2
    // delay(100);
    // Check USBSer1 (CDC 1) for ENQ character - responds only for itself
    if ( USBSer1.available( ) > 0 && flashingArduino == false && millis( ) < 2000 ) {
        char c = USBSer1.peek( ); // Look at the character without removing it
        if ( c == 0x05 ) {        // ENQ character
            USBSer1.read( );      // Remove the ENQ character from buffer

            // Generate dynamic name based on config
            const char* func1_name = getStringFromTable(
                jumperlessConfig.serial_1.function, uartFunctionTable );
            if ( func1_name && strcmp( func1_name, "off" ) != 0 &&
                 strcmp( func1_name, "disable" ) != 0 ) {
                USBSer1.print( "CDC1: JL " );
                // Print with first letter capitalized and underscores as spaces
                char c = func1_name[ 0 ];
                if ( c >= 'a' && c <= 'z' )
                    c = c - 'a' + 'A';
                USBSer1.print( c );
                for ( int i = 1; func1_name[ i ]; i++ ) {
                    USBSer1.print( func1_name[ i ] == '_' ? ' ' : func1_name[ i ] );
                }
                USBSer1.println( );
            } else {
                USBSer1.println( "CDC1: Jumperless Serial 1" );
            }
            USBSer1.flush( );
        }
    }
#endif

#if USB_CDC_ENABLE_COUNT >= 3
    // delay(100);
    // Check USBSer2 (CDC 2) for ENQ character - responds only for itself
    if ( jumperlessConfig.serial_2.function != 0 && USBSer2.available( ) > 0 ) {
        char c = USBSer2.peek( ); // Look at the character without removing it
        if ( c == 0x05 ) {        // ENQ character
            USBSer2.read( );      // Remove the ENQ character from buffer

            // Generate dynamic name based on config
            const char* func2_name = getStringFromTable(
                jumperlessConfig.serial_2.function, uartFunctionTable );
            if ( func2_name && strcmp( func2_name, "off" ) != 0 &&
                 strcmp( func2_name, "disable" ) != 0 ) {
                USBSer2.print( "CDC2: JL " );
                // Print with first letter capitalized and underscores as spaces
                char c = func2_name[ 0 ];
                if ( c >= 'a' && c <= 'z' )
                    c = c - 'a' + 'A';
                USBSer2.print( c );
                for ( int i = 1; func2_name[ i ]; i++ ) {
                    USBSer2.print( func2_name[ i ] == '_' ? ' ' : func2_name[ i ] );
                }
                USBSer2.println( );
            } else {
                USBSer2.println( "CDC2: Jumperless Serial 2" );
            }
            USBSer2.flush( );
        }
    }

#endif
}

int checkForArduinoCommands( uint8_t serialBuffer[], int serialBufferIndex ) {
    for ( int i = 0; i < 10; i++ ) {
        if ( strcasecmp( arduinoCommandStrings[ i ], (const char*)serialBuffer ) == 0 ) {
            Serial.println( "Arduino command received" );
            Serial.println( arduinoCommandStrings[ i ] );
            Serial.println( );
            return i;
        }
    }
    Serial.println( "Arduino command not found" );
    Serial.println( (const char*)serialBuffer );
    Serial.println( );
    Serial.flush( );
    return -1;
}