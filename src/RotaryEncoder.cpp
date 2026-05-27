#include "RotaryEncoder.h"
#include "externVars.h"  // noteUserInput()
#include "CH446Q.h"
#include "FileParsing.h"
#include "JumperlessDefines.h"
#include "LEDs.h"
#include "MatrixState.h"
#include "NetManager.h"
#include "NetsToChipConnections.h"
#include "Peripherals.h"
#include "Probing.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "quadrature.pio.h"

#include "Commands.h"
#include "Graphics.h"
#include "Menus.h"
#include <cstdint>

volatile int slotChanged = 0;
PIO pioEnc = nullptr; // Will be dynamically assigned

int smEnc = -1; // Will be dynamically assigned
uint offsetEnc = 0;
const uint PIN_AB = 12;

int netSlot = 0;

int newPositionEncoder = 0;
int lastPositionEncoder = 0;
int encoderRaw = 0;
int lastPosition = 0;
int position = 0;
volatile bool resetPosition = false;

int lastButtonState = 0;

volatile int rotaryEncoderMode = 0;

volatile int slotPreview = 0;

// volatile enum { IDLE, UP, DOWN } encoderDirectionState;
// volatile enum { IDLE, PRESSED, HELD, RELEASED } encoderButtonState;

void initRotaryEncoder( void ) {
    pinMode( BUTTON_ENC, INPUT );
    pinMode( QUADRATURE_A_PIN, INPUT_PULLUP );
    pinMode( QUADRATURE_B_PIN, INPUT_PULLUP );

    // CRITICAL: Dynamically claim an unused PIO to avoid conflicts
    // Serial.println("◆ Initializing rotary encoder with dynamic PIO allocation...");

    // Try PIO instances in order: PIO2, PIO0, PIO1 (same priority as logic analyzer)
    PIO pio_instances[] = { pio0, pio1, pio2 };
    bool pio_allocated = false;

    for ( int i = 0; i < 3 && !pio_allocated; i++ ) {
        PIO test_pio = pio_instances[ i ];
        // Serial.printf("◆ Trying PIO%d for rotary encoder...\n", pio_get_index(test_pio));

        // Try to claim a state machine
        int test_sm = pio_claim_unused_sm( test_pio, false );
        if ( test_sm < 0 ) {
            //  Serial.printf("◆ PIO%d: No available state machines\n", pio_get_index(test_pio));
            continue;
        }

        // Check if we can add the program
        if ( !pio_can_add_program( test_pio, &quadrature_encoder_program ) ) {
            // Serial.printf("◆ PIO%d: Cannot add quadrature encoder program\n", pio_get_index(test_pio));
            pio_sm_unclaim( test_pio, test_sm );
            continue;
        }

        // Add the program
        uint test_offset = pio_add_program( test_pio, &quadrature_encoder_program );
        if ( test_offset < 0 ) {
            // Serial.printf("◆ PIO%d: Failed to add quadrature encoder program\n", pio_get_index(test_pio));
            pio_sm_unclaim( test_pio, test_sm );
            continue;
        }

        // Success! Assign the resources
        pioEnc = test_pio;
        smEnc = test_sm;
        offsetEnc = test_offset;

        // Serial.printf("◆ SUCCESS: Rotary encoder allocated PIO%d SM%d offset=%d\n",
        //             pio_get_index(pioEnc), smEnc, offsetEnc);

        pio_allocated = true;
    }

    if ( !pio_allocated ) {
        // Serial.println("◆ ERROR: Failed to allocate PIO resources for rotary encoder");
        // Serial.println("◆ Rotary encoder will not function");
        return;
    }

    // Initialize the quadrature encoder
    quadrature_encoder_program_init( pioEnc, smEnc, PIN_AB, 0 );
    // Serial.println("◆ Rotary encoder initialized successfully");
}

void unInitRotaryEncoder( void ) {
    if ( pioEnc && smEnc != (uint)-1 ) {
        // Serial.printf("◆ Cleaning up rotary encoder resources: PIO%d SM%d\n",
        //               pio_get_index(pioEnc), smEnc);

        // Remove the program and unclaim the state machine
        pio_remove_program( pioEnc, &quadrature_encoder_program, offsetEnc );
        pio_sm_unclaim( pioEnc, smEnc );

        // Reset the variables
        pioEnc = nullptr;
        smEnc = -1;
        offsetEnc = 0;

        // Serial.println("◆ Rotary encoder resources cleaned up");
    }
}

