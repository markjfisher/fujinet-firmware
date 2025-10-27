#include "mediatype_dsd.h"
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
MediaTypeDSD::MediaTypeDSD() : MediaTypeBase()
{
    _media_type = MEDIATYPE_DSD;
    _sector_size = 256;
    _sectors_per_track = 10;
    _num_sides = 2;
    _num_tracks = 40; // Default, will be determined from image size
}

/**
 * @brief Destructor
 */
MediaTypeDSD::~MediaTypeDSD()
{
}

/**
 * @brief Calculate physical sector number from track/side/sector
 * 
 * DSD files interleave sectors by side within each track.
 */
uint32_t MediaTypeDSD::physical_sector(uint8_t track, uint8_t side, uint8_t sector)
{
    return (track * _sectors_per_track * _num_sides) + (side * _sectors_per_track) + sector;
}

/**
 * @brief Convert logical sector to track/side/sector
 */
void MediaTypeDSD::logical_to_physical(uint32_t logical_sector, uint8_t *track, uint8_t *side, uint8_t *sector)
{
    // Logical sectors are numbered sequentially across both sides
    // Side 0: sectors 0-9, 20-29, 40-49, etc.
    // Side 1: sectors 10-19, 30-39, 50-59, etc.
    
    uint32_t sectors_per_track_both_sides = _sectors_per_track * _num_sides;
    *track = logical_sector / sectors_per_track_both_sides;
    uint32_t sector_in_track = logical_sector % sectors_per_track_both_sides;
    *side = sector_in_track / _sectors_per_track;
    *sector = sector_in_track % _sectors_per_track;
}

/**
 * @brief Parse the DFS catalogs
 */
