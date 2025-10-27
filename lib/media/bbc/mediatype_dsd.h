#ifndef MEDIATYPE_DSD_H
#define MEDIATYPE_DSD_H

#include "../media_base.h"

/**
 * @brief BBC Micro Double-Sided DFS disk format
 * 
 * Format specifications:
 * - 256 bytes per sector
 * - 10 sectors per track (numbered 0-9)
 * - 40 or 80 tracks per side (numbered 0-39 or 0-79)
 * - 2 sides (0 and 1)
 * - Total capacity: 200KB (40 track) or 400KB (80 track)
 * - File extension: .dsd
 * 
 * DFS Catalog structure:
 * - Side 0, Track 0, Sectors 0-1: Catalog for side 0
 * - Side 1, Track 0, Sectors 0-1: Catalog for side 1
 * 
 * Sector layout in file:
 * - Sectors are interleaved by side
 * - Track 0: Side 0 sectors 0-9, then Side 1 sectors 0-9
 * - Track 1: Side 0 sectors 0-9, then Side 1 sectors 0-9
 * - etc.
 * 
 * Physical sector calculation:
 * - physical_sector = (track * sectors_per_track * 2) + (side * sectors_per_track) + sector
 * 
 * Example for Track 1, Side 1, Sector 5:
 * - physical_sector = (1 * 10 * 2) + (1 * 10) + 5 = 20 + 10 + 5 = 35
 */
class MediaTypeDSD : public MediaTypeBase
{
private:
    uint8_t _num_tracks = 40;        // 40 or 80 per side
    uint8_t _sectors_per_track = 10; // Always 10 for DFS
    uint8_t _num_sides = 2;          // Always 2 for DSD
    
    /**
     * @brief Parse the DFS catalogs (one per side)
     * 
     * Reads sectors 0-1 of track 0 for both sides to extract disk information.
     * 
     * @return true if error occurred, false on success
     */
    bool parse_catalog();
    
    /**
     * @brief Calculate physical sector number from track/side/sector
     * 
     * DSD files interleave sectors by side within each track.
     * 
     * @param track Track number (0-39 or 0-79)
     * @param side Side number (0 or 1)
     * @param sector Sector within track (0-9)
     * @return Physical sector number in the file
     */
    uint32_t physical_sector(uint8_t track, uint8_t side, uint8_t sector);
    
    /**
     * @brief Convert logical sector to track/side/sector
     * 
     * Logical sectors are numbered sequentially across both sides.
     * 
     * @param logical_sector Logical sector number
     * @param track Output: track number
     * @param side Output: side number
     * @param sector Output: sector within track
     */
    void logical_to_physical(uint32_t logical_sector, uint8_t *track, uint8_t *side, uint8_t *sector);

public:
    /**
     * @brief Constructor
     */
    MediaTypeDSD();
    
    /**
     * @brief Destructor
     */
    ~MediaTypeDSD();
    
    /**
     * @brief Mount a DSD disk image
     * 
     * Validates the image size and parses the catalogs.
     * 
     * @param f File handle (already opened)
     * @param disksize Size of the image in bytes
     * @return MEDIATYPE_DSD on success, MEDIATYPE_UNKNOWN on failure
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
     * Sets status bytes for DSD format.
     * 
     * @param status_buffer Buffer to update with status
     */
    void status(uint8_t *status_buffer) override;
    
    /**
     * @brief Get number of tracks per side
     * @return Number of tracks per side (40 or 80)
     */
    uint8_t num_tracks() const { return _num_tracks; }
    
    /**
     * @brief Get sectors per track
     * @return Sectors per track (always 10)
     */
    uint8_t sectors_per_track() const { return _sectors_per_track; }
    
    /**
     * @brief Get number of sides
     * @return Number of sides (always 2)
     */
    uint8_t num_sides() const { return _num_sides; }
    
    /**
     * @brief Create a blank DSD image
     * 
     * Creates a formatted DSD disk image with empty catalogs on both sides.
     * 
     * @param f File handle to write to
     * @param num_tracks Number of tracks per side (40 or 80)
     * @return true if error occurred, false on success
     */
    static bool create(fnFile *f, uint8_t num_tracks = 40);
};

#endif // MEDIATYPE_DSD_H