// const char rotaryEncoderHelp[] =
//     "\n\r" // this is for the og jumperless
//     "\t\t  Rotary Encoder Help\n\r"
//     "\t\t  -------------------\n\r"
//     "\n\r"
//     "            A  COM  B                                 \n\r"
//     "    _________________________________________________   \n\r"
//     "  /        D12 D11 D10                                \\ \n\r"
//     " / .        [,][,][,][][][][][][][][][][][][][]        \\  \n\r"
//     "|  ' `0    |  /```\\  |                                  | \n\r"
//     "|  '   `   | (  O  ) |                                  | \n\r"
//     "|  '     ` |  \\___/  |                                  | \n\r"
//     "|  '     '  ['][ ][']{}{}{}{}{}{}{}{}{}[][][][]         | \n\r"
//     "|  '     '  D13  AREF                                   | \n\r"
//     "|            button                                     | \n\r"
//     "\n\r"
//     "Stick a 5 pin rotary encoder into the board as shown above. \n\n\r"
//     "When rotary encoder mode is on, these pins will be connected\n\r"
//     "to the Jumperless [A-UART_Rx(16), B-UART_Tx(17), SW-GPIO_0] \n\n\r"

//     "The LEDs under A0-A7 {shown in curly braces} show which of the \n\r"
//     "8 slots are active/connected(pink) and previewing(blue/green)\n\n\r"
//     "Press the encoder button to made the previewed slot active\n\r"
//     "(going into probing mode will make the previewed slot active)\n\n\r"

//     "You can cycle through slots by entering z(next) or x(previous)\n\r"
//     "or by turning the rotary encoder and then pressing (obviously)\n\n\r"

//     "You can show the contents of all the slot files by entering s\n\r"
//     "(copy/paste the output into a text file on your computer) \n\n\r"
//     "Load files by entering o and paste the text into this terminal \n\n\r"

//     "Wokwi sketches will be loaded into whichever slot is active\n\n\r"

//     "This is a WIP, so let me know if something's broken or you want\n\r"
//     "something added. \n\n\r ";

// void printRotaryEncoderHelp(void) {
//   Serial.print(rotaryEncoderHelp);
//   return;
// }

// Function to check if rotary encoder is properly initialized
bool isRotaryEncoderInitialized( void ) {
    return ( pioEnc != nullptr && smEnc != (uint)-1 );
}

// Function to get rotary encoder status
void printRotaryEncoderStatus( void ) {
    if ( isRotaryEncoderInitialized( ) ) {
        Serial.printf( "◆ Rotary encoder: PIO%d SM%d offset=%d\n",
                       pio_get_index( pioEnc ), smEnc, offsetEnc );
    } else {
        Serial.println( "◆ Rotary encoder: Not initialized" );
    }
}

unsigned long buttonHoldStart = 0;
unsigned long buttonHoldLength = 500;

unsigned long doubleClickTimer = 0;
unsigned long doubleClickLength = 250;

unsigned long buttonDebounceTimer = 0;
unsigned long buttonDebounceTimer2 = 0;
unsigned long debounceTime = 2000;

long positionOffset = 0;

int showingPreview = 0;
int rotState = 0;
int encoderWasPressed = 1;
int encoderIsPressed = 0;
int buttonState = 1; // digitalRead(BUTTON_ENC);

int encoderAstate = 0;
int encoderBstate = 0;
int justPressed = 1;
int lastEncoderBstate = 0;

int probeWasActive = 0;
int encoderReleased = 0;
int printSlotChanges = 0;

int debugEncoder = 0;

int encoderStepsToChangePosition = 2;

int lastRotaryDivider = 8;

/// number of steps to trigger a encoderDirectionState change
int rotaryDivider = 8;

volatile int encoderOverride = 0;

bool resetEncoderPosition = false;
long encoderPositionOffset = 0;
volatile long encoderPosition = 0;

