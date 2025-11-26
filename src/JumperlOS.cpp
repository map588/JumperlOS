// SPDX-License-Identifier: MIT
#include "JumperlOS.h"
#include "PersistentStuff.h"
#include "Probing.h"
#include "Highlighting.h"
#include "Menus.h"
#include "Peripherals.h"
#include "States.h"
#include "Jerial.h" // TermControl is now part of Jerial
#include "oled.h"
#include "USBfs.h"
#include "AsyncPassthrough.h"
#include "SingleCharCommands.h"

#ifdef USE_TINYUSB
#include "tusb.h"
#endif

// External debug flags
extern bool debugWaitLoopTiming;

// Static member initialization
jOSmanager* jOSmanager::core1Instance = nullptr;

// Global references for clean syntax (initialized after singletons)
Probing& probing = Probing::getInstance();
Highlighting& highlighting = Highlighting::getInstance();
Menus& menus = Menus::getInstance();
Peripherals& peripherals = Peripherals::getInstance();
SlotManager& slotManager = SlotManager::getInstance();
jOSmanager& jOS = jOSmanager::getInstance();

// System service references
TermSerialService& termSerialService = TermSerialService::getInstance();
InjectedCommandService& injectedCommandService = InjectedCommandService::getInstance();
AsyncPassthroughService& asyncPassthroughService = AsyncPassthroughService::getInstance();
TinyUSBService& tinyUSBService = TinyUSBService::getInstance();
USBPeriodicService& usbPeriodicService = USBPeriodicService::getInstance();
OLEDService& oledService = OLEDService::getInstance();

/**
 * @brief Construct a jOSmanager for a specific core
 */
jOSmanager::jOSmanager(uint8_t coreId) 
    : serviceCount(0)
    , coreId(coreId)
    , blockingService(nullptr)
    , loopCounter(0)
    , criticalDivisor(1)
    , highDivisor(1)
    , normalDivisor(3)
    , lowDivisor(20)
{
    // Initialize service array
    for (uint8_t i = 0; i < MAX_SERVICES; i++) {
        services[i].service = nullptr;
        services[i].active = false;
    }
}

jOSmanager::~jOSmanager() {
    // Don't delete services - we don't own them
}

/**
 * @brief Get the Core 1 singleton instance
 */
jOSmanager& jOSmanager::getInstance() {
    if (core1Instance == nullptr) {
        core1Instance = new jOSmanager(1);
    }
    return *core1Instance;
}

/**
 * @brief Register a service with this manager
 */
bool jOSmanager::registerService(Service* service) {
    if (service == nullptr) {
        return false;
    }
    
    if (serviceCount >= MAX_SERVICES) {
        return false;
    }
    
    // Check for duplicates
    for (uint8_t i = 0; i < serviceCount; i++) {
        if (services[i].service == service) {
            return false; // Already registered
        }
    }
    
    // Add to array
    services[serviceCount].service = service;
    services[serviceCount].active = true;
    serviceCount++;
    
    // Re-sort by priority
    sortServicesByPriority();
    
    return true;
}

/**
 * @brief Unregister a service
 */
