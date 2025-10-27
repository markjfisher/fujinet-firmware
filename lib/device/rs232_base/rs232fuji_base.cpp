#include "rs232fuji_base.h"

#include "fnSystem.h"
#include "fnConfig.h"
#include "fsFlash.h"
#include "fnWiFi.h"
#include "utils.h"
#include "compat_string.h"
#include <endian.h>

#include "../../include/debug.h"
#include "../../utils/utils.h"

/**
 * @brief Common setup implementation
 * 
 * Initializes disk slots from config and adds devices to the bus.
 * This is the shared setup logic for all RS232 Fuji implementations.
 */
void rs232FujiBase::setup()
{
    Debug_println("rs232FujiBase::setup()");
    
    // Populate disk slots from configuration
    populate_slots_from_config();
    
    // Insert boot device (config disk) using platform-specific extension and media type
    insert_boot_device(Config.get_general_boot_mode(), 
                      get_image_extension(), 
                      get_default_mediatype(), 
                      &bootdisk);
    
    // Configure boot settings
    boot_config = Config.get_general_config_enabled();
    status_wait_enabled = Config.get_general_status_wait_enabled();
    
    // Let platform-specific implementation add its devices
    addDevices();
    
    Debug_println("rs232FujiBase::setup() complete");
}

/**
 * @brief Get data from computer with checksum validation
 * 
 * Receives data from the computer and validates the checksum using
 * the shared RS232 protocol checksum calculation.
 */
bool rs232FujiBase::transaction_get(void *data, size_t len)
{
    uint8_t ck = virtualDevice::bus_to_peripheral((uint8_t *)data, len);
    uint8_t calculated = RS232Protocol::calculate_checksum((uint8_t *)data, len);
    Debug_printv("ck: %02x, calc: %02x, data:\n%s", ck, calculated, util_hexdump(data, len).c_str());
    return calculated == ck;
}

/**
 * @brief Send data to computer with checksum
 * 
 * Sends data to the computer using the shared RS232 protocol.
 */
void rs232FujiBase::transaction_put(const void *data, size_t len, bool err)
{
    virtualDevice::bus_to_computer((uint8_t *)data, len, err);
}

/**
 * @brief Common RS232 status implementation
 * 
 * Returns mount status for all disk slots.
 */
void rs232FujiBase::rs232_status()
{
    Debug_println("rs232FujiBase::rs232_status()");
    
    if (cmdFrame.aux == STATUS_MOUNT_TIME)
    {
        // Return drive slot mount status: 0 if unmounted, otherwise time when mounted
        time_t mount_status[MAX_DISK_DEVICES];
        
        for (int idx = 0; idx < MAX_DISK_DEVICES; idx++)
        {
            mount_status[idx] = _fnDisks[idx].disk_dev.get_mount_time();
        }
        
        transaction_put((uint8_t *)mount_status, sizeof(mount_status), false);
    }
    else
    {
        // each device type seems to fill this in, e.g. network setups up its own status. Why is this here though?
        char ret[4] = {0};
        Debug_printf("Status for what? %08lx\n", cmdFrame.aux);
        transaction_put((uint8_t *)ret, sizeof(ret), false);
    }
}

/**
 * @brief Create new disk image
 *
 * Common implementation for creating a new blank disk image.
 */
void rs232FujiBase::rs232_new_disk()
{
    Debug_println("rs232FujiBase::rs232_new_disk()");
    
    struct
    {
        unsigned short numSectors;
        unsigned short sectorSize;
        // NEW FIELD TO ALLOW USERS TO SPECIFY THE DISK TYPE TO CREATE
        // required to differentiate between types that have similar geometry, e.g. 40 Track DSD  and 80 Track SSD on BBC
        // TODO: probably needs rework in general as this also has a crappy large filename buffer
        unsigned short mediaType;
        unsigned char hostSlot;
        unsigned char deviceSlot;
        char filename[MAX_FILENAME_LEN];
    } newDisk;
    
    // Get new disk parameters
    if (!transaction_get((uint8_t *)&newDisk, sizeof(newDisk)))
    {
        Debug_println("Bad checksum");
        transaction_error();
        return;
    }
    
    if (newDisk.deviceSlot >= MAX_DISK_DEVICES || newDisk.hostSlot >= MAX_HOSTS)
    {
        Debug_println("Bad disk or host slot parameter");
        transaction_error();
        return;
    }
    
    fujiDisk &disk = _fnDisks[newDisk.deviceSlot];
    fujiHost &host = _fnHosts[newDisk.hostSlot];
    
    disk.host_slot = newDisk.hostSlot;
    disk.access_mode = DISK_ACCESS_MODE_WRITE;
    strlcpy(disk.filename, newDisk.filename, sizeof(disk.filename));
    
    if (host.file_exists(disk.filename))
    {
        Debug_printf("File exists: \"%s\"\n", disk.filename);
        transaction_error();
        return;
    }
    
    disk.fileh = host.fnfile_open(disk.filename, disk.filename, sizeof(disk.filename), "w");
    if (disk.fileh == nullptr)
    {
        Debug_printf("Couldn't open file for writing: \"%s\"\n", disk.filename);
        transaction_error();
        return;
    }
    
    // Pass the media type to write_blank
    mediatype_t disk_type = static_cast<mediatype_t>(newDisk.mediaType);
    Debug_printf("Creating disk with mediaType=0x%04X\n", disk_type);
    
    bool ok = disk.disk_dev.write_blank(disk.fileh, newDisk.sectorSize, newDisk.numSectors, disk_type);
    fnio::fclose(disk.fileh);
    
    if (ok == false)
    {
        Debug_println("Data write failed");
        transaction_error();
        return;
    }
    
    Debug_println("New disk created successfully");
    transaction_complete();
}

