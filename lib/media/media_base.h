#ifndef MEDIA_BASE_H
#define MEDIA_BASE_H

#include <stdint.h>
#include <cstring>
#include "fnio.h"
#include "../fuji/fujiHost.h"

#define INVALID_SECTOR_VALUE 65536
#define MEDIA_SECTORBUF_SIZE 512

/**
 * @brief Common media type enumeration
 * Platform-specific types start at 0x1000 to avoid conflicts
 */
enum mediatype_t
{
    MEDIATYPE_UNKNOWN = 0,
    
    // Atari types (0x0100-0x01FF)
    MEDIATYPE_ATR   = 0x0100,
    MEDIATYPE_ATX   = 0x0101,
    MEDIATYPE_XEX   = 0x0102,
    MEDIATYPE_CAS   = 0x0103,
    MEDIATYPE_WAV   = 0x0104,

    // Apple types (0x0200-0x02FF)
    MEDIATYPE_DO    = 0x0200,
    MEDIATYPE_DSK   = 0x0201,
    MEDIATYPE_PO    = 0x0202,
    MEDIATYPE_WOZ   = 0x0203,
    
    // BBC types (0x0300-0x03FF)
    MEDIATYPE_SSD   = 0x0300,  // Single-sided DFS
    MEDIATYPE_DSD   = 0x0301,  // Double-sided DFS
    MEDIATYPE_ADFS  = 0x0302, // ADFS format
    
    // Generic types (0xFF00-0xFFFF)
    MEDIATYPE_IMG   = 0xFF00,
    MEDIATYPE_RAW   = 0xFF01
};

/**
 * @brief Base class for all media format implementations
 * 
 * This class defines the interface for reading/writing disk images
 * in various formats. Subclasses implement format-specific logic.
 * 
 * Design principles:
 * - Pure virtual methods force subclasses to implement critical operations
 * - Sector buffer is owned by this class to reduce allocation overhead
 * - File handle management is centralized here
 * - Common utilities are provided for subclasses
 */
class MediaTypeBase
{
protected:
    // File handle for the disk image
    fnFile *_file = nullptr;
    
    // Media geometry
    uint32_t _image_size = 0;
    uint32_t _num_sectors = 0;
    uint16_t _sector_size = 256;  // Default to 256 bytes (common for 8-bit systems)
    
    // State tracking
    int32_t _last_sector = INVALID_SECTOR_VALUE;
    bool _readonly = true;
    
    // Host reference (for network-mounted images)
    fujiHost *_host = nullptr;
    
    // Sector buffer - owned by this class
    uint8_t _sector_buffer[MEDIA_SECTORBUF_SIZE];

public:
    // Media type identifier
    mediatype_t _media_type = MEDIATYPE_UNKNOWN;
    
    // Filename for reference and debugging
    char _filename[256];
    
    /**
     * @brief Constructor
     */
    MediaTypeBase();
    
    /**
     * @brief Virtual destructor to ensure proper cleanup
     */
    virtual ~MediaTypeBase();
    
    /**
     * @brief Mount a disk image file
     * 
     * Subclasses must implement this to:
     * - Validate the image format
     * - Parse any headers
     * - Set up geometry (_num_sectors, _sector_size)
     * - Return the actual media type
     * 
     * @param f File handle (already opened)
     * @param disksize Size of the image in bytes
     * @return Media type on success, MEDIATYPE_UNKNOWN on failure
     */
    virtual mediatype_t mount(fnFile *f, uint32_t disksize) = 0;
    
    /**
     * @brief Unmount the disk image
     * 
     * Default implementation closes the file and resets state.
     * Subclasses can override for format-specific cleanup.
     */
    virtual void unmount();
    
    /**
     * @brief Read a sector from the image
     * 
     * Subclasses must implement this to:
     * - Calculate the file offset for the sector
     * - Read data into _sector_buffer
     * - Set *count to bytes read
     * - Return error status
     * 
     * @param sector_num Sector number to read (0-based or 1-based depending on format)
     * @param count Pointer to receive bytes read
     * @return true if error occurred, false on success
     */
    virtual bool read(uint32_t sector_num, uint16_t *count) = 0;
    
    /**
     * @brief Write a sector to the image
     * 
     * Subclasses must implement this to:
     * - Calculate the file offset for the sector
     * - Write data from _sector_buffer
     * - Optionally verify the write
     * - Return error status
     * 
     * @param sector_num Sector number to write
     * @param verify Whether to verify the write (read back and compare)
     * @return true if error occurred, false on success
     */
    virtual bool write(uint32_t sector_num, bool verify) = 0;
    
    /**
     * @brief Format the disk image
     * 
     * Default implementation returns error. Subclasses override
     * to implement format-specific formatting.
     * 
     * @param response_size Pointer to receive response size
     * @return true if error occurred, false on success
     */
    virtual bool format(uint16_t *response_size);
    
    /**
     * @brief Get sector size for a given sector
     * 
     * Some formats have variable sector sizes (e.g., Atari ATR has
     * 128-byte boot sectors). Default returns _sector_size.
     * 
     * @param sector_num Sector number
     * @return Size of the sector in bytes
     */
    virtual uint16_t sector_size(uint32_t sector_num);
    
    /**
     * @brief Update status information
     * 
     * Subclasses must implement this to update platform-specific
     * status bytes (e.g., write protect, disk geometry).
     * 
     * @param status_buffer Buffer to update with status
     */
    virtual void status(uint8_t *status_buffer) = 0;
    
    /**
     * @brief Get number of sectors
     * @return Total sectors in image
     */
    uint32_t num_sectors() const { return _num_sectors; }
    
    /**
     * @brief Get default sector size
     * @return Default sector size in bytes
     */
    uint16_t get_sector_size() const { return _sector_size; }
    
    /**
     * @brief Check if media is read-only
     * @return true if read-only
     */
    bool is_readonly() const { return _readonly; }
    
    /**
     * @brief Get sector buffer
     * @return Pointer to sector buffer
     */
    uint8_t* get_sector_buffer() { return _sector_buffer; }
    
    /**
     * @brief Set host reference for network-mounted images
     * @param host Pointer to fujiHost
     */
    void set_host(fujiHost *host) { _host = host; }
    
    /**
     * @brief Get host reference
     * @return Pointer to fujiHost or nullptr
     */
    fujiHost* get_host() const { return _host; }
    
    /**
     * @brief Static method to discover media type from filename extension
     * 
     * This is a helper that can be overridden by platform-specific
     * implementations. Default implementation checks common extensions.
     * 
     * @param filename Name of the file
     * @return Detected media type or MEDIATYPE_UNKNOWN
     */
    static mediatype_t discover_mediatype(const char *filename);

    /**
     * @brief Static method to convert new media types to single byte that matches the old media type
     * 
     * @param filename New media type
     * @return Old media type enum value as single byte
     */
    static uint8_t to_old_type(mediatype_t t);

protected:
    /**
     * @brief Helper to get file extension
     * @param filename Filename to parse
     * @return Pointer to extension (without dot) or nullptr
     */
    static const char* get_extension(const char *filename);
    
    /**
     * @brief Helper to compare extensions (case-insensitive)
     * @param ext1 First extension
     * @param ext2 Second extension
     * @return true if extensions match
     */
    static bool ext_matches(const char *ext1, const char *ext2);
};

#endif // MEDIA_BASE_H