volatile int numberOfSteps = 0;

volatile encoderDirectionStates encoderDirectionState = NONE;
volatile encoderButtonStates encoderButtonState = IDLE;

volatile encoderButtonStates lastButtonEncoderState = IDLE;

volatile encoderDirectionStates lastDirectionState = NONE;
volatile bool encoderDirectionConsumed = true;

// Timer to prevent Core 2 from clearing button events before Core 1 can read them
static unsigned long buttonEventTimestamp = 0;
const unsigned long BUTTON_EVENT_MIN_DURATION_US = 10000; // 10ms minimum hold time - ensures Core 1 catches events even when busy

// ── Hold animation state ──
// Managed by holdAnimationStuff(); drives the white→red LED sweep and
// transitions encoderButtonState from HELD → LONG_HELD when the animation ends.
static unsigned long holdAnimTimer = 0;
static int holdAnimStep = 0;
static bool holdAnimActive = false;
static bool holdAnimLongHeldFlashed = false;

// ── Press animation state ──
// White flash on press, rainbow transition during second half of buttonHoldLength.
static bool pressAnimActive = false;
static unsigned long pressAnimUpdateTimer = 0;
volatile bool buttonPressAnimActive = false;
volatile uint32_t pressAnimLogoColors[ 8 ] = { 0 };

void rotaryEncoderButtonStuff( void ) {
    // CRITICAL: Don't update lastButtonEncoderState while we're holding an event for Core 1
    // This preserves the PRESSED->RELEASED transition that Core 1 checks for
    // Only update when state actually changes or when not in a held event state
    bool isHoldingEventForCore1 = false;
    if ( encoderButtonState == RELEASED || encoderButtonState == DOUBLECLICKED ) {
        // Check if we're still within the minimum hold time
        if ( micros( ) - buttonEventTimestamp < BUTTON_EVENT_MIN_DURATION_US ) {
            isHoldingEventForCore1 = true;
        }
    }

    // Only update lastButtonEncoderState if we're not holding an event
    // EXCEPTION: If Core 1 cleared state to IDLE, always update (event was consumed)
    if ( !isHoldingEventForCore1 || encoderButtonState == IDLE ) {
        lastButtonEncoderState = encoderButtonState;
    }

    buttonState = gpio_get( BUTTON_ENC );
    // Serial.print("buttonState: ");
    // Serial.println(buttonState);

    if ( buttonState == 0 ) {
        encoderIsPressed = 1;
        //     Serial.print("pressed: ");
        // Serial.println(encoderIsPressed);
    } else {
        encoderIsPressed = 0;
        // Serial.print("pressed: ");
        // Serial.println(encoderIsPressed);
    }

    // State machine logic - only transitions on actual changes
    // RELEASED and DOUBLECLICKED states persist until explicitly cleared by consuming code
    // This prevents missing button events in polling loops

    // Multi-core protection: Ensure event states persist long enough for Core 1 to detect
    // Core 2 runs this function much faster than Core 1 can poll, so we need a minimum hold time

    // Only transition to IDLE if we're not in a persistent event state
    if ( encoderIsPressed == 0 && encoderWasPressed == 0 ) {
        // Don't auto-clear RELEASED or DOUBLECLICKED states immediately
        // Wait for minimum duration OR explicit clear by consuming code
        if ( encoderButtonState == RELEASED || encoderButtonState == DOUBLECLICKED ) {
            // Check if enough time has passed since the event was set
            if ( micros( ) - buttonEventTimestamp >= BUTTON_EVENT_MIN_DURATION_US ) {
                // Time expired - now safe to auto-clear if still not consumed
                lastButtonEncoderState = encoderButtonState; // Update before clearing
                encoderButtonState = IDLE;
            }
        } else if ( encoderButtonState != RELEASED && encoderButtonState != DOUBLECLICKED ) {
            // For PRESSED or HELD states, clear immediately when button released
            encoderButtonState = IDLE;
        }
        // lastButtonEncoderState = IDLE;
    }

    if ( encoderIsPressed == 0 && encoderWasPressed == 1 ) {
        // CRITICAL: Set lastButtonEncoderState BEFORE changing to RELEASED
        // This ensures the PRESSED state is captured for Core 1's detection pattern
        lastButtonEncoderState = encoderButtonState; // Should be PRESSED or HELD
        encoderButtonState = RELEASED;
        buttonEventTimestamp = micros( ); // Mark event time for minimum hold duration
        encoderReleased = 1;
        doubleClickTimer = millis( );
        buttonDebounceTimer2 = micros( );

        showLEDsCore2 = 1;
        encoderWasPressed = encoderIsPressed;
    }

    if ( encoderIsPressed == 1 && encoderWasPressed == 1 ) {
        if ( millis( ) - buttonHoldStart > buttonHoldLength && encoderButtonState != MEDIUM_HELD && encoderButtonState != LONG_HELD ) {
            // HELD is set here; LONG_HELD is driven by holdAnimationStuff()
            // when the animation finishes its cycles.
            encoderButtonState = HELD;
            // if (triggerFlash == -1) {
            //     triggerFlash = 1;
            //     flashCountdown = 0;
            // }
        }
    }

    if ( encoderIsPressed == 1 && encoderWasPressed == 0 ) {
        buttonHoldStart = millis( );
        pio_sm_restart( pioEnc, smEnc );
        // encoderRaw = quadrature_encoder_get_count(pioEnc, smEnc);
        // lastPositionEncoder = encoderRaw;

        if ( millis( ) - doubleClickTimer < doubleClickLength ) {
            encoderButtonState = DOUBLECLICKED;
            buttonEventTimestamp = micros( ); // Mark event time for minimum hold duration

        } else {
            encoderButtonState = PRESSED;
        }
        doubleClickTimer = millis( );
        // buttonHoldStart = millis();

        encoderWasPressed = encoderIsPressed;
    }

    lastButtonState = buttonState;
    encoderWasPressed = encoderIsPressed;
}

