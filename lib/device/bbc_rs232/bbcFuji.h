#ifdef BUILD_BBC_RS232
#ifndef BBC_RS232_FUJI_H
#define BBC_RS232_FUJI_H

#include "../fujiDevice.h"  // This already includes disk.h via fujiDisk.h
#include "../../bus/bbc_rs232/bbc_rs232.h"

#define STATUS_MOUNT_TIME 0x01

/**
 * @brief BBC RS232 Fuji Device
 *
 * Minimal implementation to support disk operations for BBC Micro.
 * This class manages disk slots and handles Fuji commands over RS232.
 *
 * Note: Inherits from fujiDevice which already inherits from virtualDevice,
 * so we don't need to inherit from virtualDevice again.
 */
class bbcRS232Fuji : public fujiDevice
{
private:

protected:
    // Implement fujiDevice pure virtuals with RS232 protocol
    void transaction_complete() override { rs232_complete(); }
    void transaction_error() override { rs232_error(); }
    
    bool transaction_get(void *data, size_t len) override {
        uint8_t ck = virtualDevice::bus_to_peripheral((uint8_t *)data, len);
        if (RS232Protocol::calculate_checksum((uint8_t *)data, len) != ck)
            return false;
        return true;
    }
    
    void transaction_put(const void *data, size_t len, bool err) override {
        virtualDevice::bus_to_computer((uint8_t *)data, len, err);
    }
    
    size_t setDirEntryDetails(fsdir_entry_t *f, uint8_t *dest, uint8_t maxlen) override;
    
    // BBC-specific command handlers (minimal set for disk support)
    void rs232_new_disk();           // Create new disk image
    void rs232_open_directory();     // Open directory for browsing
    void rs232_copy_file();          // Copy file between hosts

public:
    /**
     * @brief Setup the Fuji device
     * 
     * Initializes disk slots and adds devices to the bus
     */
    void setup() override;
    
    /**
     * @brief RS232 status command
     */
    void rs232_status() override;
    
    /**
     * @brief RS232 command processor
     * 
     * Handles Fuji commands received over RS232
     * 
     * @param cmd_ptr Pointer to command frame
     */
    void rs232_process(cmdFrame_t *cmd_ptr) override;
};

#endif /* BBC_RS232_FUJI_H */
#endif /* BUILD_BBC_RS232 */