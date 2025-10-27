// SPDX-License-Identifier: MIT
#ifndef JUMPERLOS_H
#define JUMPERLOS_H

#include <Arduino.h>
#include "Jerial.h"

// Forward declarations
class Service;
class jOSmanager;
class Probing;
class Highlighting;
class Menus;
class Peripherals;
class SlotManager;

/**
 * @brief Status of a service after its service() method executes
 */
enum class ServiceStatus {
    IDLE,      // Service has nothing to do this cycle
    BUSY,      // Service is actively working but non-blocking
    BLOCKING,  // Service needs exclusive control (blocks lower priority services)
    ERROR      // Service encountered an error
};

/**
 * @brief Priority level for service execution ordering
 * Higher priority services run first in each cycle
 */
enum class ServicePriority {
    CRITICAL = 0,  // Highest priority - always runs (e.g., menus, user input)
    HIGH = 1,      // High priority - time-sensitive operations (e.g., probing, highlighting)
    NORMAL = 2,    // Normal priority - periodic tasks (e.g., measurements)
    LOW = 3        // Lowest priority - background tasks
};

/**
 * @brief Base interface for all services in JumperlOS
 * 
 * Services are modular subsystems that can be registered with a ServiceManager
 * and executed in priority order. They report their status after each execution.
 * 
 * Design is core-agnostic - services can run on Core 0 or Core 1
 */
class Service {
public:
    virtual ~Service() {}
    
    /**
     * @brief Main service execution method - called each loop iteration
     * @return ServiceStatus indicating current state
     */
    virtual ServiceStatus service() = 0;
    
    /**
     * @brief Get the service name for debugging/logging
     */
    virtual const char* getName() const = 0;
    
    /**
     * @brief Get the service priority
     */
    virtual ServicePriority getPriority() const = 0;
    
    /**
     * @brief Check if this service is currently active/busy
     */
    virtual bool isActive() const {
        return lastStatus == ServiceStatus::BUSY || 
               lastStatus == ServiceStatus::BLOCKING;
    }
    
    /**
     * @brief Get the last status returned by service()
     */
    ServiceStatus getLastStatus() const { return lastStatus; }

protected:
    ServiceStatus lastStatus = ServiceStatus::IDLE;
};

/**
 * @brief Manages and coordinates execution of all registered services
 * 
 * Instance-based design allows multiple jOSmanagers (e.g., Core 1 and Core 2)
 * Executes services in priority order, handling blocking scenarios
 */
class jOSmanager {
public:
    jOSmanager(uint8_t coreId = 0);
    ~jOSmanager();
    
    /**
     * @brief Get the Core 1 (main) jOSmanager instance
     * This is a convenience singleton for the primary core
     */
    static jOSmanager& getInstance();
    
    /**
     * @brief Register a service with this manager
     * @param service Pointer to service (must remain valid)
     * @param priority Priority level for execution ordering
     * @return true if registered successfully
     */
    bool registerService(Service* service);
    
    /**
     * @brief Unregister a service
     */
    bool unregisterService(Service* service);
    
    /**
     * @brief Execute all registered services in priority order
     * 
     * If a service returns BLOCKING, only CRITICAL priority services
     * will continue to run until the blocking service returns to non-blocking state
     */
    void serviceAll();
    
    /**
     * @brief Execute ONLY CRITICAL priority services
     * 
     * This is for use within blocking operations (like probeMode) that need
     * to keep critical services like button checking running in their inner loops
     */
    void serviceCritical();
    
    /**
     * @brief Get the currently blocking service (if any)
     * @return Pointer to blocking service, or nullptr if none
     */
    Service* getBlockingService() const { return blockingService; }
    
    /**
     * @brief Check if any service is currently blocking
     */
    bool isBlocked() const { return blockingService != nullptr; }
    
    /**
     * @brief Get core ID this manager is running on
     */
    uint8_t getCoreId() const { return coreId; }
    
private:
    static const uint8_t MAX_SERVICES = 16;
    
    struct ServiceEntry {
        Service* service;
        bool active;
    };
    
    ServiceEntry services[MAX_SERVICES];
    uint8_t serviceCount;
    uint8_t coreId;
    Service* blockingService;
    