bool isEncoderButtonPhysicallyPressed( void ) {
    // Button is active LOW - returns true when physically pressed (GPIO reads 0)
    return gpio_get( BUTTON_ENC ) == 0;
}

void holdAnimationStuff( void ) {
    // Hold animation: fill logo LED pairs from bottom → top in rainbow.
    // 3 pairs (GPIO, DAC, ADC) light up in sequence and STAY on per sweep.
    // 2 full sweeps with increasing speed, then LONG_HELD fires.
    // Colours cycle through a rainbow hue based on the overall animation progress.

    static const logoOverrideNames holdLogoPairs[][ 2 ] = {
        { GPIO_0, GPIO_1 },
        { DAC_0, DAC_1 },
        { ADC_0, ADC_1 },
    };
    static const logoOverrideNames holdLogos[] = {
        GPIO_0, GPIO_1,
        DAC_0, DAC_1,
        ADC_0, ADC_1 };
    static const int PAIR_COUNT = 3;
    static const int LOGO_COUNT = 6;
    static const int HOLD_ANIM_CYCLES = 2;            // number of full sweeps before LONG_HELD
    static const unsigned long HOLD_STEP_MS = 450;    // ms per pair step (slow/deliberate)
    static const unsigned long HOLD_STEP_IN_PAD = 50; // ms per pair step (slow/deliberate)

    static const uint32_t PRESS_WHITE = 0x151633; // visible but not blinding

    static int triggerFlash = -1;
    static unsigned long flashDuration = 150;
    static unsigned long flashStartTime = 0;
    static encoderButtonStates lastEncoderButtonState = IDLE;
    static int rebootFlag = -1;

    uint32_t flashColors[ 4 ] = {
        0x101053, // PRESSED
        0x441022, // HELD
        0x663305, // MEDIUM_HELD
        0x990000, // LONG_HELD
    };

    if ( rebootFlag > 0 && encoderIsPressed == 1) {
        // if ( encoderButtonState != IDLE) {
            rebootFlag++;
            if ( rebootFlag >= 100 ) {
                rebootFlag = -1;
                triggerFlash = -1;
                rp2040.reboot( );
            }
            triggerFlash = 3;

        // } else {
            // rebootFlag = -1;
        // }
    } else {
        rebootFlag = -1;
    }
    // Trigger flash whenever encoderButtonState changes (map state → flash color index)
    if ( encoderButtonState != lastEncoderButtonState ) {
        lastEncoderButtonState = encoderButtonState;
        if ( encoderButtonState == PRESSED ) {
            triggerFlash = 0;
        } else if ( encoderButtonState == HELD ) {
            triggerFlash = 1;
        } else if ( encoderButtonState == MEDIUM_HELD ) {
            triggerFlash = 2;
        } else if ( encoderButtonState == LONG_HELD ) {
            triggerFlash = 3;
        } else {
            triggerFlash = -1; // IDLE, RELEASED, DOUBLECLICKED — no flash
        }
        if ( triggerFlash >= 0 ) {
            flashStartTime = millis( );
        }
    }

    // ── Press animation: white flash → rainbow transition ──
    // Activates on PRESSED, runs until HELD or release.

    if ( encoderButtonState == PRESSED && !pressAnimActive ) {
        // Immediate white flash on press
        // Write colors BEFORE setting the cross-core flag so logoSwirl()
        // on Core 1 never sees the flag with stale/zero color data.
        pressAnimActive = true;
        for ( int i = 0; i < 8; i++ ) {
            pressAnimLogoColors[ i ] = flashColors[ 0 ];
        }
        pressAnimUpdateTimer = millis( );
        buttonPressAnimActive = true; // set AFTER colors are ready
        showLEDsCore2 = 2;
    }
    unsigned long elapsed = millis( ) - buttonHoldStart;

    if ( pressAnimActive || holdAnimActive ) {

        // first press, hold trigger, medium hold, long hold

        // for ( int i = 0; i < 4; i++ ) {
        //     if ( millis( ) - buttonHoldStart >= flashCountdown && millis( ) - buttonHoldStart <= flashCountdown + flashDuration ) {
        //         // setLogoOverride( LOGO_TOP, flashColors[ i ] );
        //         // setLogoOverride( LOGO_BOTTOM, flashColors[ i ] );
        //         // showLEDsCore2 = 2;
        //         inFlashRange = i;
        //         break;
        //     }
        // }

        // Throttle updates to ~20ms
        // Skip rainbow overwrite while state-change flash is active so flash colors aren't clobbered
        bool flashActive = ( triggerFlash >= 0 && ( millis( ) - flashStartTime < flashDuration ) );
        if ( millis( ) - pressAnimUpdateTimer >= 10 && rebootFlag == -1 ) {
            pressAnimUpdateTimer = millis( );

            unsigned long halfPoint = 1;
            // Serial.print("elapsed: ");
            // Serial.println(elapsed);

            // Serial.flush();

            if ( elapsed >= halfPoint && !flashActive ) {
                // Second half: transition outer LEDs from white to rainbow
                float progress = 1.0f - (float)( elapsed - ( halfPoint ) ) / (float)( HOLD_ANIM_CYCLES * ( HOLD_STEP_MS + HOLD_STEP_IN_PAD ) * LOGO_COUNT );
                // Serial.println(progress);
                if ( progress > 1.0f ) {
                    progress -= 1.0f;
                } else if ( progress < 0.0 ) {
                    progress += 1.0f;
                }

                uint8_t sat = 255 - (uint8_t)( progress * 90.0f ); // 0 (white) → 230 (vivid)

                for ( int i = 0; i < 6; i++ ) {
                    uint8_t hue = ( (uint8_t)( ( i * 255 ) / 6 ) + (uint8_t)( progress * 2000.0f ) ) % 255; // evenly spaced rainbow

                    uint8_t val = (uint8_t)( ( 1.0f - progress ) * 80.0 ) + 35;

                    hsvColor hsv = { hue, sat, val };

                    pressAnimLogoColors[ i ] = HsvToRaw( hsv );
                }
                // Center LEDs stay white
                // pressAnimLogoColors[ 6 ] = PRESS_WHITE;
                pressAnimLogoColors[ 7 ] = PRESS_WHITE;
                if ( lastButtonEncoderState == encoderButtonState ) {
                    showLEDsCore2 = 2;
                }
            }
        }
    }

    // Cancel press animation on double-click
    if ( pressAnimActive && encoderButtonState == DOUBLECLICKED ) {
        pressAnimActive = false;
        buttonPressAnimActive = false;
        showLEDsCore2 = 2;
    }

    // ── Start animation on entering HELD ──
    if ( ( encoderButtonState == HELD || encoderButtonState == MEDIUM_HELD ) && !holdAnimActive ) {
        // Keep press animation (logo rainbow) running — it stays visible
        // while the connector LED hold animation plays alongside it.

        holdAnimStep = 0;
        holdAnimTimer = millis( );
        holdAnimActive = true;
        holdAnimLongHeldFlashed = false;
        for ( int p = 0; p < PAIR_COUNT; p++ ) {
            setLogoOverride( holdLogoPairs[ p ][ 0 ], -3 );
            setLogoOverride( holdLogoPairs[ p ][ 1 ], -3 );
        }
    }

    // ── Step animation while HELD ──
    if ( ( encoderButtonState == HELD || encoderButtonState == MEDIUM_HELD ) && holdAnimActive ) {
        unsigned long stepDuration = holdAnimStep % 2 == 0 ? HOLD_STEP_MS : HOLD_STEP_IN_PAD; // Alternate between main step and pad
        if ( millis( ) - holdAnimTimer >= stepDuration ) {
            holdAnimTimer = millis( );

            int cycleNum = holdAnimStep / LOGO_COUNT; // which sweep (0‥N-1)
            int pairIdx = holdAnimStep % LOGO_COUNT;  // which pair  (0‥2)

            if ( cycleNum < HOLD_ANIM_CYCLES ) {
                // Rainbow: hue progresses through 0°→360° across all steps
                int totalSteps = HOLD_ANIM_CYCLES * LOGO_COUNT;
                uint8_t hue = ( 255 - ( (int)( (float)holdAnimStep / (float)( totalSteps - 4 ) * 255.0f ) ) ) % 256; // 0‥255

                // // HSV→RGB (S=1, V=1) — full saturation, full brightness
                // float h60 = hue / 60.0f;
                // int   sector = (int)h60 % 6;
                // float frac = h60 - (int)h60;
                // uint8_t q = (uint8_t)(255 * (1.0f - frac));
                // uint8_t t = (uint8_t)(255 * frac);
                // uint8_t r, g, bv;
                // switch (sector) {
                //   case 0: r = 255; g = t;   bv = 0;   break;
                //   case 1: r = q;   g = 255; bv = 0;   break;
                //   case 2: r = 0;   g = 255; bv = t;   break;
                //   case 3: r = 0;   g = q;   bv = 255; break;
                //   case 4: r = t;   g = 0;   bv = 255; break;
                //   default:r = 255; g = 0;   bv = q;   break;
                // }
                // uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bv;
                hsvColor hsv = { hue, 180, 190 };
                uint32_t color = HsvToRaw( hsv );
                // At the start of each new sweep, clear all pairs to begin fresh fill
                if ( pairIdx == 0 ) {
                    for ( int p = 0; p < PAIR_COUNT; p++ ) {
                        setLogoOverride( holdLogoPairs[ p ][ 0 ], -3 );
                        setLogoOverride( holdLogoPairs[ p ][ 1 ], -3 );
                    }
                }

                // Light this pair (it stays on until next sweep clears)
                setLogoOverride( holdLogos[ pairIdx ], color );
                // setLogoOverride(holdLogoPairs[pairIdx][0], color);
                // setLogoOverride(holdLogoPairs[pairIdx][1], color);
                showLEDsCore2 = 2;

                holdAnimStep++;
                // After first 3 logo pairs (one full sweep of 6 steps) are lit, fire MEDIUM_HELD and flash
                if ( holdAnimStep == LOGO_COUNT ) {
                    encoderButtonState = MEDIUM_HELD;
                    lastEncoderButtonState = MEDIUM_HELD;
                    triggerFlash = 2;
                    flashStartTime = millis( );
                }
                // After second full sweep (12 steps), fire LONG_HELD and flash
                if ( holdAnimStep == HOLD_ANIM_CYCLES * LOGO_COUNT ) {
                    encoderButtonState = LONG_HELD;
                    lastEncoderButtonState = LONG_HELD;
                    triggerFlash = 3;
                    flashStartTime = millis( );
                    buttonEventTimestamp = micros( );
                }
            } else {
                // Animation finished → fire LONG_HELD
            }
        }
    }

    // ── Flash all red on LONG_HELD (once) ──
    if ( encoderButtonState == LONG_HELD && holdAnimActive && !holdAnimLongHeldFlashed ) {
        for ( int p = 0; p < PAIR_COUNT; p++ ) {
            setLogoOverride( holdLogoPairs[ p ][ 0 ], 0xaaaaaa );
            setLogoOverride( holdLogoPairs[ p ][ 1 ], 0xaaaaaa );
        }

        for ( int i = 0; i < 6; i++ ) {
            pressAnimLogoColors[ i ] = flashColors[ triggerFlash ];
        }
        pressAnimLogoColors[ 7 ] = PRESS_WHITE;

        showLEDsCore2 = 2;
        holdAnimLongHeldFlashed = true;
        if ( rebootFlag == -1 ) {
            rebootFlag = 1;
        }
        // rp2040.reboot( );
    }

    // ── Clean up when button is released ──
    if ( holdAnimActive && encoderIsPressed == 0 ) {
        for ( int p = 0; p < PAIR_COUNT; p++ ) {
            setLogoOverride( holdLogoPairs[ p ][ 0 ], -3 );
            setLogoOverride( holdLogoPairs[ p ][ 1 ], -3 );
        }
        showLEDsCore2 = 2;
        holdAnimActive = false;
        holdAnimLongHeldFlashed = false;
        holdAnimStep = 0;
        // Also clear the logo rainbow that was kept alive during HELD
        buttonPressAnimActive = false;
        pressAnimActive = false;
    }

    // Clean up press animation on actual release (RELEASED, IDLE, DOUBLECLICKED).
    // Don't clear during HELD/LONG_HELD — the logo rainbow stays visible there.
    if ( pressAnimActive && encoderButtonState != PRESSED && encoderButtonState != HELD && encoderButtonState != MEDIUM_HELD && encoderButtonState != LONG_HELD ) {
        buttonPressAnimActive = false;
        pressAnimActive = false;
        rebootFlag = -1;
    }

    // ── Show state-change flash last: set logo colors then showLEDsCore2 so nothing overwrites before Core 1 reads ──
    if ( triggerFlash >= 0 ) {
        for ( int i = 0; i < 6; i++ ) {
            pressAnimLogoColors[ i ] = flashColors[ triggerFlash ];
        }
        pressAnimLogoColors[ 7 ] = PRESS_WHITE;
        if ( millis( ) - flashStartTime >= flashDuration ) {
            triggerFlash = -1;
        }
        showLEDsCore2 = 2;
    }
}

