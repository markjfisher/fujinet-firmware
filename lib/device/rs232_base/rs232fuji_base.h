#ifndef RS232FUJI_BASE_H
#define RS232FUJI_BASE_H

#include "../fujiDevice.h"
#include "../../bus/rs232_base/rs232_protocol.h"

#define STATUS_MOUNT_TIME 0x01

/**
 * @file rs232fuji_base.h
 * @brief Base class for RS232 Fuji device implementations
 * 
 * This class provides the common implementation for RS232-based Fuji devices
 * across all platforms (Atari, BBC, etc.). It extracts the shared logic from
 * platform-specific implementations to reduce code duplication and ensure
 * consistent behavior.
 * 
 * Platform-specific classes should inherit from this base class and:
 * - Provide platform-specific device IDs via pure virtual methods
 * - Provide platform-specific image extensions via pure virtual methods
 * - Override setDirEntryDetails() for platform-specific directory formatting
 * - Optionally override rs232_process() to add platform-specific commands
 */
class rs232FujiBase : public fujiDevice
{
protected:
    /**
     * @brief Transaction completion (uses RS232 protocol)
     */
    void transaction_complete() override { rs232_complete(); }
    
    /**
     * @brief Transaction error (uses RS232 protocol)
     */
    void transaction_error() override { rs232_error(); }
    
    /**
     * @brief Get data from computer with checksum validation
     * 
     * Receives data from the computer and validates the checksum.
     * Uses the shared RS232 protocol checksum calculation.
     * 
     * @param data Buffer to receive data into
     * @param len Length of data to receive
     * @return true if checksum valid, false otherwise
     */
    bool transaction_get(void *data, size_t len) override;
    
    /**
     * @brief Send data to computer with checksum
     * 
     * Sends data to the computer using the shared RS232 protocol.
     * 
     * @param data Buffer to send
     * @param len Length of data
     * @param err Error flag (true = send ERROR, false = send COMPLETE)
     */
    void transaction_put(const void *data, size_t len, bool err = false) override;
    
    /**
     * @brief Pure virtual: Get platform-specific disk device ID base
     * 
     * Each platform may use different device ID ranges.
     * For example:
     * - BBC uses RS232_DEVICEID_DISK (0x31)
     * - Atari uses FUJI_DEVICEID_DISK
     * 
     * @return Base device ID for disk devices
     */
    virtual uint8_t get_disk_device_id_base() const = 0;
    
    /**
     * @brief Pure virtual: Get platform-specific network device ID base
     * 
     * Each platform may use different device ID ranges.
     * For example:
     * - BBC uses RS232_DEVICEID_NETWORK (0x71)
     * - Atari uses FUJI_DEVICEID_NETWORK
     * 
     * @return Base device ID for network devices
     */
    virtual uint8_t get_network_device_id_base() const = 0;
    
    /**
     * @brief Pure virtual: Get platform-specific image extension
     * 
     * Different platforms use different disk image formats:
     * - BBC uses ".ssd"
     * - Atari uses ".img"
     * 
     * @return Image file extension (including the dot)
     */
    virtual const char* get_image_extension() const = 0;
    
    /**
     * @brief Pure virtual: Get platform-specific media type
     *
     * Different platforms may use different default media types.
     *
     * @return Default media type for disk images
     */
    virtual mediatype_t get_default_mediatype() const = 0;
    
    /**
     * @brief Pure virtual: Add platform-specific devices to the bus
     *
     * Each platform must implement this to add its supported devices.
     * For example:
     * - BBC adds only disk devices (network support not yet implemented)
     * - Atari adds both disk and network devices
     *
     * This is called by setup() after common initialization is complete.
     */
    virtual void addDevices() = 0;
    
    /**
     * @brief Common RS232 status implementation
     * 
     * Returns mount status for all disk slots.
     * Can be overridden by platforms that need different status behavior.
     */
    void rs232_status() override;
    
    /**
     * @brief Common new disk implementation
     * 
     * Creates a new blank disk image on the specified host.
     * This implementation is shared across all RS232 platforms.
     */
    void rs232_new_disk();
    
    /**
     * @brief Common open directory implementation
     * 
     * Opens a directory for browsing on the specified host.
     * This implementation is shared across all RS232 platforms.
     */
    void rs232_open_directory();
    
    /**
     * @brief Common copy file implementation
     * 
     * Copies a file between hosts.
     * This implementation is shared across all RS232 platforms.
     */
    void rs232_copy_file();
    
    /**
     * @brief Common RS232 command processor
     * 
     * Handles common Fuji commands that are shared across all RS232 platforms.
     * Platform-specific implementations can override this to add additional
     * commands, but should call this base implementation for common commands.
     * 
     * @param cmd_ptr Pointer to command frame
     */
    void rs232_process(FujiBusPacket &packet) override;

public:
    /**
     * @brief Common setup implementation
     * 
     * Initializes disk slots from config and adds devices to the bus.
     * Platform-specific implementations can override to add additional
     * setup steps, but should call this base implementation.
     */
    void setup() override;
};

#endif /* RS232FUJI_BASE_H */