/**
 * @brief Open directory for browsing
 * 
 * Common implementation for opening a directory on a host.
 */
void rs232FujiBase::rs232_open_directory()
{
    char dirpath[256];
    
    if (!transaction_get(dirpath, sizeof(dirpath)))
    {
        transaction_error();
        return;
    }
    
    fujicmd_open_directory_success(cmdFrame.aux1, dirpath, sizeof(dirpath));
}

/**
 * @brief Copy file between hosts
 * 
 * Common implementation for copying files.
 */
void rs232FujiBase::rs232_copy_file()
{
    char csBuf[256];
    
    if (!transaction_get(csBuf, sizeof(csBuf)))
    {
        transaction_error();
        return;
    }
    
    fujicmd_copy_file_success(cmdFrame.aux1, cmdFrame.aux2, csBuf);
}

/**
 * @brief Common RS232 command processor
 * 
 * Handles common Fuji commands that are shared across all RS232 platforms.
 * This includes disk operations, directory operations, and host management.
 * 
 * Platform-specific implementations can override this to add additional
 * commands, but should call this base implementation for common commands.
 */
void rs232FujiBase::rs232_process(FujiBusPacket &packet)
{
    Debug_println("rs232FujiBase::rs232_process()");
    
    Debug_printf("CF: %02x %02x %02x %02x %02x %02x [chk: %02x]\n", packet.device(), packet.command());
    
    switch (packet.command())
    {
    case FUJICMD_STATUS:
        rs232_ack();
        rs232_status();
        break;
        
    case FUJICMD_RESET:
        rs232_ack();
        fujicmd_reset();
        break;
        
    case FUJICMD_MOUNT_HOST:
        rs232_ack();
        fujicmd_mount_host_success(cmdFrame.aux1);
        break;
        
    case FUJICMD_MOUNT_IMAGE:
        rs232_ack();
        fujicmd_mount_disk_image_success(cmdFrame.aux1, cmdFrame.aux2);
        break;
        
    case FUJICMD_UNMOUNT_IMAGE:
        rs232_ack();
        fujicmd_unmount_disk_image_success(cmdFrame.aux1);
        break;
        
    case FUJICMD_OPEN_DIRECTORY:
        rs232_ack();
        rs232_open_directory();
        break;
        
    case FUJICMD_READ_DIR_ENTRY:
        rs232_ack();
        fujicmd_read_directory_entry(cmdFrame.aux1, cmdFrame.aux2);
        break;
        
    case FUJICMD_CLOSE_DIRECTORY:
        rs232_ack();
        fujicmd_close_directory();
        break;
        
    case FUJICMD_GET_DIRECTORY_POSITION:
        rs232_ack();
        fujicmd_get_directory_position();
        break;
        
    case FUJICMD_SET_DIRECTORY_POSITION:
        rs232_ack();
        fujicmd_set_directory_position(rs232_get_aux16_lo());
        break;
        
    case FUJICMD_READ_HOST_SLOTS:
        rs232_ack();
        fujicmd_read_host_slots();
        break;
        
    case FUJICMD_WRITE_HOST_SLOTS:
        rs232_ack();
        fujicmd_write_host_slots();
        break;
        
    case FUJICMD_WRITE_HOST_SLOT_N:
        rs232_ack();
        fujicmd_write_host_slot_n();
        break;
        
    case FUJICMD_READ_DEVICE_SLOTS:
        rs232_ack();
        fujicmd_read_device_slots(MAX_DISK_DEVICES);
        break;
        
    case FUJICMD_WRITE_DEVICE_SLOTS:
        rs232_ack();
        fujicmd_write_device_slots(MAX_DISK_DEVICES);
        break;
        
    case FUJICMD_NEW_DISK:
        rs232_ack();
        rs232_new_disk();
        break;
        
    case FUJICMD_SET_DEVICE_FULLPATH:
        rs232_ack();
        fujicmd_set_device_filename_success(cmdFrame.aux1, cmdFrame.aux2 >> 4, cmdFrame.aux2 & 0x0F);
        break;
        
    case FUJICMD_SET_HOST_PREFIX:
        rs232_ack();
        fujicmd_set_host_prefix(cmdFrame.aux1);
        break;
        
    case FUJICMD_GET_HOST_PREFIX:
        rs232_ack();
        fujicmd_get_host_prefix(cmdFrame.aux1);
        break;
        
    case FUJICMD_GET_DEVICE_FULLPATH:
        rs232_ack();
        fujicmd_get_device_filename(cmdFrame.aux1);
        break;
        
    case FUJICMD_CONFIG_BOOT:
        rs232_ack();
        fujicmd_set_boot_config(cmdFrame.aux1);
        break;
        
    case FUJICMD_COPY_FILE:
        rs232_ack();
        rs232_copy_file();
        break;
        
    case FUJICMD_MOUNT_ALL:
        rs232_ack();
        fujicmd_mount_all_success();
        break;
        
    case FUJICMD_SET_BOOT_MODE:
        rs232_ack();
        fujicmd_set_boot_mode(cmdFrame.aux1, get_image_extension(), get_default_mediatype(), &bootdisk);
        break;
        
    default:
        // Unknown command - let platform-specific implementation handle it
        Debug_printf("Unknown command in base: 0x%02X\n", cmdFrame.comnd);
        rs232_nak();
        break;
    }
}