void rotaryEncoderStuff( void ) {

    if ( false ) {
        return;
    }

    if ( encoderOverride > 0 ) {
        encoderOverride--;
        // Serial.print("encoderOverride: ");
        // Serial.println(encoderOverride);
        return;
    }

    // Handle button state checking and updates
    rotaryEncoderButtonStuff( );

    // Drive the hold animation + LONG_HELD transition
    holdAnimationStuff( );

    if ( lastRotaryDivider != rotaryDivider ) {
        pio_sm_restart( pioEnc, smEnc );
        lastRotaryDivider = rotaryDivider;
        encoderRaw = quadrature_encoder_get_count( pioEnc, smEnc );
        // encoderRaw -= positionOffset;
        encoderRaw = encoderRaw / rotaryDivider;
        lastPositionEncoder = encoderRaw;

        // quadrature_program_init(pioEnc, smEnc, offsetEnc, QUADRATURE_A_PIN,
        // QUADRATURE_B_PIN);
    }
    // if (resetPosition == true) {
    //  // quadrature_encoder_program_init(pioEnc, smEnc, PIN_AB, 0);
    //   //pio_sm_restart(pioEnc, smEnc);
    //   //pio_sm_clear_fifos(pioEnc, smEnc);
    //   //pio_sm_drain_tx_fifo(pioEnc, smEnc);

    //   positionOffset = quadrature_encoder_get_count(pioEnc, smEnc);
    //   positionOffset = positionOffset / rotaryDivider;
    //   Serial.print("\n\n\rencoderRaw: ");
    //   Serial.println(encoderRaw);
    //   Serial.print("positionOffset: ");
    //   Serial.println(positionOffset);

    //   // encoderRaw -= positionOffset;
    //   // encoderRaw = encoderRaw / rotaryDivider;
    //   //lastPositionEncoder = positionOffset/rotaryDivider;
    //   resetPosition = false;
    // }

    encoderRaw = quadrature_encoder_get_count( pioEnc, smEnc );

    encoderPosition = quadrature_encoder_get_count( pioEnc, smEnc ) - encoderPositionOffset;
    if ( resetEncoderPosition == true ) {
        encoderPositionOffset = quadrature_encoder_get_count( pioEnc, smEnc );
        resetEncoderPosition = false;
    }

    encoderRaw = encoderRaw / rotaryDivider;
    // encoderRaw -= positionOffset;
    numberOfSteps = abs( lastPositionEncoder - encoderRaw );

    if ( ( lastPositionEncoder - encoderRaw > 1 || lastPositionEncoder - encoderRaw < -1 ) || ( lastPositionEncoder != encoderRaw && rotaryDivider < 4 ) ) {

        if ( lastPositionEncoder > encoderRaw && encoderDirectionState != DOWN ) {
            position++;
            encoderDirectionState = UP;
            encoderDirectionConsumed = false; // Mark as unconsumed
            // numberOfSteps = abs(lastPositionEncoder - encoderRaw);
            numberOfSteps = abs( lastPositionEncoder - encoderRaw );
            noteUserInput( );  // gate background flash writes - user is turning

            lastPositionEncoder = encoderRaw;

        } else if ( lastPositionEncoder < encoderRaw &&
                    encoderDirectionState != UP ) {
            position--;
            encoderDirectionState = DOWN;
            encoderDirectionConsumed = false; // Mark as unconsumed
            numberOfSteps = lastPositionEncoder - encoderRaw;
            noteUserInput( );
            lastPositionEncoder = encoderRaw;

        } else if ( encoderDirectionConsumed ) {
            // Only clear to NONE if already consumed
            encoderDirectionState = NONE;
        }

        //}

    } else if ( encoderDirectionConsumed ) {
        // Only clear to NONE if already consumed
        encoderDirectionState = NONE;
    }

    // slotManager();

    // buttonState = digitalRead(BUTTON_ENC);
    //  if (millis() - buttonHoldStart > buttonHoldLength && buttonState == 0 )
    //  {
    //      //Serial.println("held\n\r");
    //      //refreshSavedColors();

    //     slotChanged = 1;
    //     netSlot = slotPreview;
    // }

    if ( debugEncoder == 1 ) {
        if ( encoderButtonState != lastButtonEncoderState ) {
            switch ( encoderButtonState ) {
            case IDLE:
                // Serial.print("IDLE");
                break;
            case PRESSED:
                Serial.print( lastButtonEncoderState );
                Serial.println( " PRESSED" );
                // delay(150);
                break;
            case HELD:

                Serial.println( "HELD" );
                // delay(150);
                break;
            case LONG_HELD:

                Serial.println( "LONG_HELD" );
                // delay(150);
                break;
            case RELEASED:

                Serial.println( "RELEASED" );
                // delay(150);
                break;

            case DOUBLECLICKED:
                Serial.println( "DOUBLECLICKED" );
                /// delay(150);
                break;

            default:
                break;
            }
        }
        if ( encoderDirectionState != NONE || encoderButtonState != IDLE ) {
            switch ( encoderDirectionState ) {
            case NONE:

                // Serial.print("NONE");
                break;
            case UP:
                Serial.print( position );
                Serial.println( "  UP" );

                // delay(150);
                break;
            case DOWN:
                Serial.print( position );
                Serial.println( "  DOWN" );
                // delay(150);
                break;
            default:
                break;
            }
        }
    }
}
