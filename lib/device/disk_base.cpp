#include "disk_base.h"
#include <cstring>

// Include debug if available
#ifdef ESP_PLATFORM
#include "../include/debug.h"
#else
#include <cstdio>
#define Debug_printf printf
#define Debug_print(x) printf("%s", x)
#endif

/**
 * @brief Constructor
 */
DiskBase::DiskBase()
{
    _media = nullptr;
    _device_active = false;
    _readonly = false;
    _mount_time = 0;
    _host = nullptr;
}

/**
 * @brief Destructor
 */
DiskBase::~DiskBase()
{
    unmount();
}

/**
 * @brief Discover media type from filename
 * 
 * Default implementation uses MediaTypeBase::discover_mediatype().
 * Subclasses can override for platform-specific detection logic.
 */
mediatype_t DiskBase::discover_media_type(const char *filename)
{
    return MediaTypeBase::discover_mediatype(filename);
}

/**
 * @brief Mount a disk image file
 * 
 * This method provides common mounting logic shared across all platforms.
 */
mediatype_t DiskBase::mount(fnFile *f, const char *filename, uint32_t disksize, 
                            mediatype_t disk_type)
{
    Debug_print("DiskBase::mount()\n");
    
    // Unmount any existing disk
    if (_media != nullptr)
    {
        Debug_print("Unmounting existing disk\n");
        unmount();
    }
    
    // Discover media type if not explicitly provided
    if (disk_type == MEDIATYPE_UNKNOWN && filename != nullptr)
    {
        disk_type = discover_media_type(filename);
        Debug_printf("Discovered media type: 0x%04X\n", disk_type);
    }
    
    // If still unknown, we can't proceed
    if (disk_type == MEDIATYPE_UNKNOWN)
    {
        Debug_print("ERROR: Unknown media type\n");
        return MEDIATYPE_UNKNOWN;
    }
    
    // Create the appropriate MediaType object via factory
    _media = create_media_type(disk_type);
    if (_media == nullptr)
    {
        Debug_printf("ERROR: Failed to create media type 0x%04X\n", disk_type);
        return MEDIATYPE_UNKNOWN;
    }
    
    // Set up the media object
    if (_host != nullptr)
    {
        _media->set_host(_host);
    }
    
    if (filename != nullptr)
    {
        strncpy(_media->_filename, filename, sizeof(_media->_filename) - 1);
        _media->_filename[sizeof(_media->_filename) - 1] = '\0';
    }
    
    // Call MediaType::mount() to validate and set up the image
    mediatype_t result = _media->mount(f, disksize);
    
    if (result == MEDIATYPE_UNKNOWN)
    {
        Debug_print("ERROR: MediaType::mount() failed\n");
        delete _media;
        _media = nullptr;
        return MEDIATYPE_UNKNOWN;
    }
    
    // Update device state
    _device_active = true;
    _readonly = _media->is_readonly();
    _mount_time = time(nullptr);
    
    Debug_printf("Mount successful: %s (%u sectors, %u bytes/sector)\n",
                 filename ? filename : "unknown",
                 _media->num_sectors(),
                 _media->get_sector_size());
    
    return result;
}

/**
 * @brief Unmount the current disk image
 */
void DiskBase::unmount()
{
    Debug_print("DiskBase::unmount()\n");
    
    if (_media != nullptr)
    {
        _media->unmount();
        delete _media;
        _media = nullptr;
    }
    
    _device_active = false;
    _readonly = false;
    _mount_time = 0;
}

/**
 * @brief Create a blank disk image
 * 
 * Default implementation returns error. Subclasses can override.
 */
bool DiskBase::write_blank(fnFile *f, uint16_t sector_size, uint16_t num_sectors)
{
    Debug_print("DiskBase::write_blank() - Not implemented\n");
    return true; // Error - not implemented
}

/**
 * @brief Get the current media type
 */
mediatype_t DiskBase::get_media_type() const
{
    if (_media == nullptr)
        return MEDIATYPE_UNKNOWN;
    
    return _media->_media_type;
}

/**
 * @brief Get number of sectors (if mounted)
 */
uint32_t DiskBase::get_num_sectors() const
{
    if (_media == nullptr)
        return 0;
    
    return _media->num_sectors();
}

/**
 * @brief Get sector size (if mounted)
 */
uint16_t DiskBase::get_sector_size() const
{
    if (_media == nullptr)
        return 0;
    
    return _media->get_sector_size();
}