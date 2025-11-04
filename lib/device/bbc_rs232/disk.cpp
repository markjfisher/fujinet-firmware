#include "disk.h"
#include "../../media/bbc/mediatype_ssd.h"
#include "../../media/bbc/mediatype_dsd.h"

// Include debug if available
#ifdef ESP_PLATFORM
#include "../../include/debug.h"
#else
#include <cstdio>
#define Debug_printf printf
#define Debug_print(x) printf("%s", x)
#endif

/**
 * @brief Constructor
 */
bbcRS232Disk::bbcRS232Disk() : DiskBase(), virtualDevice() {}

/**
 * @brief Destructor
 */
bbcRS232Disk::~bbcRS232Disk()
{
    Debug_print("bbcRS232Disk::~bbcRS232Disk()\n");
}

/**
 * @brief Factory method for BBC media types
 *
 * Creates MediaType objects for BBC-supported formats.
 */
std::unique_ptr<MediaTypeBase> bbcRS232Disk::create_media_type(mediatype_t disk_type)
{
    Debug_printf("bbcRS232Disk::create_media_type(0x%04X)\n", disk_type);
    
    switch (disk_type)
    {
    case MEDIATYPE_SSD:
        Debug_print("Creating MediaTypeSSD\n");
        return std::make_unique<MediaTypeSSD>();
        
    case MEDIATYPE_DSD:
        Debug_print("Creating MediaTypeDSD\n");
        return std::make_unique<MediaTypeDSD>();
        
    case MEDIATYPE_ADFS:
        Debug_print("ADFS format not yet implemented\n");
        return nullptr;
        
    default:
        Debug_printf("Unsupported media type: 0x%04X\n", disk_type);
        return nullptr;
    }
}

/**
 * @brief Read a sector from the mounted media
 * 
 * Implements DiskBase pure virtual method for BBC protocol.
 */
bool bbcRS232Disk::read_sector(uint32_t sector_num, uint8_t *buffer, uint16_t *count)
{
    Debug_printf("bbcRS232Disk::read_sector(%u)\n", sector_num);
    
    if (!is_mounted())
    {
        Debug_print("ERROR: No media mounted\n");
        return true; // Error
    }
    
    // Call the media type's read method
    bool err = _media->read(sector_num, count);
    
    if (!err && buffer != nullptr)
    {
        // Copy data from media's sector buffer to provided buffer
        memcpy(buffer, _media->get_sector_buffer(), *count);
    }
    
    return err;
}

/**
 * @brief Write a sector to the mounted media
 * @return Error status, true if there was an error
 * Implements DiskBase pure virtual method for BBC protocol.
 */
bool bbcRS232Disk::write_sector(uint32_t sector_num, const uint8_t *buffer, uint16_t *count, bool verify)
{
    Debug_printf("bbcRS232Disk::write_sector(%u, verify=%d)\n", sector_num, verify);

    if (!is_mounted())
    {
        Debug_print("ERROR: No media mounted\n");
        return true; // Error
    }
    
    if (_readonly)
    {
        Debug_print("ERROR: Media is read-only\n");
        return true; // Error
    }
    
    // Copy data from provided buffer to media's sector buffer
    if (buffer != nullptr && count != nullptr)
    {
        memcpy(_media->get_sector_buffer(), buffer, *count);
    }
    
    // Call the media type's write method
    bool err = _media->write(sector_num, verify);
    
    return err;
}

/**
 * @brief Get BBC-specific status information
 * 
 * Implements DiskBase pure virtual method.
 * 
 * BBC status format (to be refined based on actual BBC protocol):
 * Byte 0: Drive status
 *   Bit 7: Reserved
 *   Bit 6: Double-sided
 *   Bit 5: 80 track
 *   Bit 4: Reserved
 *   Bit 3: Write protected
 *   Bit 2: Reserved
 *   Bit 1: Reserved
 *   Bit 0: Disk present
 * 
 * Byte 1: Number of tracks (40 or 80)
 * Byte 2: Sectors per track (10 for DFS)
 * Byte 3: Sector size (256 bytes = 0x01)
 */
