#include "media_base.h"
#include <cstring>
#include <cctype>

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
MediaTypeBase::MediaTypeBase()
{
    _file = nullptr;
    _image_size = 0;
    _num_sectors = 0;
    _sector_size = 256;
    _last_sector = INVALID_SECTOR_VALUE;
    _host = nullptr;
    _media_type = MEDIATYPE_UNKNOWN;
    
    memset(_sector_buffer, 0, MEDIA_SECTORBUF_SIZE);
    memset(_filename, 0, sizeof(_filename));
}

/**
 * @brief Destructor
 */
MediaTypeBase::~MediaTypeBase()
{
    unmount();
}

/**
 * @brief Unmount the disk image
 * 
 * Default implementation closes the file and resets state.
 * Subclasses can override for format-specific cleanup.
 */
void MediaTypeBase::unmount()
{
    if (_file != nullptr)
    {
        fnio::fclose(_file);
        _file = nullptr;
    }
    
    _image_size = 0;
    _num_sectors = 0;
    _last_sector = INVALID_SECTOR_VALUE;
    _media_type = MEDIATYPE_UNKNOWN;
    
    memset(_sector_buffer, 0, MEDIA_SECTORBUF_SIZE);
}

/**
 * @brief Format the disk image
 * 
 * Default implementation returns error. Subclasses override
 * to implement format-specific formatting.
 */
bool MediaTypeBase::format(uint16_t *response_size)
{
    Debug_print("MediaTypeBase::format() - Not implemented\n");
    if (response_size != nullptr)
        *response_size = 0;
    return true; // Error - not implemented
}

/**
 * @brief Get sector size for a given sector
 * 
 * Some formats have variable sector sizes. Default returns _sector_size.
 */
uint16_t MediaTypeBase::sector_size(uint32_t sector_num)
{
    return _sector_size;
}

/**
 * @brief Helper to get file extension
 */
const char* MediaTypeBase::get_extension(const char *filename)
{
    if (filename == nullptr)
        return nullptr;
    
    const char *dot = strrchr(filename, '.');
    if (dot == nullptr || dot == filename)
        return nullptr;
    
    return dot + 1; // Skip the dot
}

/**
 * @brief Helper to compare extensions (case-insensitive)
 */
bool MediaTypeBase::ext_matches(const char *ext1, const char *ext2)
{
    if (ext1 == nullptr || ext2 == nullptr)
        return false;
    
    while (*ext1 && *ext2)
    {
        if (tolower(*ext1) != tolower(*ext2))
            return false;
        ext1++;
        ext2++;
    }
    
    return (*ext1 == '\0' && *ext2 == '\0');
}

uint8_t MediaTypeBase::to_old_type(mediatype_t t)
{
    // The new media types are 16 bit and separated by platform, but the disks are in the same order as the old media types.
    // We just return the lower byte + 1, as the new types are indexed at 0, unless it is the UNKNOWN type, in which case we return 0.
    if (t == MEDIATYPE_UNKNOWN) return 0;
    return (t + 1) & 0xFF;
}

/**
 * @brief Static method to discover media type from filename extension
 * 
 * This is a base implementation that checks common extensions.
 * Platform-specific implementations can override this.
 */
mediatype_t MediaTypeBase::discover_mediatype(const char *filename)
{
    if (filename == nullptr)
        return MEDIATYPE_UNKNOWN;
    
    const char *ext = get_extension(filename);
    if (ext == nullptr)
        return MEDIATYPE_UNKNOWN;
    
    // Atari formats
    if (ext_matches(ext, "atr"))
        return MEDIATYPE_ATR;
    if (ext_matches(ext, "atx"))
        return MEDIATYPE_ATX;
    if (ext_matches(ext, "xex"))
        return MEDIATYPE_XEX;
    if (ext_matches(ext, "cas"))
        return MEDIATYPE_CAS;
    if (ext_matches(ext, "wav"))
        return MEDIATYPE_WAV;
    
    // Apple formats
    if (ext_matches(ext, "do"))
        return MEDIATYPE_DO;
    if (ext_matches(ext, "po"))
        return MEDIATYPE_PO;
    if (ext_matches(ext, "dsk"))
        return MEDIATYPE_DSK;
    if (ext_matches(ext, "woz"))
        return MEDIATYPE_WOZ;
    
    // BBC formats
    if (ext_matches(ext, "ssd"))
        return MEDIATYPE_SSD;
    if (ext_matches(ext, "dsd"))
        return MEDIATYPE_DSD;
    if (ext_matches(ext, "adfs"))
        return MEDIATYPE_ADFS;
    
    // Generic formats
    if (ext_matches(ext, "img"))
        return MEDIATYPE_IMG;
    
    Debug_printf("Unknown media type for extension: %s\n", ext);
    return MEDIATYPE_UNKNOWN;
}