bool MediaTypeDSD::parse_catalog()
{
    Debug_print("MediaTypeDSD::parse_catalog()\n");
    
    if (_file == nullptr)
        return true; // Error
    
    // Parse catalog for side 0 (sectors 0-1)
    fnio::fseek(_file, 0, SEEK_SET);
    uint8_t catalog_s0[256];
    size_t bytes_read = fnio::fread(catalog_s0, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read side 0 catalog sector 0 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    fnio::fseek(_file, _sector_size, SEEK_SET);
    uint8_t catalog_s1[256];
    bytes_read = fnio::fread(catalog_s1, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read side 0 catalog sector 1 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    // Extract disk information from side 0, sector 1
    char disk_title[9];
    memcpy(disk_title, catalog_s1, 8);
    disk_title[8] = '\0';
    
    uint8_t num_files = catalog_s1[0xF6] / 8;
    uint8_t boot_option = catalog_s1[0xF7];
    uint16_t catalog_sectors = catalog_s1[0xF8] | (catalog_s1[0xF9] << 8);
    
    Debug_printf("Side 0 - Title: '%s', Files: %u, Boot: 0x%02X, Sectors: %u\n", 
                 disk_title, num_files, boot_option, catalog_sectors);
    
    // Parse catalog for side 1 (physical sectors 10-11)
    uint32_t side1_catalog_offset = physical_sector(0, 1, 0) * _sector_size;
    fnio::fseek(_file, side1_catalog_offset, SEEK_SET);
    bytes_read = fnio::fread(catalog_s0, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read side 1 catalog sector 0 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    fnio::fseek(_file, side1_catalog_offset + _sector_size, SEEK_SET);
    bytes_read = fnio::fread(catalog_s1, 1, _sector_size, _file);
    
    if (bytes_read != _sector_size)
    {
        Debug_printf("ERROR: Failed to read side 1 catalog sector 1 (got %zu bytes)\n", bytes_read);
        return true; // Error
    }
    
    // Extract disk information from side 1, sector 1
    memcpy(disk_title, catalog_s1, 8);
    disk_title[8] = '\0';
    
    num_files = catalog_s1[0xF6] / 8;
    boot_option = catalog_s1[0xF7];
    catalog_sectors = catalog_s1[0xF8] | (catalog_s1[0xF9] << 0x08);
    
    Debug_printf("Side 1 - Title: '%s', Files: %u, Boot: 0x%02X, Sectors: %u\n", 
                 disk_title, num_files, boot_option, catalog_sectors);
    
    return false; // Success
}

/**
 * @brief Mount a DSD disk image
 */
mediatype_t MediaTypeDSD::mount(fnFile *f, uint32_t disksize)
{
    Debug_printf("MediaTypeDSD::mount(size=%u)\n", disksize);
    
    _file = f;
    _image_size = disksize;
    
    // Calculate number of sectors
    _num_sectors = disksize / _sector_size;
    
    // Determine number of tracks from image size
    // 40 tracks * 2 sides = 800 sectors = 204,800 bytes
    // 80 tracks * 2 sides = 1600 sectors = 409,600 bytes
    uint32_t tracks_both_sides = _num_sectors / _sectors_per_track;
    _num_tracks = tracks_both_sides / _num_sides;
    
    if (_num_tracks != 40 && _num_tracks != 80)
    {
        Debug_printf("ERROR: Invalid DSD size: %u bytes (%u sectors, %u tracks)\n", 
                     disksize, _num_sectors, _num_tracks);
        Debug_print("Expected: 204800 bytes (40 track) or 409600 bytes (80 track)\n");
        return MEDIATYPE_UNKNOWN;
    }
    
    Debug_printf("DSD: %u tracks/side, %u sides, %u sectors/track, %u total sectors\n",
                 _num_tracks, _num_sides, _sectors_per_track, _num_sectors);
    
    // Parse the catalogs
    if (parse_catalog())
    {
        Debug_print("ERROR: Failed to parse catalogs\n");
        return MEDIATYPE_UNKNOWN;
    }
    
    // Check if file is writable
    // For now, assume read-only. This can be enhanced later.
    _readonly = true;
    
    return MEDIATYPE_DSD;
}

/**
 * @brief Read a sector from the image
 */
bool MediaTypeDSD::read(uint32_t sector_num, uint16_t *count)
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
    
    // Convert logical sector to track/side/sector
    uint8_t track, side, sector;
    logical_to_physical(sector_num, &track, &side, &sector);
    
    // Calculate physical sector in file
    uint32_t phys_sector = physical_sector(track, side, sector);
    
    Debug_printf("Read: logical=%u -> track=%u, side=%u, sector=%u -> physical=%u\n",
                 sector_num, track, side, sector, phys_sector);
    
    // Calculate file offset
    long offset = phys_sector * _sector_size;
    
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
bool MediaTypeDSD::write(uint32_t sector_num, bool verify)
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
    
    // Convert logical sector to track/side/sector
    uint8_t track, side, sector;
    logical_to_physical(sector_num, &track, &side, &sector);
    
    // Calculate physical sector in file
    uint32_t phys_sector = physical_sector(track, side, sector);
    
    Debug_printf("Write: logical=%u -> track=%u, side=%u, sector=%u -> physical=%u\n",
                 sector_num, track, side, sector, phys_sector);
    
    // Calculate file offset
    long offset = phys_sector * _sector_size;
    
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
void MediaTypeDSD::status(uint8_t *status_buffer)
{
    // Status is handled by the disk device class
    // This method can be used to update media-specific status
}

/**
 * @brief Create a blank DSD image
 */
bool MediaTypeDSD::create(fnFile *f, uint8_t num_tracks)
{
    Debug_printf("MediaTypeDSD::create(tracks=%u)\n", num_tracks);
    
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
    
    uint32_t num_sectors = num_tracks * 10 * 2; // tracks * sectors/track * sides
    uint32_t image_size = num_sectors * 256;
    
    Debug_printf("Creating DSD: %u sectors, %u bytes\n", num_sectors, image_size);
    
    // Create empty sector buffer
    uint8_t sector[256];
    memset(sector, 0, sizeof(sector));
    
    // Write catalog for side 0 (sectors 0-1)
    // Sector 0: Empty file entries
    fnio::fwrite(sector, 1, 256, f);
    
    // Sector 1: Disk info for side 0
    memset(sector, 0, sizeof(sector));
    memcpy(sector, "BLANK S0", 8); // Disk title
    sector[0xF6] = 0;  // No files
    sector[0xF7] = 0;  // Boot option
    // Note: Sector count in catalog is per side
    uint16_t sectors_per_side = (num_tracks * 10);
    sector[0xF8] = sectors_per_side & 0xFF;  // Sectors low
    sector[0xF9] = (sectors_per_side >> 8) & 0xFF;  // Sectors high
    fnio::fwrite(sector, 1, 256, f);
    
    // Write remaining sectors of side 0, track 0 (sectors 2-9)
    memset(sector, 0, sizeof(sector));
    for (int i = 2; i < 10; i++)
    {
        fnio::fwrite(sector, 1, 256, f);
    }
    
    // Write catalog for side 1 (sectors 10-11 in physical layout)
    // Sector 0: Empty file entries
    fnio::fwrite(sector, 1, 256, f);
    
    // Sector 1: Disk info for side 1
    memset(sector, 0, sizeof(sector));
    memcpy(sector, "BLANK S1", 8); // Disk title
    sector[0xF6] = 0;  // No files
    sector[0xF7] = 0;  // Boot option
    sector[0xF8] = sectors_per_side & 0xFF;  // Sectors low
    sector[0xF9] = (sectors_per_side >> 8) & 0xFF;  // Sectors high
    fnio::fwrite(sector, 1, 256, f);
    
    // Write remaining sectors of side 1, track 0 (sectors 12-19)
    memset(sector, 0, sizeof(sector));
    for (int i = 2; i < 10; i++)
    {
        fnio::fwrite(sector, 1, 256, f);
    }
    
    // Write remaining tracks (both sides interleaved)
    for (uint32_t track = 1; track < num_tracks; track++)
    {
        // Write 10 sectors for side 0
        for (int i = 0; i < 10; i++)
        {
            fnio::fwrite(sector, 1, 256, f);
        }
        // Write 10 sectors for side 1
        for (int i = 0; i < 10; i++)
        {
            fnio::fwrite(sector, 1, 256, f);
        }
    }
    
    fnio::fflush(f);
    
    Debug_print("DSD image created successfully\n");
    return false; // Success
}