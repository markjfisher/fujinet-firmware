#ifdef BUILD_BBC_RS232
#include "bbcFuji.h"

#include "fnSystem.h"
#include "fnConfig.h"
#include "fsFlash.h"
#include "fnWiFi.h"
#include "utils.h"
#include "compat_string.h"
#include <endian.h>

#define IMAGE_EXTENSION ".ssd"

// Global instances
bbcRS232Fuji platformFuji;
fujiDevice *theFuji = &platformFuji;

/**
 * @brief Setup the BBC RS232 Fuji device
 * 
 * Initializes disk slots from config and adds devices to the bus
 */
void bbcRS232Fuji::setup()
{
    Debug_println("bbcRS232Fuji::setup()");
    
    // Populate disk slots from configuration
    populate_slots_from_config();
    
    // Insert boot device (config disk)
    // TODO: need to handle this, it doesn't work because of new MediaType
    insert_boot_device(Config.get_general_boot_mode(), IMAGE_EXTENSION, MEDIATYPE_UNKNOWN, &bootdisk);
    
    // Configure boot settings
    boot_config = Config.get_general_config_enabled();
    status_wait_enabled = Config.get_general_status_wait_enabled();
    
    // Why do we do this? Why not add them on demand?
    // Add disk devices to the bus
    for (int i = 0; i < MAX_DISK_DEVICES; i++)
    {
        SYSTEM_BUS.addDevice(&_fnDisks[i].disk_dev, RS232_DEVICEID_DISK + i);
        Debug_printf("Added disk device %d at ID 0x%02X\n", i, RS232_DEVICEID_DISK + i);
    }
    
    Debug_println("bbcRS232Fuji::setup() complete");
}

/**
 * @brief RS232 status command
 */
void bbcRS232Fuji::rs232_status()
{
    Debug_println("bbcRS232Fuji::rs232_status()");
    
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
        char ret[4] = {0};
        Debug_printf("Status for what? %08lx\n", cmdFrame.aux);
        transaction_put((uint8_t *)ret, sizeof(ret), false);
    }
}

/**
 * @brief Create new disk image
 */
void bbcRS232Fuji::rs232_new_disk()
{
    Debug_println("bbcRS232Fuji::rs232_new_disk()");
    
    struct
    {
        unsigned short numSectors;
        unsigned short sectorSize;
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
    
    bool ok = disk.disk_dev.write_blank(disk.fileh, newDisk.sectorSize, newDisk.numSectors);
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
 */
void bbcRS232Fuji::rs232_open_directory()
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
 */
void bbcRS232Fuji::rs232_copy_file()
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
 * @brief RS232 command processor
 * 
 * Handles Fuji commands - minimal implementation for disk support
 */
void bbcRS232Fuji::rs232_process(cmdFrame_t *cmd_ptr)
{
    Debug_println("bbcRS232Fuji::rs232_process()");
    
    cmdFrame = *cmd_ptr;
    
    switch (cmdFrame.comnd)
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
        fujicmd_set_boot_mode(cmdFrame.aux1, IMAGE_EXTENSION, MEDIATYPE_UNKNOWN, &bootdisk);
        break;
        
    // Network commands not implemented for minimal disk-only support
    case FUJICMD_SCAN_NETWORKS:
    case FUJICMD_GET_SCAN_RESULT:
    case FUJICMD_SET_SSID:
    case FUJICMD_GET_SSID:
    case FUJICMD_GET_WIFISTATUS:
    case FUJICMD_GET_WIFI_ENABLED:
    case FUJICMD_GET_ADAPTERCONFIG:
    case FUJICMD_GET_ADAPTERCONFIG_EXTENDED:
        Debug_printf("Network command 0x%02X not implemented\n", cmdFrame.comnd);
        rs232_nak();
        break;
        
    default:
        Debug_printf("Unknown command: 0x%02X\n", cmdFrame.comnd);
        rs232_nak();
        break;
    }
}

/**
 * @brief Set directory entry details for BBC
 * 
 * Formats directory entry information for transmission to BBC
 */
size_t bbcRS232Fuji::setDirEntryDetails(fsdir_entry_t *f, uint8_t *dest, uint8_t maxlen)
{
    struct DirEntAttrib
    {
        uint8_t year, month, day, hour, minute, second;
        uint32_t file_size;
        uint8_t flags, truncated;
        uint8_t file_type;
    } __attribute__((packed));
    
    DirEntAttrib *attrib = (DirEntAttrib *)dest;
    
    // File modified date-time
    struct tm *modtime = localtime(&f->modified_time);
    attrib->year = modtime->tm_year - 100;
    attrib->month = modtime->tm_mon + 1;
    attrib->day = modtime->tm_mday;
    attrib->hour = modtime->tm_hour;
    attrib->minute = modtime->tm_min;
    attrib->second = modtime->tm_sec;
    
    attrib->file_size = htole32(f->size);
    
    // File flags
    #define FF_DIR 0x01
    #define FF_TRUNC 0x02
    
    attrib->flags = f->isDir ? FF_DIR : 0;
    
    maxlen -= sizeof(*attrib);
    if (f->isDir)
        maxlen--;
    
    attrib->truncated = strlen(f->filename) >= maxlen ? FF_TRUNC : 0;
    
    // File type (detect from extension)
    attrib->file_type = MediaTypeBase::discover_mediatype(f->filename);
    
    Debug_printf("Dir entry: %s, size=%lu, flags=0x%02X\n", 
                 f->filename, f->size, attrib->flags);
    
    return sizeof(*attrib);
}

#endif /* BUILD_BBC_RS232 */