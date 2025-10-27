#ifdef BUILD_BBC_RS232
#include "bbcFuji.h"

#include <endian.h>

#include "../../include/debug.h"

// Global instances
bbcRS232Fuji platformFuji;
fujiDevice *theFuji = &platformFuji;

/**
 * @brief Add BBC-specific devices to the bus
 *
 * Currently adds only disk devices. Network device support
 * will be added in the future as the BBC implementation matures.
 */
void bbcRS232Fuji::addDevices()
{
    Debug_println("bbcRS232Fuji::addDevices()");
    
    // Add disk devices to the bus
    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        SYSTEM_BUS.addDevice(&_fnDisks[i].disk_dev, get_disk_device_id_base() + i);
        Debug_printf("Added disk device %d at ID 0x%02X\n", i, get_disk_device_id_base() + i);
    }
    
    // TODO: Add network devices when BBC network support is implemented
    // for (int i = 0; i < MAX_NETWORK_DEVICES; i++)
    //     SYSTEM_BUS.addDevice(&bbcNetDevs[i], get_network_device_id_base() + i);
}

/**
 * @brief RS232 command processor override
 *
 * Handles BBC-specific command filtering (rejects network commands).
 * Calls base class for all common disk/directory/host commands.
 */
void bbcRS232Fuji::rs232_process(cmdFrame_t *cmd_ptr)
{
    Debug_println("bbcRS232Fuji::rs232_process()");
    
    cmdFrame = *cmd_ptr;
    
    // Check for network commands that BBC doesn't support
    switch (cmdFrame.comnd)
    {
    case FUJICMD_SCAN_NETWORKS:
    case FUJICMD_GET_SCAN_RESULT:
    case FUJICMD_SET_SSID:
    case FUJICMD_GET_SSID:
    case FUJICMD_GET_WIFISTATUS:
    case FUJICMD_GET_WIFI_ENABLED:
    case FUJICMD_GET_ADAPTERCONFIG:
    case FUJICMD_GET_ADAPTERCONFIG_EXTENDED:
        Debug_printf("Network command 0x%02X not implemented for BBC\n", cmdFrame.comnd);
        rs232_nak();
        return;
        
    default:
        // Let base class handle all other commands
        rs232FujiBase::rs232_process(cmd_ptr);
        break;
    }
}

/**
 * @brief Set directory entry details for BBC
 *
 * Formats directory entry information for transmission to BBC Micro.
 * This is BBC-specific and differs from other RS232 platforms.
 */
size_t bbcRS232Fuji::setDirEntryDetails(fsdir_entry_t *f, uint8_t *dest, uint8_t maxlen)
{
    // This is only used for CONFIG when displaying extra information for a file.
    // This whole area needs rethinking and centralising. It's a mess.
    return 0;
}

#endif /* BUILD_BBC_RS232 */