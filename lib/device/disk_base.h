#ifndef DISK_BASE_H
#define DISK_BASE_H

#include <stdint.h>
#include <time.h>
#include "../FileSystem/fnio.h"
#include "../media/media_base.h"
#include "../fuji/fujiHost.h"

/**
 * @brief Base class for all disk device implementations
 * 
 * This class provides common functionality for disk devices across
 * all bus types (SIO, IWM, RS232, BBC, etc.). It manages the MediaType
 * lifecycle and provides a clean interface for subclasses to implement
 * bus-specific protocol handling.
 * 
 * Design principles:
 * - Pure virtual methods for platform-specific I/O operations
 * - Common mount/unmount logic shared across all platforms
 * - Factory pattern for creating platform-specific MediaType objects
 * - Clear separation between protocol handling and media management
 */
class DiskBase
{
protected:
    // Media management
    MediaTypeBase *_media = nullptr;
    
    // Device state
    bool _device_active = false;
    bool _readonly = false;
    time_t _mount_time = 0;

    bool _switched = false;
    
    // Host reference (for network-mounted images)
    fujiHost *_host = nullptr;
    
    /**
     * @brief Pure virtual methods that subclasses MUST implement
     * These handle the actual sector I/O with bus-specific logic
     */
    
    /**
     * @brief Read a sector from the mounted media
     * 
     * Subclasses implement this to:
     * - Handle bus-specific protocol (ACK/NAK, timing)
     * - Call _media->read() to get the data
     * - Send data to the computer via the bus
     * - Handle errors appropriately
     * 
     * @param sector_num Sector number to read (0-based or 1-based depending on platform)
     * @param buffer Buffer to read data into (usually _media->get_sector_buffer())
     * @param count Pointer to count of bytes read
     * @return true if error occurred, false on success
     */
    virtual bool read_sector(uint32_t sector_num, uint8_t *buffer, uint16_t *count) = 0;
    
    /**
     * @brief Write a sector to the mounted media
     * 
     * Subclasses implement this to:
     * - Receive data from the computer via the bus
     * - Call _media->write() to write the data
     * - Handle verification if requested
     * - Handle errors appropriately
     * 
     * @param sector_num Sector number to write
     * @param buffer Buffer containing data to write
     * @param count Pointer to count of bytes to write
     * @param verify Whether to verify the write
     * @return true if error occurred, false on success
     */
    virtual bool write_sector(uint32_t sector_num, const uint8_t *buffer, uint16_t *count, bool verify) = 0;
    
    /**
     * @brief Get platform-specific status information
     * 
     * Subclasses implement this to fill in platform-specific
     * status bytes (drive status, controller status, etc.)
     * 
     * @param status_buffer Buffer to fill with status data
     * @param buffer_size Size of status buffer
     */
    virtual void get_status(uint8_t *status_buffer, size_t buffer_size) = 0;
    
    /**
     * @brief Factory method to create appropriate MediaType for platform
     * 
     * Subclasses override this to create platform-specific MediaType objects.
     * For example, sioDisk creates MediaTypeATR, iwmDisk creates MediaTypePO,
     * bbcDisk creates MediaTypeSSD, etc.
     * 
     * @param disk_type The media type to create
     * @return Pointer to new MediaType object or nullptr if unsupported
     */
    virtual MediaTypeBase* create_media_type(mediatype_t disk_type) = 0;
    
    /**
     * @brief Discover media type from filename
     * 
     * Default implementation uses MediaTypeBase::discover_mediatype().
     * Subclasses can override for platform-specific detection logic.
     * 
     * @param filename Name of the file
     * @return Detected media type
     */
    virtual mediatype_t discover_media_type(const char *filename);

public:
    /**
     * @brief Constructor
     */
    DiskBase();
    
    /**
     * @brief Virtual destructor to ensure proper cleanup
     */
    virtual ~DiskBase();
    
    /**
     * @brief Mount a disk image file
     * 
     * This method:
     * 1. Unmounts any existing disk
     * 2. Discovers the media type (if not explicitly provided)
     * 3. Creates the appropriate MediaType object via factory
     * 4. Calls MediaType::mount() to validate and set up the image
     * 5. Updates device state
     * 
     * @param f File handle to mount (already opened by caller)
     * @param filename Name of the file (for type detection)
     * @param disksize Size of the disk image in bytes
     * @param disk_type Explicit media type (or MEDIATYPE_UNKNOWN for auto-detect)
     * @return The detected/mounted media type, or MEDIATYPE_UNKNOWN on failure
     */
    mediatype_t mount(fnFile *f, const char *filename, uint32_t disksize, 
                      mediatype_t disk_type = MEDIATYPE_UNKNOWN);
    
    /**
     * @brief Unmount the current disk image
     * 
     * This method:
     * 1. Calls MediaType::unmount() to close the file
     * 2. Deletes the MediaType object
     * 3. Resets device state
     */
    void unmount();
    
    /**
     * @brief Create a blank disk image
     * 
     * Default implementation returns error. Subclasses can override
     * to support creating blank images in platform-specific formats.
     * 
     * @param f File handle to write to
     * @param sector_size Size of each sector
     * @param num_sectors Number of sectors
     * @return true if error occurred, false on success
     */
    virtual bool write_blank(fnFile *f, uint16_t sector_size, uint16_t num_sectors);
    
    /**
     * @brief Get the current media type
     * @return Current media type or MEDIATYPE_UNKNOWN if not mounted
     */
    mediatype_t get_media_type() const;
    
    /**
     * @brief Check if a disk is currently mounted
     * @return true if disk is mounted
     */
    bool is_mounted() const { return _media != nullptr; }
    
    /**
     * @brief Check if device is active
     * 
     * Device becomes active after successful mount. Some platforms
     * may have additional activation logic.
     * 
     * @return true if device is active
     */
    bool is_active() const { return _device_active; }
    
    /**
     * @brief Set device active state
     * @param active New active state
     */
    void set_active(bool active) { _device_active = active; }
    
    /**
     * @brief Check if disk is read-only
     * @return true if read-only
     */
    bool is_readonly() const { return _readonly; }
    
    /**
     * @brief Set read-only state
     * @param readonly New read-only state
     */
    void set_readonly(bool readonly) { _readonly = readonly; }
    
    /**
     * @brief Set the host for network-mounted images
     * @param host Pointer to fujiHost
     */
    void set_host(fujiHost *host) { _host = host; }
    
    /**
     * @brief Get host reference
     * @return Pointer to fujiHost or nullptr
     */
    fujiHost* get_host() const { return _host; }
    
    /**
     * @brief Get mount time
     * @return Time when disk was mounted (0 if not mounted)
     */
    time_t get_mount_time() const { return _mount_time; }
    
    /**
     * @brief Get number of sectors (if mounted)
     * @return Number of sectors or 0 if not mounted
     */
    uint32_t get_num_sectors() const;
    
    /**
     * @brief Get sector size (if mounted)
     * @return Sector size or 0 if not mounted
     */
    uint16_t get_sector_size() const;
    
    /**
     * @brief Get media object (for advanced operations)
     * @return Pointer to MediaTypeBase or nullptr
     */
    MediaTypeBase* get_media() const { return _media; }

    /**
     * @brief Get Disk switched flag
     * @return switched flag
     */
    bool get_switched() { return _switched; }

    /**
     * @brief Set Disk switched flag
     */
    void set_switched(bool switched) { _switched = switched; }


};

#endif // DISK_BASE_H