#ifndef MEDIATYPE_SSD_H
#define MEDIATYPE_SSD_H

#include "../media_base.h"

/**
 * @brief BBC Micro Single-Sided DFS disk format
 * 
 * Format specifications:
 * - 256 bytes per sector
 * - 10 sectors per track (numbered 0-9)
 * - 40 or 80 tracks (numbered 0-39 or 0-79)
 * - Total capacity: 100KB (40 track) or 200KB (80 track)
 * - File extension: .ssd
 * 
 * DFS Catalog structure (sectors 0-1 of track 0):
 * - Sector 0: First 8 file entries (8 bytes each)
 * - Sector 1: Second 8 file entries + disk information
 * 
 * Sector numbering:
 * - Logical sector = (track * 10) + sector_within_track
 * - Physical offset = logical_sector * 256
 * 
 * File entry format (8 bytes):
 * - Bytes 0-6: Filename (7 chars, space-padded)
 * - Byte 7: Directory character
 * 
 * Disk information (in sector 1):
 * - Bytes 0x00-0x07: Disk title (8 chars)
 * - Byte 0xF6: Number of files * 8
 * - Byte 0xF7: Boot option
 * - Bytes 0xF8-0xF9: Number of sectors (low, high)
 */
class MediaTypeSSD : public MediaTypeBase
{
private:
    uint8_t _num_tracks = 40;        // 40 or 80
    uint8_t _sectors_per_track = 10; // Always 10 for DFS
    
    /**
     * @brief Parse the DFS catalog
     * 
     * Reads sectors 0-1 of track 0 to extract disk information.
     * 
     * @return true if error occurred, false on success
     */
    bool parse_catalog();
    
    /**
     * @brief Calculate physical sector number
     * 
     * @param track Track number (0-39 or 0-79)
     * @param sector Sector within track (0-9)
     * @return Physical sector number
     */
    uint32_t physical_sector(uint8_t track, uint8_t sector);

public:
    /**
     * @brief Constructor
     */
    MediaTypeSSD();
    
    /**
     * @brief Destructor
     */
    ~MediaTypeSSD();
    
    /**
     * @brief Mount an SSD disk image
     * 
     * Validates the image size and parses the catalog.
     * 
     * @param f File handle (already opened)
     * @param disksize Size of the image in bytes
     * @return MEDIATYPE_SSD on success, MEDIATYPE_UNKNOWN on failure
     */
    mediatype_t mount(fnFile *f, uint32_t disksize) override;
    
    /**
     * @brief Read a sector from the image
     * 
     * @param sector_num Logical sector number (0-based)
     * @param count Pointer to receive bytes read (should be 256)
     * @return true if error occurred, false on success
     */
    bool read(uint32_t sector_num, uint16_t *count) override;
    
    /**
     * @brief Write a sector to the image
     * 
     * @param sector_num Logical sector number (0-based)
     * @param verify Whether to verify the write
     * @return true if error occurred, false on success
     */
    bool write(uint32_t sector_num, bool verify) override;
    
    /**
     * @brief Update status information
     * 
     * Sets status bytes for SSD format.
     * 
     * @param status_buffer Buffer to update with status
     */
    void status(uint8_t *status_buffer) override;
    
    /**
     * @brief Get number of tracks
     * @return Number of tracks (40 or 80)
     */
    uint8_t num_tracks() const { return _num_tracks; }
    
    /**
     * @brief Get sectors per track
     * @return Sectors per track (always 10)
     */
    uint8_t sectors_per_track() const { return _sectors_per_track; }
    
    /**
     * @brief Create a blank SSD image
     * 
     * Creates a formatted SSD disk image with empty catalog.
     * 
     * @param f File handle to write to
     * @param num_tracks Number of tracks (40 or 80)
     * @return true if error occurred, false on success
     */
    static bool create(fnFile *f, uint8_t num_tracks = 40);
};

#endif // MEDIATYPE_SSD_H