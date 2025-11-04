#ifdef BUILD_BBC_RS232
#ifndef BBC_RS232_DISK_H
#define BBC_RS232_DISK_H

#include <memory>
#include "../disk_base.h"
#include "../../bus/bbc_rs232/bbc_rs232.h"

// Forward declarations
class MediaTypeSSD;
class MediaTypeDSD;

/**
 * @brief BBC Micro RS232 disk device implementation
 *
 * Handles BBC Micro disk operations over RS232/RS423 serial connection.
 * This class uses multiple inheritance:
 * - DiskBase: Provides disk logic (mount, unmount, media management)
 * - virtualDevice: Provides RS232 bus protocol methods
 * 
 * BBC Micro disk interface characteristics:
 * - DFS (Disk Filing System) format
 * - 256 bytes per sector
 * - 10 sectors per track
 * - 40 or 80 tracks
 * - Single or double-sided
 * 
 * Command set (to be defined based on BBC Micro protocol):
 * - Read sector
 * - Write sector
 * - Format disk
 * - Get status
 */
class bbcRS232Disk : public DiskBase, public virtualDevice
{
private:
    // RS232-specific command handlers
    void rs232_read();
    void rs232_write(bool verify);
    void rs232_format();

protected:
    /**
     * @brief Read a sector from the mounted media
     * 
     * Implements DiskBase pure virtual method for BBC protocol.
     * Handles BBC-specific timing and error codes.
     * 
     * @param sector_num Sector number to read (BBC uses logical sector numbers)
     * @param buffer Buffer to read data into
     * @param count Pointer to count of bytes read
     * @return true if error occurred, false on success
     */
    bool read_sector(uint32_t sector_num, uint8_t *buffer, uint16_t *count) override;
    
    /**
     * @brief Write a sector to the mounted media
     * 
     * Implements DiskBase pure virtual method for BBC protocol.
     * Handles BBC-specific timing and error codes.
     * 
     * @param sector_num Sector number to write
     * @param buffer Buffer containing data to write
     * @param count Pointer to count of bytes to write
     * @param verify Whether to verify the write
     * @return true if error occurred, false on success
     */
    bool write_sector(uint32_t sector_num, const uint8_t *buffer, uint16_t *count, bool verify) override;
    
    /**
     * @brief Get BBC-specific status information
     * 
     * Implements DiskBase pure virtual method.
     * Returns BBC-specific status bytes.
     * 
     * @param status_buffer Buffer to fill with status data
     * @param buffer_size Size of status buffer
     */
    void get_status(uint8_t *status_buffer, size_t buffer_size) override;
    
    /**
     * @brief Implement virtualDevice pure virtual for RS232 status
     *
     * This is called by the RS232 bus to get device status.
     */
    void rs232_status() override;
    
    /**
     * @brief Implement virtualDevice pure virtual for RS232 command processing
     *
     * This is called by the RS232 bus when a command is received.
     *
     * @param cmd_ptr Pointer to command frame
     */
    void rs232_process(cmdFrame_t *cmd_ptr) override;
    
    /**
     * @brief Factory method for BBC media types
     * 
     * Creates MediaType objects for BBC-supported formats:
     * - MEDIATYPE_SSD: Single-sided DFS
     * - MEDIATYPE_DSD: Double-sided DFS
     * - MEDIATYPE_ADFS: ADFS format (future)
     * 
     * @param disk_type The media type to create
     * @return Pointer to new MediaType object or nullptr if unsupported
     */
    std::unique_ptr<MediaTypeBase> create_media_type(mediatype_t disk_type) override;

public:
    /**
     * @brief Constructor
     */
    bbcRS232Disk();
    
    /**
     * @brief Destructor
     */
    ~bbcRS232Disk();
    
    /**
     * @brief Create a blank BBC disk image
     *
     * Overrides DiskBase::write_blank() to support BBC formats.
     *
     * @param f File handle to write to
     * @param sector_size Size of each sector (should be 256 for BBC)
     * @param num_sectors Number of sectors
     * @param disk_type Explicit media type (MEDIATYPE_SSD or MEDIATYPE_DSD)
     * @return true if error occurred, false on success
     */
    bool write_blank(fnFile *f, uint16_t sector_size, uint32_t num_sectors, mediatype_t disk_type = MEDIATYPE_UNKNOWN) override;
};

#endif // BBC_RS232_DISK_H
#endif // BUILD_BBC_RS232
