#ifndef ROTARYENCODER_H
#define ROTARYENCODER_H

#include <Arduino.h>  // For millis() and abs()

#define NUM_SLOTS 8

extern volatile int rotaryEncoderMode;
extern int netSlot;
extern volatile int slotChanged;

extern volatile int slotPreview;
extern int rotState;
extern int encoderIsPressed;
extern int showingPreview;

extern int rotaryDivider;
extern int encoderRaw;
extern volatile int numberOfSteps;
extern volatile bool resetPosition;

extern volatile int encoderOverride;

extern volatile long encoderPosition;
extern long encoderPositionOffset;
extern bool resetEncoderPosition;
enum encoderDirectionStates { NONE,UP,DOWN };

enum encoderButtonStates { IDLE, PRESSED, HELD, RELEASED, DOUBLECLICKED};

extern volatile encoderDirectionStates encoderDirectionState;
extern volatile encoderButtonStates encoderButtonState;
extern volatile encoderButtonStates lastButtonEncoderState;
extern volatile encoderDirectionStates lastDirectionState;

void initRotaryEncoder(void);
void unInitRotaryEncoder(void);
void printRotaryEncoderHelp(void);
void rotaryEncoderStuff(void);
bool isRotaryEncoderInitialized(void);
void printRotaryEncoderStatus(void);

/**
 * @brief Helper class for rotary encoder acceleration
 * 
 * Provides smooth acceleration for fast scrolling while maintaining precision
 * at slow speeds. Works for both integer and character selection.
 * 
 * Configurable parameters allow tuning the acceleration feel for different use cases.
 */
class EncoderAccelerator {
private:
    float accelerationMultiplier; // Current acceleration multiplier
    int lastDirection = 0;  // -1=down, 0=none, 1=up
    int consecutiveFastCount = 0;
    unsigned long lastChangeTime = 0;
    
    // Configurable acceleration parameters
    float baseSpeed;        // Starting multiplier (e.g., 0.1 for very slow)
    float maxSpeed;         // Maximum multiplier (e.g., 50.0 for fast)
    float rampRate;         // How fast to accelerate (e.g., 12.0)
    float decayRate;        // Decay when slowing down (e.g., 0.9)
    int fastThreshold;      // Delta magnitude for "fast" rotation (e.g., 4)
    int timeoutMs;          // Reset timeout in milliseconds (e.g., 120)
    
public:
    /**
     * @brief Construct accelerator with custom parameters
     * 
     * @param baseSpeed Starting multiplier (default 0.1 = very slow/precise)
     * @param maxSpeed Maximum multiplier (default 50.0 = very fast)
     * @param rampRate Acceleration rate (default 12.0 = aggressive)
     * @param decayRate Deceleration rate (default 0.9 = slow decay)
     * @param fastThreshold Delta for "fast" rotation (default 4)
     * @param timeoutMs Reset timeout in ms (default 120)
     */
    EncoderAccelerator(
        float baseSpeed = 0.1f,
        float maxSpeed = 50.0f,
        float rampRate = 12.0f,
        float decayRate = 0.9f,
        int fastThreshold = 4,
        int timeoutMs = 120
    ) : baseSpeed(baseSpeed),
        maxSpeed(maxSpeed),
        rampRate(rampRate),
        decayRate(decayRate),
        fastThreshold(fastThreshold),
        timeoutMs(timeoutMs)
    {
        reset();
    }
    
    void reset() {
        accelerationMultiplier = baseSpeed;
        lastDirection = 0;
        consecutiveFastCount = 0;
        lastChangeTime = millis();
    }
    
    /**
     * @brief Calculate accelerated delta value
     * 
     * @param encoderDelta Raw encoder change
     * @return Accelerated delta (fractional for smooth movement)
     */
    float getAcceleratedDelta(long encoderDelta) {
        if (encoderDelta == 0) {
            return 0.0f;
        }
        
        // Determine current direction
        int currentDirection = (encoderDelta > 0) ? 1 : ((encoderDelta < 0) ? -1 : 0);
        
        // Reset acceleration if direction changed
        if (currentDirection != 0 && currentDirection != lastDirection) {
            accelerationMultiplier = baseSpeed;
            consecutiveFastCount = 0;
            lastDirection = currentDirection;
        }
        
        // Calculate acceleration based on delta magnitude and timing
        unsigned long currentTime = millis();
        unsigned long timeSinceLastChange = currentTime - lastChangeTime;
        int deltaMagnitude = abs(encoderDelta);
        
        // Fast rotation = large delta between polls
        bool isFastRotation = (deltaMagnitude >= fastThreshold);
        
        if (isFastRotation) {
            consecutiveFastCount++;
            
            // Accelerate aggressively for fast movements
            if (consecutiveFastCount >= 0) {
                accelerationMultiplier += rampRate;
                if (accelerationMultiplier > maxSpeed) {
                    accelerationMultiplier = maxSpeed;
                }
            }
        } else if (timeSinceLastChange > timeoutMs) {
            // Slow/stopped - reset to base speed
            accelerationMultiplier = baseSpeed;
            consecutiveFastCount = 0;
            lastDirection = 0;
        } else {
            // Medium speed - maintain fractional acceleration
            consecutiveFastCount = 0;
            if (accelerationMultiplier > 1.0f) {
                accelerationMultiplier *= decayRate;
            }
        }
        
        lastChangeTime = currentTime;
        
        // Calculate fractional change - allows sub-unit movements
        return encoderDelta * accelerationMultiplier;
    }
    
    /**
     * @brief Get current acceleration multiplier (for debugging/display)
     */
    float getMultiplier() const {
        return accelerationMultiplier;
    }
    
    // Preset configurations for common use cases
    
    /**
     * @brief Fast preset - for large ranges (e.g., 0-2048)
     * Very aggressive acceleration for quick scrolling
     */
    static EncoderAccelerator Fast() {
        return EncoderAccelerator(0.1f, 50.0f, 12.0f, 0.9f, 4, 120);
    }
    
    /**
     * @brief Medium preset - for moderate ranges or character selection
     * Balanced acceleration with good precision
     */
    static EncoderAccelerator Medium() {
        return EncoderAccelerator(0.1f, 0.3f, 0.1f, 10.85f, 4, 50);
    }
    
    /**
     * @brief Slow preset - for precise selection (e.g., 0-100)
     * Gentle acceleration with maximum precision
     */
    static EncoderAccelerator Slow() {
        return EncoderAccelerator(0.1f, 0.4f, 0.1f, 10.8f, 5, 170);
    }

    /**
     * @brief Subtle preset - for subtle acceleration for menu selection
     * 
     */
    static EncoderAccelerator Subtle() {
        return EncoderAccelerator(0.01f, 1.0f, 0.3f, 10.8f, 6, 170);
    }   
};









#endif