void bbcRS232Disk::get_status(uint8_t *status_buffer, size_t buffer_size)
{
    Debug_printf("bbcRS232Disk::get_status()\n");
    
    if (status_buffer == nullptr || buffer_size < 4)
        return;
    
    // Initialize status bytes
    memset(status_buffer, 0, buffer_size);
    
    if (!is_mounted())
    {
        // No disk present
        status_buffer[0] = 0x00;
        return;
    }
    
    // Byte 0: Drive status
    status_buffer[0] = 0x01; // Disk present
    
    if (_readonly)
    {
        status_buffer[0] |= 0x08; // Write protected
    }
    
    // Determine disk geometry from media type
    uint32_t num_sectors = _media->num_sectors();
    uint16_t sector_size = _media->get_sector_size();
    
    // DFS: 10 sectors per track, 256 bytes per sector
    uint8_t sectors_per_track = 10;
    uint32_t total_tracks = num_sectors / sectors_per_track;
    
    // Check if double-sided (DSD has 2x the sectors of SSD)
    bool double_sided = (_media->_media_type == MEDIATYPE_DSD);
    if (double_sided)
    {
        status_buffer[0] |= 0x40; // Double-sided
        total_tracks /= 2; // Tracks per side
    }
    
    // Check if 80 track
    bool track_80 = (total_tracks > 40);
    if (track_80)
    {
        status_buffer[0] |= 0x20; // 80 track
    }
    
    // Byte 1: Number of tracks per side
    status_buffer[1] = (uint8_t)total_tracks;
    
    // Byte 2: Sectors per track
    status_buffer[2] = sectors_per_track;
    
    // Byte 3: Sector size (256 bytes = 0x01, representing 256)
    status_buffer[3] = (sector_size == 256) ? 0x01 : 0x00;
    
    Debug_printf("Status: tracks=%u, spt=%u, size=%u, flags=0x%02X\n",
                 status_buffer[1], status_buffer[2], sector_size, status_buffer[0]);
}

/**
 * @brief RS232 read command handler
 *
 * Implements virtualDevice pure virtual method.
 */
void bbcRS232Disk::rs232_read()
{
    Debug_print("bbcRS232Disk::rs232_read()\n");
    
    if (!is_mounted())
    {
        rs232_error();
        return;
    }
    
    // Get sector number from command frame
    uint32_t sector_num = rs232_get_aux32();
    
    uint16_t count;
    uint8_t *buffer = _media->get_sector_buffer();
    
    // Read the sector
    bool err = read_sector(sector_num, buffer, &count);
    
    // Send result to BBC
    bus_to_computer(buffer, count, err);
}

/**
 * @brief RS232 write command handler
 */
void bbcRS232Disk::rs232_write(bool verify)
{
    Debug_printf("bbcRS232Disk::rs232_write(verify=%d)\n", verify);
    
    if (!is_mounted())
    {
        rs232_error();
        return;
    }
    
    // Get sector number from command frame
    uint32_t sector_num = rs232_get_aux32();
    uint16_t sector_size = _media->get_sector_size();
    
    uint8_t *buffer = _media->get_sector_buffer();
    memset(buffer, 0, MEDIA_SECTORBUF_SIZE);
    
    // Receive data from BBC
    uint8_t ck = bus_to_peripheral(buffer, sector_size);
    
    // Verify checksum
    if (ck == RS232Protocol::calculate_checksum(buffer, sector_size))
    {
        uint16_t count = sector_size;
        if (write_sector(sector_num, buffer, &count, verify) == false)
        {
            rs232_complete();
            return;
        }
    }
    
    rs232_error();
}

/**
 * @brief RS232 format command handler
 */
void bbcRS232Disk::rs232_format()
{
    Debug_print("bbcRS232Disk::rs232_format()\n");
    
    if (!is_mounted())
    {
        rs232_error();
        return;
    }
    
    uint16_t responsesize;
    bool err = _media->format(&responsesize);
    
    // Send result to BBC
    bus_to_computer(_media->get_sector_buffer(), responsesize, err);
}

/**
 * @brief RS232 status command handler (virtualDevice interface)
 *
 * Implements virtualDevice pure virtual method.
 */
