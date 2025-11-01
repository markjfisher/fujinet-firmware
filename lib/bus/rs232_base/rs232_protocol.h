#ifndef RS232_PROTOCOL_H
#define RS232_PROTOCOL_H

#include <cstdint>
#include <forward_list>

// Include channel types (same for all RS232 platforms)
#include "../../hardware/ACMChannel.h"
#include "../../hardware/UARTChannel.h"
#include "../../hardware/TTYChannel.h"

#define RS232_DEVICEID_DISK            0x31
#define RS232_DEVICEID_DISK_LAST       0x3F
#define RS232_DEVICEID_PRINTER         0x40
#define RS232_DEVICEID_PRINTER_LAST    0x43
#define RS232_DEVICEID_NETWORK         0x71
#define RS232_DEVICEID_NETWORK_LAST    0x78
#define RS232_DEVICEID_FUJINET         0x70

// TODO: (Fenrock) is this the right place for this?
#ifdef ESP_PLATFORM
#define SERIAL_DEVICE FN_UART_BUS
#else /* !ESP_PLATFORM */
#define SERIAL_DEVICE Config.get_serial_port()
#endif /* ESP_PLATFORM */

/**
 * @file rs232_protocol.h
 * @brief Shared RS232 protocol definitions and base classes
 * 
 * This file contains the common RS232 protocol code shared across all
 * RS232-based platforms (Atari, BBC, etc.). It includes:
 * - Command frame structure
 * - Protocol timing constants
 * - Checksum calculation
 * - Base device and bus classes
 * 
 * Platform-specific implementations should inherit from these base classes
 * and implement the pure virtual methods.
 */

namespace RS232Protocol {

// ============================================================================
// Protocol Constants
// ============================================================================

/**
 * @brief Default timing delay T4 (microseconds)
 * Can be overridden by platform-specific implementations
 */
constexpr int DEFAULT_DELAY_T4 = 800;

/**
 * @brief Default timing delay T5 (microseconds)
 * Can be overridden by platform-specific implementations
 */
constexpr int DEFAULT_DELAY_T5 = 800;

/**
 * @brief Data direction flags
 */
constexpr uint8_t DIRECTION_NONE  = 0x00;
constexpr uint8_t DIRECTION_READ  = 0x40;
constexpr uint8_t DIRECTION_WRITE = 0x80;

// ============================================================================
// Command Frame Structure
// ============================================================================

/**
 * @brief RS232 command frame structure
 * 
 * This structure is used by all RS232-based platforms to communicate
 * commands between the computer and FujiNet devices.
 * 
 * Layout:
 * - device: Device ID (0x31-0x38 for disks, 0x70 for Fuji, etc.)
 * - comnd: Command byte
 * - aux: 4 auxiliary bytes (can be accessed as bytes, 16-bit, or 32-bit)
 * - cksum: 8-bit checksum
 */
typedef struct
{
    uint8_t device;   ///< Device ID
    uint8_t comnd;    ///< Command byte
    union {
        struct {
            uint8_t aux1;  ///< Auxiliary byte 1
            uint8_t aux2;  ///< Auxiliary byte 2
            uint8_t aux3;  ///< Auxiliary byte 3
            uint8_t aux4;  ///< Auxiliary byte 4
        };
        struct {
            uint16_t aux12;  ///< Aux bytes 1-2 as 16-bit value
            uint16_t aux34;  ///< Aux bytes 3-4 as 16-bit value
        };
        uint32_t aux;  ///< All aux bytes as 32-bit value
    };
    uint8_t cksum;  ///< 8-bit checksum
} __attribute__((packed)) cmdFrame_t;

// ============================================================================
// Protocol Functions
// ============================================================================

/**
 * @brief Calculate RS232 checksum
 * 
 * Calculates an 8-bit wrap-around checksum for the given buffer.
 * This is the standard checksum algorithm used by all RS232 platforms.
 * 
 * @param buf Buffer to checksum
 * @param len Length of buffer in bytes
 * @return 8-bit checksum value
 */
uint8_t calculate_checksum(uint8_t *buf, unsigned short len);

} // namespace RS232Protocol

// ============================================================================
// Forward Declarations
// ============================================================================

class RS232BusBase;

// ============================================================================
// RS232 Device Base Class
// ============================================================================