bool jOSmanager::unregisterService(Service* service) {
    if (service == nullptr) {
        return false;
    }
    
    // Find and remove
    for (uint8_t i = 0; i < serviceCount; i++) {
        if (services[i].service == service) {
            // Shift remaining services down
            for (uint8_t j = i; j < serviceCount - 1; j++) {
                services[j] = services[j + 1];
            }
            serviceCount--;
            
            // Clear blocking if this was the blocking service
            if (blockingService == service) {
                blockingService = nullptr;
            }
            
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Execute all registered services in priority order
 * 
 * Priority-based scheduling:
 * - CRITICAL services run every loop (divisor = 1 typically)
 * - HIGH services run every N loops (configurable, default = 1)
 * - NORMAL services run every M loops (configurable, default = 5)
 * - LOW services run every K loops (configurable, default = 20)
 * 
 * This ensures critical services (button input, menus) are ultra-responsive
 * while less time-sensitive services don't block the main loop.
 * 
 * Service ID mapping (as registered in main.cpp):
 *   0 = probeButton (CRITICAL)
 *   1 = menus (CRITICAL)
 *   2 = slotManager (HIGH)
 *   3 = probing (HIGH)
 *   4 = highlighting (HIGH)
 *   5 = peripherals (NORMAL)
 */
void jOSmanager::serviceAll() {
    loopCounter++;
    //debugWaitLoopTiming = true;
    
    // DEBUG: Track current service for crash debugging
    static const char* currentServiceName = nullptr;
    static uint8_t currentServiceIndex = 255;
    
    // DEBUG: Print service index every 5000 calls to track where freeze happens
    static uint32_t serviceAllCounter = 0;
    serviceAllCounter++;
    bool printServiceDebug = false; //(serviceAllCounter % 5000 == 0);
    
    // DEBUG: Mark start of serviceAll
    if (printServiceDebug) {
        Serial.write('{');
        tud_task();
    }
    
    // Debug: Print service execution order every N loops
    static unsigned long lastDebugLoop = 0;
    bool printServiceOrder = 0;//debugWaitLoopTiming && (loopCounter % 100 == 0);
    
    if (printServiceOrder && loopCounter != lastDebugLoop) {
        lastDebugLoop = loopCounter;
        Serial.printf("\n=== Service Execution Order (Loop #%lu) ===\n", loopCounter);
    }
    
    unsigned long loopStart = micros( );
    for (uint8_t i = 0; i < serviceCount; i++) {
        if (!services[i].active) {
            if (printServiceOrder) {
                Serial.printf("  [%d] (inactive)\n", i);
            }
            continue;
        }
        
        Service* svc = services[i].service;
        if (svc == nullptr) {
            if (printServiceOrder) {
                Serial.printf("  [%d] (null service)\n", i);
            }
            continue;
        }
        
        // If we're blocked, only allow CRITICAL priority services to run
        if (blockingService != nullptr && 
            blockingService != svc && 
            svc->getPriority() != ServicePriority::CRITICAL) {
            if (printServiceOrder) {
                Serial.printf("  [%d] %s - SKIPPED (blocked by %s)\n", i, svc->getName(), 
                             blockingService->getName());
            }
            continue;
        }
        
        // Priority-based scheduling - skip services based on their priority and divisor
        ServicePriority priority = svc->getPriority();
        bool shouldRun = false;
        const char* priorityName = "UNKNOWN";
        
        switch (priority) {
            case ServicePriority::CRITICAL:
                shouldRun = (loopCounter % criticalDivisor == 0);
                priorityName = "CRITICAL";
                break;
            case ServicePriority::HIGH:
                shouldRun = (loopCounter % highDivisor == 0);
                priorityName = "HIGH";
                break;
            case ServicePriority::NORMAL:
                shouldRun = (loopCounter % normalDivisor == 0);
                priorityName = "NORMAL";
                break;
            case ServicePriority::LOW:
                shouldRun = (loopCounter % lowDivisor == 0);
                priorityName = "LOW";
                break;
        }
        
        if (!shouldRun) {
            if (printServiceOrder) {
                Serial.printf("  [%d] %s (%s) - SKIPPED (scheduled)\n", i, svc->getName(), priorityName);
            }
            continue;  // Skip this service this iteration
        }
        
        if (printServiceOrder) {
            Serial.printf("  [%d] %s (%s) - RUNNING...\n", i, svc->getName(), priorityName);
        }
        
        // DEBUG: Print which service is about to run
        if (printServiceDebug) {
            Serial.write('[');
            Serial.print(i);
            tud_task();
        }
        
        // Execute the service with timing
        unsigned long svcStart = micros();
        ServiceStatus status = svc->service();
        unsigned long svcEnd = micros();
        unsigned long svcTime = svcEnd - svcStart;
        
        // DEBUG: Print completion marker
        if (printServiceDebug) {
            Serial.write(']');
            tud_task();
        }
        
        // CRITICAL: Report ANY service taking > 100ms (causes command delays!)
        if (debugWaitLoopTiming && svcTime > 100000) {  // > 100ms
            Serial.printf("⏱️  SLOW SERVICE: %s took %lu ms\n", svc->getName(), svcTime / 1000);
            Serial.flush();
        }
        
        if (debugWaitLoopTiming) {
            if (printServiceOrder) {
                Serial.printf("       └─> completed in %lu us (status=%d)\n", svcTime, (int)status);
            } else if (debugWaitLoopTiming && svcTime > 100000) {
                // Debug: Report if any service takes more than 1ms (even when not printing full order)
                Serial.printf("DEBUG:   Service #%d (%s) took %lu us\n", i, svc->getName(), svcTime);
            }
        }
        
        // Update blocking state
        if (status == ServiceStatus::BLOCKING) {
            if (blockingService != svc) {
                // New blocking service
                blockingService = svc;
                if (debugWaitLoopTiming) {
                    Serial.printf("DEBUG:   Service #%d is now BLOCKING\n", i);
                }
            }
        } else if (blockingService == svc) {
            // This service was blocking but is no longer
            blockingService = nullptr;
            if (debugWaitLoopTiming) {
                Serial.printf("DEBUG:   Service #%d released blocking\n", i);
            }
        }
    }
    if (debugWaitLoopTiming && ( micros() - loopStart ) > 20000 ) {
        Serial.printf("DEBUG:   serviceAll() took %lu us (%.2f ms)\n", micros() - loopStart, (micros() - loopStart) / 1000.0);
        Serial.flush();
    }
    
    // DEBUG: Mark end of serviceAll
    if (printServiceDebug) {
        Serial.write('}');
        tud_task();
    }
    //debugWaitLoopTiming = false;
}

/**
 * @brief Execute ONLY CRITICAL priority services
 * 
 * This is used within blocking operations (like probeMode) to keep
 * critical services like button checking alive in their inner loops.
 * 
 * Also keeps current sense measurements updating so marching ants
 * visualization stays current during probe mode.
 */
void jOSmanager::serviceCritical() {
    for (uint8_t i = 0; i < serviceCount; i++) {
        if (!services[i].active) {
            continue;
        }
        
        Service* svc = services[i].service;
        if (svc == nullptr) {
            continue;
        }
        
        // Only execute CRITICAL priority services
        if (svc->getPriority() != ServicePriority::CRITICAL) {
            continue;
        }
        
        // Execute the service (ignore status - we're in a blocking context already)
        svc->service();
    }
    
    // Also keep current sense measurements updating during blocking operations
    // This ensures marching ants visualization stays current in probe mode
    Peripherals::getInstance().pollCurrentSense();
}

/**
 * @brief Sort services by priority using bubble sort
 * Simple algorithm since we have a small number of services
 */
void jOSmanager::sortServicesByPriority() {
    if (serviceCount <= 1) {
        return;
    }
    
    // Bubble sort by priority
    for (uint8_t i = 0; i < serviceCount - 1; i++) {
        for (uint8_t j = 0; j < serviceCount - i - 1; j++) {
            if (services[j].service == nullptr || services[j + 1].service == nullptr) {
                continue;
            }
            
            ServicePriority p1 = services[j].service->getPriority();
            ServicePriority p2 = services[j + 1].service->getPriority();
            
            // Lower enum value = higher priority (CRITICAL=0, LOW=3)
            if (static_cast<int>(p1) > static_cast<int>(p2)) {
                // Swap
                ServiceEntry temp = services[j];
                services[j] = services[j + 1];
                services[j + 1] = temp;
            }
        }
    }
}

/**
 * @brief Set execution divisors for priority-based scheduling
 * 
 * Controls how frequently services at each priority level run.
 * 
 * Example: setExecutionDivisors(1, 2, 10, 50) means:
 * - CRITICAL services run every loop
 * - HIGH services run every 2nd loop
 * - NORMAL services run every 10th loop  
 * - LOW services run every 50th loop
 * 
 * This allows fine-tuning of responsiveness vs. throughput tradeoffs.
 * 
 * @param critical How often to run CRITICAL services (default: 1 = every loop)
 * @param high How often to run HIGH services (default: 1 = every loop)
 * @param normal How often to run NORMAL services (default: 5 = every 5th loop)
 * @param low How often to run LOW services (default: 20 = every 20th loop)
 */
void jOSmanager::setExecutionDivisors(uint8_t critical, uint8_t high, 
                                       uint8_t normal, uint8_t low) {
    criticalDivisor = critical > 0 ? critical : 1;  // Ensure at least 1
    highDivisor = high > 0 ? high : 1;
    normalDivisor = normal > 0 ? normal : 1;
    lowDivisor = low > 0 ? low : 1;
}

// ============================================================================
// System Service Implementations
// ============================================================================

// TermSerialService - Terminal input handling
TermSerialService* TermSerialService::instance = nullptr;

TermSerialService::TermSerialService() : jerialInstance(nullptr) {}

void TermSerialService::setTermControl(JerialClass* jerial) {
    jerialInstance = jerial;
}

TermSerialService& TermSerialService::getInstance() {
    if (instance == nullptr) {
        instance = new TermSerialService();
    }
    return *instance;
}

/**
 * @brief Service method for terminal input
 * CRITICAL priority - user input must be instantly responsive
 * 
 * NOTE: Injected commands are now handled by InjectedCommandService (fast path).
 * This service only handles user-typed input via TermControl line buffering.
 */
ServiceStatus TermSerialService::service() {
    lastStatus = ServiceStatus::IDLE;
    
    if (jerialInstance == nullptr) {
        return lastStatus;
    }
    
    extern struct config jumperlessConfig;
    
    // Only service if line buffering is enabled for user input
    // Injected commands are handled separately by InjectedCommandService (fast path)
    if (jumperlessConfig.display.terminal_line_buffering != 1) {
        return lastStatus;
    }
    
    // Service returns true when line is complete
    if (jerialInstance->service()) {
        lastStatus = ServiceStatus::BUSY;
    }
    
    return lastStatus;
}

// InjectedCommandService - Immediate command execution from injection buffer
InjectedCommandService* InjectedCommandService::instance = nullptr;

InjectedCommandService& InjectedCommandService::getInstance() {
    if (instance == nullptr) {
        instance = new InjectedCommandService();
    }
    return *instance;
}

/**
 * @brief Service method for injected command processing
 * CRITICAL priority - executes commands immediately to prevent buffer pile-up
 * 
 * This service uses a FAST PATH that directly reads from the injection buffer
 * without waiting for slow TermControl processing (which takes 800ms+).
 * 
 * Flow:
 * 1. AsyncPassthrough receives data on SerialPIO
 * 2. Tag parser injects command characters + newline into injection_buffer
 * 3. Sets hasInjectedCommand flag
 * 4. This service (runs every loop as CRITICAL) detects flag
 * 5. DIRECTLY extracts line from injection_buffer (bypasses TermControl)
 * 6. Executes via singleCharCommands.executeCommand()
 * 7. Continues processing if more commands are available
 * 
 * Performance: Commands execute in <1ms instead of 800ms+ via TermControl
 * Thread-safe: Uses atomic buffer position updates
 */
ServiceStatus InjectedCommandService::service() {
    // =========================================================================
    // DISABLED: Commands are now handled synchronously in main.cpp via CommandBuffer
    // 
    // The new architecture uses CommandBuffer for all UART-injected commands:
    // 1. AsyncPassthrough parses <j>/<p> tags and sets pending command in CommandBuffer
    // 2. Main loop checks CommandBuffer::hasPendingCommand() and processes synchronously
    // 3. No competing services, no race conditions, no async complexity
    // 
    // This service is kept for backwards compatibility but does nothing.
    // =========================================================================
    lastStatus = ServiceStatus::IDLE;
    
    // Clear legacy flags that may have been set
    Jerial.hasInjectedCommand = 0;
    
    return lastStatus;
    
    // LEGACY CODE BELOW - KEPT FOR REFERENCE
    #if 0
    // Fast check: is there a complete line in injection buffer?
    // This bypasses slow TermControl and reads directly from buffer
    if (!Jerial.hasInjectedCompleteLine()) {
        // No complete lines, clear flag and return
        Jerial.hasInjectedCommand = 0;
        return lastStatus;
    }
    #endif
}

// AsyncPassthroughService - USB CDC1 <-> UART0 bridging
AsyncPassthroughService* AsyncPassthroughService::instance = nullptr;

AsyncPassthroughService& AsyncPassthroughService::getInstance() {
    if (instance == nullptr) {
        instance = new AsyncPassthroughService();
    }
    return *instance;
}

/**
 * @brief Service method for AsyncPassthrough
 * CRITICAL priority - must run every loop to prevent data loss and maintain low latency
 * 
 * Bridges USB CDC1 (Serial1) <-> UART0 for async passthrough communication.
 * Handles USB->UART and UART->USB data transfer, line coding updates, and
 * command tag detection. Must run continuously to prevent buffer overflows
 * and ensure minimal latency.
 */
ServiceStatus AsyncPassthroughService::service() {
    lastStatus = ServiceStatus::IDLE;
    
    extern bool asyncPassthroughEnabled;
    
    // Only run if async passthrough is enabled
    if (asyncPassthroughEnabled) {
#if ASYNC_PASSTHROUGH_ENABLED == 1
        AsyncPassthrough::task();
        lastStatus = ServiceStatus::BUSY;
#endif
    }
    
    return lastStatus;
}

// TinyUSBService - USB communication
TinyUSBService* TinyUSBService::instance = nullptr;

TinyUSBService& TinyUSBService::getInstance() {
    if (instance == nullptr) {
        instance = new TinyUSBService();
    }
    return *instance;
}

/**
 * @brief Service method for TinyUSB task
 * HIGH priority - USB communication is time-sensitive
 */
ServiceStatus TinyUSBService::service() {
    lastStatus = ServiceStatus::IDLE;
    
#ifdef USE_TINYUSB
    tud_task();
    lastStatus = ServiceStatus::BUSY;
#endif
    
    return lastStatus;
}

// USBPeriodicService - USB mass storage housekeeping
USBPeriodicService* USBPeriodicService::instance = nullptr;

USBPeriodicService& USBPeriodicService::getInstance() {
    if (instance == nullptr) {
        instance = new USBPeriodicService();
    }
    return *instance;
}

/**
 * @brief Service method for USB periodic tasks
 * NORMAL priority - periodic USB maintenance
 * Only runs when MSC mode is enabled
 */
ServiceStatus USBPeriodicService::service() {
    lastStatus = ServiceStatus::IDLE;
    
    extern bool mscModeEnabled;
    
    if (mscModeEnabled) {
        usbPeriodic();
        lastStatus = ServiceStatus::BUSY;
    }
    
    return lastStatus;
}

// OLEDService - OLED display updates
OLEDService* OLEDService::instance = nullptr;

OLEDService& OLEDService::getInstance() {
    if (instance == nullptr) {
        instance = new OLEDService();
    }
    return *instance;
}

/**
 * @brief Service method for OLED periodic updates
 * LOW priority - display updates are not time-critical
 */
ServiceStatus OLEDService::service() {
    lastStatus = ServiceStatus::IDLE;
    
    if (oledDisplay != nullptr) {
        //Serial.println("OLEDService::oledPeriodic");
        oledDisplay->oledPeriodic();
        lastStatus = ServiceStatus::BUSY;
    } else {
        //Serial.println("OLEDService::oledPeriodic oledDisplay is nullptr");
    }
    
    return lastStatus;
}

