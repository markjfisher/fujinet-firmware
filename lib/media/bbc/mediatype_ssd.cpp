#include "mediatype_ssd.h"
#include <cstring>

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
MediaTypeSSD::MediaTypeSSD() : MediaTypeBase()
{
    _media_type = MEDIATYPE_SSD;
    _sector_size = 256;
    _sectors_per_track = 10;
    _num_tracks = 40; // Default, will be determined from image size
}

/**
 * @brief Destructor
 */
MediaTypeSSD::~MediaTypeSSD()
{
}

/**
 * @brief Calculate physical sector number
 */
uint32_t MediaTypeSSD::physical_sector(uint8_t track, uint8_t sector)
{
    return (track * _sectors_per_track) + sector;
}

/**
 * @brief Parse the DFS catalog
 */
bool MediaTypeSSD::parse_catalog()
{
    Debug_print("MediaTypeSSD::parse_catalog()\n");
    
    if (_file == nullptr)
        return true; // Error
    
    // Read sector 0 (first catalog sector)
    fnio::fseek(_file, 0, SEEK_SET);
    size_t bytes_read = fnio::fread(_sector_buffer, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read catalog sector 0 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    // Read sector 1 (second catalog sector with disk info)
    fnio::fseek(_file, _sector_size, SEEK_SET);
    uint8_t catalog_sector1[256];
    bytes_read = fnio::fread(catalog_sector1, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read catalog sector 1 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    // Extract disk information from sector 1
    // Bytes 0x00-0x07: Disk title
    char disk_title[9];
    memcpy(disk_title, catalog_sector1, 8);
    disk_title[8] = '\0';
    
    // Byte 0xF6: Number of files * 8
    uint8_t num_files = catalog_sector1[0xF6] / 8;
    
    // Byte 0xF7: Boot option
    uint8_t boot_option = catalog_sector1[0xF7];
    
    // Bytes 0xF8-0xF9: Number of sectors (low, high)
    uint16_t catalog_sectors = catalog_sector1[0xF8] | (catalog_sector1[0xF9] << 8);
    
    Debug_printf("Disk title: '%s'\n", disk_title);
    Debug_printf("Files: %u, Boot: 0x%02X, Sectors: %u\n", 
                 num_files, boot_option, catalog_sectors);
    
    // Validate sector count matches image size
    if (catalog_sectors != _num_sectors)
    {
        Debug_printf("WARNING: Catalog sectors (%u) != image sectors (%u)\n",
                     catalog_sectors, _num_sectors);
    }
    
    return false; // Success
}

/**
 * @brief Mount an SSD disk image
 */
mediatype_t MediaTypeSSD::mount(fnFile *f, uint32_t disksize)
{
    Debug_printf("MediaTypeSSD::mount(size=%u)\n", disksize);
    
    _file = f;
    _image_size = disksize;
    
    // Calculate number of sectors
    _num_sectors = disksize / _sector_size;
    
    // Determine number of tracks from image size
    // 40 tracks = 400 sectors = 102,400 bytes
    // 80 tracks = 800 sectors = 204,800 bytes
    if (_num_sectors == 400)
    {
        _num_tracks = 40;
    }
    else if (_num_sectors == 800)
    {
        _num_tracks = 80;
    }
    else
    {
        Debug_printf("ERROR: Invalid SSD size: %u bytes (%u sectors)\n", 
                     disksize, _num_sectors);
        Debug_print("Expected: 102400 bytes (40 track) or 204800 bytes (80 track)\n");
        return MEDIATYPE_UNKNOWN;
    }
    
    Debug_printf("SSD: %u tracks, %u sectors/track, %u total sectors\n",
                 _num_tracks, _sectors_per_track, _num_sectors);
    
    // Parse the catalog
    if (parse_catalog())
    {
        Debug_print("ERROR: Failed to parse catalog\n");
        return MEDIATYPE_UNKNOWN;
    }
    
    // Check if file is writable
    // For now, assume read-only. This can be enhanced later.
    _readonly = true;
    
    return MEDIATYPE_SSD;
}

/**
 * @brief Read a sector from the image
 */
bool MediaTypeSSD::read(uint32_t sector_num, uint16_t *count)
{
    if (_file == nullptr)
    {
        Debug_print("ERROR: No file mounted\n");
        return true; // Error
    }
    
    if (sector_num >= _num_sectors)
    {
        Debug_printf("ERROR: Sector %u out of range (max %u)\n", 
                     sector_num, _num_sectors - 1);
        return true; // Error
    }
    
    // Calculate file offset
    long offset = sector_num * _sector_size;
    
    // Seek to sector
    if (fnio::fseek(_file, offset, SEEK_SET) != 0)
    {
        Debug_printf("ERROR: Failed to seek to sector %u (offset %ld)\n", 
                     sector_num, offset);
        return true; // Error
    }
    
    // Read sector
    size_t bytes_read = fnio::fread(_sector_buffer, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read sector %u (got %zu bytes)\n", 
                     sector_num, bytes_read);
        return true; // Error
    }
    
    if (count != nullptr)
        *count = _sector_size;
    
    _last_sector = sector_num;
    
    return false; // Success
}

/**
 * @brief Write a sector to the image
 */
bool MediaTypeSSD::write(uint32_t sector_num, bool verify)
{
    if (_file == nullptr)
    {
        Debug_print("ERROR: No file mounted\n");
        return true; // Error
    }
    
    if (_readonly)
    {
        Debug_print("ERROR: Disk is read-only\n");
        return true; // Error
    }
    
    if (sector_num >= _num_sectors)
    {
        Debug_printf("ERROR: Sector %u out of range (max %u)\n", 
                     sector_num, _num_sectors - 1);
        return true; // Error
    }
    
    // Calculate file offset
    long offset = sector_num * _sector_size;
    
    // Seek to sector
    if (fnio::fseek(_file, offset, SEEK_SET) != 0)
    {
        Debug_printf("ERROR: Failed to seek to sector %u (offset %ld)\n", 
                     sector_num, offset);
        return true; // Error
    }
    
    // Write sector
    size_t bytes_written = fnio::fwrite(_sector_buffer, 1, _sector_size, _file);
    
    if (bytes_written != _sector_size)
    {
        Debug_printf("ERROR: Failed to write sector %u (wrote %zu bytes)\n", 
                     sector_num, bytes_written);
        return true; // Error
    }
    
    // Flush to ensure data is written
    fnio::fflush(_file);
    
    // Verify if requested
    if (verify)
    {
        uint8_t verify_buffer[256];
        fnio::fseek(_file, offset, SEEK_SET);
        size_t bytes_read = fnio::fread(verify_buffer, 1, _sector_size, _file);
        
        if (bytes_read != _sector_size)
        {
            Debug_print("ERROR: Verify read failed\n");
            return true; // Error
        }
        
        if (memcmp(_sector_buffer, verify_buffer, _sector_size) != 0)
        {
            Debug_print("ERROR: Verify failed - data mismatch\n");
            return true; // Error
        }
    }
    
    _last_sector = sector_num;
    
    return false; // Success
}

/**
 * @brief Update status information
 */
void MediaTypeSSD::status(uint8_t *status_buffer)
{
    // Status is handled by the disk device class
    // This method can be used to update media-specific status
    
    // For SSD, we could set flags for:
    // - Write protection
    // - Disk geometry
    // But this is typically done at the device level
}

/**
 * @brief Create a blank SSD image
 */
bool MediaTypeSSD::create(fnFile *f, uint8_t num_tracks)
{
    Debug_printf("MediaTypeSSD::create(tracks=%u)\n", num_tracks);
    
    if (f == nullptr)
    {
        Debug_print("ERROR: Invalid file handle\n");
        return true; // Error
    }
    
    if (num_tracks != 40 && num_tracks != 80)
    {
        Debug_printf("ERROR: Invalid track count: %u (must be 40 or 80)\n", num_tracks);
        return true; // Error
    }
    
    uint32_t num_sectors = num_tracks * 10;
    uint32_t image_size = num_sectors * 256;
    
    Debug_printf("Creating SSD: %u sectors, %u bytes\n", num_sectors, image_size);
    
    // Create empty sector buffer
    uint8_t sector[256];
    memset(sector, 0, sizeof(sector));
    
    // Write catalog sectors (0-1) with basic DFS structure
    // Sector 0: Empty file entries
    fnio::fwrite(sector, 1, 256, f);
    
    // Sector 1: Disk info
    memset(sector, 0, sizeof(sector));
    memcpy(sector, "BLANK   ", 8); // Disk title
    sector[0xF6] = 0;  // No files
    sector[0xF7] = 0;  // Boot option
    sector[0xF8] = num_sectors & 0xFF;  // Sectors low
    sector[0xF9] = (num_sectors >> 8) & 0xFF;  // Sectors high
    fnio::fwrite(sector, 1, 256, f);
    
    // Write remaining sectors as empty
    memset(sector, 0, sizeof(sector));
    for (uint32_t i = 2; i < num_sectors; i++)
    {
        fnio::fwrite(sector, 1, 256, f);
    }
    
    fnio::fflush(f);
    
    Debug_print("SSD image created successfully\n");
    return false; // Success
}