/**
 * @brief Base class for all RS232 virtual devices
 * 
 * This class provides the common interface and protocol methods that all
 * RS232 devices must implement. Platform-specific device classes should
 * inherit from this class and implement the pure virtual methods.
 * 
 * Provides:
 * - Protocol methods (ACK, NAK, COMPLETE, ERROR)
 * - Aux byte helper methods
 * - Device ID management
 * - Command frame storage
 * 
 * Requires implementation:
 * - rs232_process() - Process incoming commands
 * - rs232_status() - Return device status
 */
class RS232DeviceBase
{
protected:
    friend RS232BusBase;
    
    int _devnum;  ///< Device number (ID)
    
    RS232Protocol::cmdFrame_t cmdFrame;  ///< Current command frame
    bool listen_to_type3_polls = false;  ///< Listen to type 3 polls
    
    RS232BusBase *_bus = nullptr;  ///< Pointer to parent bus
    
    /**
     * @brief Send data buffer to computer (shared implementation)
     *
     * Sends data to the computer with checksum and completion/error status.
     *
     * @param buff Buffer to send
     * @param len Length of buffer
     * @param err Error flag (true = send ERROR, false = send COMPLETE)
     */
    void bus_to_computer(uint8_t *buff, uint16_t len, bool err);
    
    /**
     * @brief Receive data from computer (shared implementation)
     *
     * Receives data from the computer and returns the checksum.
     *
     * @param buff Buffer to receive into
     * @param len Length to receive
     * @return Checksum byte from computer
     */
    uint8_t bus_to_peripheral(uint8_t *buff, uint16_t len);
    
    /**
     * @brief Send acknowledgement to computer (shared implementation)
     *
     * Sends 'A' to indicate command accepted and processing will begin.
     * This should be called after validating a command but before processing it.
     */
    void rs232_ack();
    
    /**
     * @brief Send non-acknowledgement to computer (shared implementation)
     *
     * Sends 'N' to indicate command rejected or invalid.
     * This should be called if the command is malformed or unsupported.
     */
    void rs232_nak();
    
    /**
     * @brief Send completion to computer (shared implementation)
     *
     * Sends 'C' to indicate command completed successfully.
     * This should be called after successfully processing a command.
     */
    void rs232_complete();
    
    /**
     * @brief Send error to computer (shared implementation)
     *
     * Sends 'E' to indicate command failed.
     * This should be called if an error occurred during command processing.
     */
    void rs232_error();
    
    /**
     * @brief Get aux bytes as 16-bit value (low)
     * @return aux1 + (aux2 << 8)
     */
    uint16_t rs232_get_aux16_lo();
    
    /**
     * @brief Get aux bytes as 16-bit value (high)
     * @return aux3 + (aux4 << 8)
     */
    uint16_t rs232_get_aux16_hi();
    
    /**
     * @brief Get all aux bytes as 32-bit value
     * @return All aux bytes combined
     */
    uint32_t rs232_get_aux32();
    
    /**
     * @brief Pure virtual: Return status to computer
     * 
     * All devices must implement this to return device-specific status.
     * Typically sends 4 bytes of status information.
     */
    virtual void rs232_status() = 0;
    
    /**
     * @brief Pure virtual: Process a command
     * 
     * All devices must implement this to handle incoming commands.
     * This is typically implemented as a switch statement on cmdFrame.comnd.
     * 
     * @param cmd_ptr Pointer to command frame
     */
    virtual void rs232_process(RS232Protocol::cmdFrame_t *cmd_ptr) = 0;
    
public:
    /**
     * @brief Optional shutdown/cleanup routine
     *
     * Called when the system is shutting down. Override to perform
     * device-specific cleanup.
     */
    virtual void shutdown() {}
    
    /**
     * @brief Get device ID
     * @return Device number (1-255)
     */
    int id() { return _devnum; }
    
    /**
     * @brief Is this device the config device?
     */
    bool is_config_device = false;
    
    /**
     * @brief Is device active (powered on)?
     */
    bool device_active = true;
    
    /**
     * @brief Status wait counter
     */
    uint8_t status_wait_count = 5;
};

// ============================================================================
// RS232 Bus Base Class
// ============================================================================

/**
 * @brief Base class for RS232 bus management
 * 
 * This class provides common bus management functionality that all RS232
 * platforms can use. Platform-specific bus classes should inherit from
 * this class and implement the pure virtual methods.
 * 
 * Provides:
 * - Device registration and management
 * - Device lookup by ID
 * - Baud rate management
 * - Daisy chain management
 * 
 * Requires implementation:
 * - setup() - Initialize the bus
 * - service() - Service the bus (main loop)
 * - shutdown() - Shutdown the bus
 */