    // Priority-based scheduling - run lower priority services less frequently
    unsigned long loopCounter;
    uint8_t criticalDivisor;  // How often to run CRITICAL (typically 1 = every loop)
    uint8_t highDivisor;      // How often to run HIGH (e.g., 2 = every 2nd loop)
    uint8_t normalDivisor;    // How often to run NORMAL (e.g., 10 = every 10th loop)
    uint8_t lowDivisor;       // How often to run LOW (e.g., 50 = every 50th loop)
    
    // Core 1 singleton instance
    static jOSmanager* core1Instance;
    
    // Sort services by priority (simple bubble sort - small array)
    void sortServicesByPriority();
    
    /**
     * @brief Set execution divisors for priority-based scheduling
     * @param critical Run CRITICAL services every N loops (default: 1)
     * @param high Run HIGH services every N loops (default: 1)
     * @param normal Run NORMAL services every N loops (default: 5)
     * @param low Run LOW services every N loops (default: 20)
     */
    void setExecutionDivisors(uint8_t critical = 1, uint8_t high = 1, 
                               uint8_t normal = 5, uint8_t low = 20);
};

/**
 * @brief System service wrappers for integrating existing subsystems into jOSmanager
 * 
 * These lightweight wrappers allow existing service routines (termSerial, tud_task, etc.)
 * to be scheduled via jOSmanager with appropriate priorities.
 */

// Forward declarations for external dependencies
class JerialClass;
class oled;

/**
 * @brief Terminal input service - handles line buffering and input processing
 * CRITICAL priority - user input must be responsive
 */
class TermSerialService : public Service {
public:
    static TermSerialService& getInstance();
    TermSerialService(const TermSerialService&) = delete;
    TermSerialService& operator=(const TermSerialService&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "TermSerial"; }
    ServicePriority getPriority() const override { return ServicePriority::CRITICAL; }
    
    void setTermControl(JerialClass* jerial);
    
private:
    TermSerialService();
    ~TermSerialService() = default;
    static TermSerialService* instance;
    JerialClass* jerialInstance;
};

/**
 * @brief TinyUSB task service - handles USB communication
 * HIGH priority - USB communication is time-sensitive
 */
class TinyUSBService : public Service {
public:
    static TinyUSBService& getInstance();
    TinyUSBService(const TinyUSBService&) = delete;
    TinyUSBService& operator=(const TinyUSBService&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "TinyUSB"; }
    ServicePriority getPriority() const override { return ServicePriority::HIGH; }
    
private:
    TinyUSBService() = default;
    ~TinyUSBService() = default;
    static TinyUSBService* instance;
};

/**
 * @brief USB periodic service - handles USB mass storage housekeeping
 * NORMAL priority - periodic USB maintenance
 */
class USBPeriodicService : public Service {
public:
    static USBPeriodicService& getInstance();
    USBPeriodicService(const USBPeriodicService&) = delete;
    USBPeriodicService& operator=(const USBPeriodicService&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "USBPeriodic"; }
    ServicePriority getPriority() const override { return ServicePriority::NORMAL; }
    
private:
    USBPeriodicService() = default;
    ~USBPeriodicService() = default;
    static USBPeriodicService* instance;
};

/**
 * @brief OLED display periodic service - handles display updates
 * LOW priority - display updates are not time-critical
 */
class OLEDService : public Service {
public:
    static OLEDService& getInstance();
    OLEDService(const OLEDService&) = delete;
    OLEDService& operator=(const OLEDService&) = delete;
    
    ServiceStatus service() override;
    const char* getName() const override { return "OLED"; }
    ServicePriority getPriority() const override { return ServicePriority::LOW; }
    
    void setOledDisplay(class oled* display) { oledDisplay = display; }
    
private:
    OLEDService() : oledDisplay(nullptr) {}
    ~OLEDService() = default;
    static OLEDService* instance;
    class oled* oledDisplay;
};

// Global references to services for clean syntax (no need for getInstance())
// These are defined in JumperlOS.cpp (or their respective .cpp files)
extern Probing& probing;
extern Highlighting& highlighting;
extern Menus& menus;
extern Peripherals& peripherals;
extern SlotManager& slotManager;
extern jOSmanager& jOS;

// System service references
extern TermSerialService& termSerialService;
extern TinyUSBService& tinyUSBService;
extern USBPeriodicService& usbPeriodicService;
extern OLEDService& oledService;

#endif // JUMPERLOS_H

