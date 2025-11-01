#ifdef BUILD_BBC_RS232
#ifndef BBC_RS232_FUJI_H
#define BBC_RS232_FUJI_H

#include "../rs232_base/rs232fuji_base.h"
#include "../../bus/bbc_rs232/bbc_rs232.h"

/**
 * @brief BBC RS232 Fuji Device
 *
 * BBC Micro-specific implementation of the RS232 Fuji device.
 * Inherits common RS232 Fuji functionality from rs232FujiBase and
 * provides BBC-specific configuration (device IDs, image format, etc.).
 */
class bbcRS232Fuji : public rs232FujiBase
{
protected:
    /**
     * @brief Get BBC-specific disk device ID base
     * @return RS232_DEVICEID_DISK (0x31)
     */
    uint8_t get_disk_device_id_base() const override { return RS232_DEVICEID_DISK; }
    
    /**
     * @brief Get BBC-specific network device ID base
     * @return RS232_DEVICEID_NETWORK (0x71)
     */
    uint8_t get_network_device_id_base() const override { return RS232_DEVICEID_NETWORK; }
    
    /**
     * @brief Get BBC-specific image extension
     * @return ".ssd" for BBC disk images
     */
    const char* get_image_extension() const override { return ".ssd"; }
    
    /**
     * @brief Get BBC-specific default media type
     * @return MEDIATYPE_UNKNOWN
     */
    mediatype_t get_default_mediatype() const override { return mediatype_t::MEDIATYPE_UNKNOWN; }
    
    /**
     * @brief BBC-specific directory entry formatting
     *
     * Formats directory entry information for transmission to BBC Micro.
     *
     * @param f Directory entry to format
     * @param dest Destination buffer
     * @param maxlen Maximum length
     * @return Number of bytes written
     */
    size_t setDirEntryDetails(fsdir_entry_t *f, uint8_t *dest, uint8_t maxlen) override;
    
    /**
     * @brief Add BBC-specific devices to the bus
     *
     */
    void addDevices() override;

public:
    /**
     * @brief RS232 command processor override
     *
     * Handles BBC-specific commands and rejects network commands.
     * Calls base class for common commands.
     *
     * @param cmd_ptr Pointer to command frame
     */
    void rs232_process(cmdFrame_t *cmd_ptr) override;
};

#endif /* BBC_RS232_FUJI_H */
#endif /* BUILD_BBC_RS232 */