void bbcRS232Disk::rs232_status()
{
    Debug_print("bbcRS232Disk::rs232_status()\n");
    
    uint8_t status[4];
    get_status(status, sizeof(status));
    
    Debug_printf("Status: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                 status[0], status[1], status[2], status[3]);
    
    bus_to_computer(status, sizeof(status), false);
}

/**
 * @brief RS232 command processor (virtualDevice interface)
 *
 * Implements virtualDevice pure virtual method.
 * This is called by the BBC RS232 bus when a command is received.
 */
void bbcRS232Disk::rs232_process(cmdFrame_t *cmd_ptr)
{
    Debug_print("bbcRS232Disk::rs232_process()\n");
    
    cmdFrame = *cmd_ptr;
    
    // BBC RS232 disk commands (based on standard disk protocol)
    #define BBC_DISKCMD_READ   0x52  // 'R'
    #define BBC_DISKCMD_WRITE  0x57  // 'W'
    #define BBC_DISKCMD_STATUS 0x53  // 'S'
    #define BBC_DISKCMD_FORMAT 0x46  // 'F'
    
    switch (cmdFrame.comnd)
    {
    case BBC_DISKCMD_READ:
        rs232_ack();
        rs232_read();
        break;
        
    case BBC_DISKCMD_WRITE:
        rs232_ack();
        rs232_write(false);
        break;
        
    case BBC_DISKCMD_STATUS:
        rs232_ack();
        rs232_status();
        break;
        
    case BBC_DISKCMD_FORMAT:
        rs232_ack();
        rs232_format();
        break;
        
    default:
        Debug_printf("Unknown command: 0x%02X\n", cmdFrame.comnd);
        rs232_nak();
        break;
    }
}

/**
 * @brief Create a blank BBC disk image
 */
bool bbcRS232Disk::write_blank(fnFile *f, uint16_t sector_size, uint32_t num_sectors, mediatype_t disk_type)
{
    Debug_printf("bbcRS232Disk::write_blank(sector_size=%u, num_sectors=%u, disk_type=0x%04X)\n",
                 sector_size, num_sectors, disk_type);
    
    // BBC disks should be 256 bytes per sector
    if (sector_size != 256)
    {
        Debug_printf("ERROR: BBC disks must have 256 byte sectors (got %u)\n", sector_size);
        return true; // Error
    }
    
    // Determine media type and track count
    // BBC disk formats:
    // - 40 track SSD: 400 sectors = 100KB
    // - 40 track DSD: 800 sectors = 200KB
    // - 80 track SSD: 800 sectors = 200KB
    // - 80 track DSD: 1600 sectors = 400KB
    
    mediatype_t media_type = disk_type;
    uint8_t tracks = 0;
    
    // If media type is explicitly specified, use it
    if (disk_type == MEDIATYPE_SSD || disk_type == MEDIATYPE_DSD)
    {
        // Calculate tracks based on sector count and media type
        if (disk_type == MEDIATYPE_SSD)
        {
            // SSD: 10 sectors per track, single-sided
            tracks = num_sectors / 10;
            if (tracks != 40 && tracks != 80)
            {
                Debug_printf("ERROR: Invalid SSD sector count %u (expected 400 or 800)\n", num_sectors);
                return true; // Error
            }
        }
        else // MEDIATYPE_DSD
        {
            // DSD: 10 sectors per track, double-sided (20 sectors per track total)
            tracks = num_sectors / 20;
            if (tracks != 40 && tracks != 80)
            {
                Debug_printf("ERROR: Invalid DSD sector count %u (expected 800 or 1600)\n", num_sectors);
                return true; // Error
            }
        }
    }
    else
    {
        // Auto-detect based on sector count (with preference for more common formats)
        if (num_sectors == 400)
        {
            // Unambiguous: 40 track SSD
            media_type = MEDIATYPE_SSD;
            tracks = 40;
        }
        else if (num_sectors == 800)
        {
            // Ambiguous: could be 40 track DSD or 80 track SSD
            // Default to 80 track SSD as it's more common
            Debug_print("WARNING: 800 sectors is ambiguous (40T DSD or 80T SSD)\n");
            Debug_print("Defaulting to 80 track SSD. Use explicit disk_type to override.\n");
            media_type = MEDIATYPE_SSD;
            tracks = 80;
        }
        else if (num_sectors == 1600)
        {
            // Unambiguous: 80 track DSD
            media_type = MEDIATYPE_DSD;
            tracks = 80;
        }
        else
        {
            Debug_printf("ERROR: Invalid sector count %u for BBC disk\n", num_sectors);
            return true; // Error
        }
    }
    
    Debug_printf("Creating %s disk with %u tracks\n",
                 (media_type == MEDIATYPE_DSD) ? "DSD" : "SSD", tracks);
    
    // Create the disk using the appropriate media type
    if (media_type == MEDIATYPE_DSD)
    {
        return MediaTypeDSD::create(f, tracks);
    }
    else
    {
        return MediaTypeSSD::create(f, tracks);
    }
}