class RS232BusBase
{
    friend RS232DeviceBase;  // Allow devices to call writeByte/flushOutput
    
protected:
    std::forward_list<RS232DeviceBase*> _daisyChain;  ///< Device daisy chain
    
    int _command_frame_counter = 0;  ///< Command frame counter
    int _rs232Baud;                  ///< Current baud rate
    
    RS232DeviceBase *_activeDev = nullptr;  ///< Currently active device
    
    // Serial port - same conditional compilation for all RS232 platforms
#if FUJINET_OVER_USB
    ACMChannel _port;
#elif defined(ITS_A_UNIX_SYSTEM_I_KNOW_THIS)
    TTYChannel _port;
#else
    UARTChannel _port;
#endif

public:
    /**
     * @brief Pure virtual: Initialize the bus
     *
     * Platform-specific implementations must initialize:
     * - Serial port (call _port.begin())
     * - Set _rs232Baud
     * - GPIO pins (if applicable)
     * - Any platform-specific hardware
     */
    virtual void setup() = 0;
    
    /**
     * @brief Service the bus (shared implementation)
     *
     * Checks for incoming commands and processes them using processCommand().
     * Platform-specific implementations can override if they need special
     * handling (e.g., modem mode, CPM mode).
     */
    virtual void service();
    
    /**
     * @brief Shutdown the bus (shared implementation)
     *
     * Shuts down all devices and closes the serial port.
     * Platform-specific implementations can override if needed.
     */
    virtual void shutdown();
    
    /**
     * @brief Process a command frame (shared implementation)
     *
     * This method handles the common logic of:
     * - Verifying checksum
     * - Finding the target device
     * - Routing the command to the device
     *
     * Platform-specific service() methods should call this when a command is ready.
     *
     * @param cmd Pointer to command frame to process
     */
    void processCommand(RS232Protocol::cmdFrame_t *cmd);
    
    /**
     * @brief Add a device to the bus
     *
     * Registers a device with the bus and assigns it a device ID.
     * The device will be added to the daisy chain and can receive commands.
     * Also sets the device's bus pointer so it can call bus methods.
     *
     * @param pDevice Pointer to device
     * @param device_id Device ID (1-255)
     */
    void addDevice(RS232DeviceBase *pDevice, int device_id);
    
    /**
     * @brief Remove a device from the bus
     * 
     * Unregisters a device from the bus. The device will no longer
     * receive commands.
     * 
     * @param pDevice Pointer to device
     */
    void remDevice(RS232DeviceBase *pDevice);
    
    /**
     * @brief Find device by ID
     * 
     * Searches the daisy chain for a device with the given ID.
     * 
     * @param device_id Device ID to find
     * @return Pointer to device or nullptr if not found
     */
    RS232DeviceBase* deviceById(int device_id);
    
    /**
     * @brief Change device ID
     * 
     * Changes the ID of a registered device.
     * 
     * @param pDevice Pointer to device
     * @param device_id New device ID
     */
    void changeDeviceId(RS232DeviceBase *pDevice, int device_id);
    
    /**
     * @brief Get number of devices on bus
     * 
     * Counts the number of devices in the daisy chain.
     * 
     * @return Device count
     */
    int numDevices();
    
    /**
     * @brief Get current baud rate
     * @return Baud rate
     */
    int getBaudrate() { return _rs232Baud; }
    
    /**
     * @brief Check if shutting down
     * @return true if shutting down
     */
    bool shuttingDown = false;
    bool getShuttingDown() { return shuttingDown; }
    
    /**
     * @brief Direct serial port access (shared for all RS232 platforms)
     *
     * These methods provide direct access to the underlying serial port.
     * All RS232 platforms use the same channel types, so these can be shared.
     */
    size_t read(void *buffer, size_t length) { return _port.read(buffer, length); }
    size_t read() { return _port.read(); }
    size_t write(const void *buffer, size_t length) { return _port.write(buffer, length); }
    size_t write(int n) { return _port.write(n); }
    size_t writeByte(uint8_t b) { return _port.write(b); }
    size_t available() { return _port.available(); }
    void flushOutput() { _port.flushOutput(); }
    size_t print(int n, int base = 10) { return _port.print(n, base); }
    size_t print(const char *str) { return _port.print(str); }
    size_t print(const std::string &str) { return _port.print(str); }
};

#endif // RS232_PROTOCOL_H