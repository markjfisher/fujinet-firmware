# BBC Micro Media Type Implementations

This directory contains BBC Micro disk format implementations for FujiNet firmware.

## Overview

BBC Micro computers used the DFS (Disk Filing System) for floppy disk storage. This implementation supports the two main DFS image formats:

- **SSD** - Single-Sided Disk
- **DSD** - Double-Sided Disk

Both formats use 256-byte sectors organized in tracks, following the standard BBC Micro DFS layout.

## Files

- [`mediatype_ssd.h`](mediatype_ssd.h) / [`mediatype_ssd.cpp`](mediatype_ssd.cpp) - Single-sided DFS implementation
- [`mediatype_dsd.h`](mediatype_dsd.h) / [`mediatype_dsd.cpp`](mediatype_dsd.cpp) - Double-sided DFS implementation
- `README.md` - This file

## DFS Format Specification

### Common Characteristics

- **Sector Size**: 256 bytes (fixed)
- **Sectors per Track**: 10 (numbered 0-9)
- **Track Numbering**: 0-based (0-39 for 40 track, 0-79 for 80 track)
- **Sector Numbering**: Logical sectors numbered sequentially from 0

### SSD Format

**File Extension**: `.ssd`

**Geometry**:
- 40 tracks: 400 sectors = 102,400 bytes
- 80 tracks: 800 sectors = 204,800 bytes

**Layout**:
```
Track 0: Sectors 0-9
Track 1: Sectors 10-19
...
Track 39: Sectors 390-399 (40 track)
Track 79: Sectors 790-799 (80 track)
```

### DSD Format

**File Extension**: `.dsd`

**Geometry**:
- 40 tracks/side × 2 sides: 800 sectors = 204,800 bytes
- 80 tracks/side × 2 sides: 1600 sectors = 409,600 bytes

**Layout** (interleaved by side):
```
Track 0, Side 0: Sectors 0-9
Track 0, Side 1: Sectors 10-19
Track 1, Side 0: Sectors 20-29
Track 1, Side 1: Sectors 30-39
...
```

**Physical Sector Calculation**:
```
physical_sector = (track × 10 × 2) + (side × 10) + sector_in_track
```

## DFS Catalog Structure

The catalog occupies sectors 0-1 of track 0 (and on DSD, also sectors 0-1 of track 0, side 1).

### Sector 0 (Catalog Sector 0)
Contains first 8 file entries (8 bytes each):
```
Offset  Size  Description
------  ----  -----------
0x00    7     Filename (space-padded)
0x07    1     Directory character
... (repeat for 8 files)
```

### Sector 1 (Catalog Sector 1)
Contains second 8 file entries plus disk information:
```
Offset  Size  Description
------  ----  -----------
0x00    7     Filename (space-padded)
0x07    1     Directory character
... (repeat for 8 files)

0x00    8     Disk title (at start of sector)
0xF6    1     Number of files × 8
0xF7    1     Boot option
0xF8    2     Number of sectors (little-endian)
0xFA    1     Reserved
0xFB    5     Reserved
```

## Usage Examples

### Mounting an SSD Image

```cpp
#include "bbc/mediatype_ssd.h"

MediaTypeSSD ssd;
fnFile *f = /* open "game.ssd" */;
mediatype_t type = ssd.mount(f, 102400);

if (type == MEDIATYPE_SSD) {
    // Successfully mounted 40-track SSD
    printf("Mounted: %u sectors\n", ssd.num_sectors());
}
```

### Reading a Sector

```cpp
uint16_t count;
bool err = ssd.read(42, &count);

if (!err) {
    uint8_t *data = ssd.get_sector_buffer();
    // Process 256 bytes of sector data
}
```

### Writing a Sector

```cpp
uint8_t *buffer = ssd.get_sector_buffer();
// ... fill buffer with 256 bytes ...

bool err = ssd.write(42, false); // false = no verify

if (!err) {
    // Sector written successfully
}
```

### Creating a Blank Disk

```cpp
fnFile *f = /* create new file */;

// Create 40-track SSD
bool err = MediaTypeSSD::create(f, 40);

// Or create 80-track DSD
bool err = MediaTypeDSD::create(f, 80);
```

## Implementation Details

### Sector Addressing

**SSD**: Simple linear addressing
```
logical_sector = (track × 10) + sector_in_track
file_offset = logical_sector × 256
```

**DSD**: Interleaved addressing
```
track, side, sector = decode(logical_sector)
physical_sector = (track × 20) + (side × 10) + sector
file_offset = physical_sector × 256
```

### Catalog Parsing

Both formats parse the DFS catalog on mount to:
- Validate the disk structure
- Extract disk title and file count
- Verify sector count matches image size
- Log disk information for debugging

### Error Handling

All methods return `bool` for error status:
- `false` = Success
- `true` = Error occurred

Errors are logged via Debug_printf for troubleshooting.

## Testing

### Test Cases

1. **Mount Validation**
   - Valid 40-track SSD (102,400 bytes)
   - Valid 80-track SSD (204,800 bytes)
   - Valid 40-track DSD (204,800 bytes)
   - Valid 80-track DSD (409,600 bytes)
   - Invalid sizes should fail

2. **Read Operations**
   - Read first sector (catalog)
   - Read middle sector
   - Read last sector
   - Read out-of-range sector (should fail)

3. **Write Operations**
   - Write to data sector
   - Write with verify
   - Write to read-only disk (should fail)
   - Write out-of-range sector (should fail)

4. **Blank Disk Creation**
   - Create 40-track SSD
   - Create 80-track SSD
   - Create 40-track DSD
   - Create 80-track DSD
   - Verify catalog structure

### Test Images

You can create test images using BBC Micro emulators or tools:
- BeebEm (BBC Micro emulator)
- DFS Explorer (disk image tool)
- Or use the `create()` static methods

## Future Enhancements

- [ ] ADFS (Advanced Disc Filing System) support
- [ ] Catalog modification (add/delete files)
- [ ] File extraction/insertion
- [ ] Disk optimization/compaction
- [ ] Bad sector handling
- [ ] Copy protection detection

## References

- [BBC Micro DFS Format](http://beebwiki.mdfs.net/Acorn_DFS_disc_format)
- [DFS Catalog Structure](http://beebwiki.mdfs.net/Acorn_DFS_catalogue_format)
- [BBC Micro Hardware](http://beebwiki.mdfs.net/BBC_Micro)
- [FujiNet Disk Architecture](../../../docs/arch/disk.md)

## Contributing

When modifying these media type implementations:

1. Maintain compatibility with standard DFS format
2. Add debug logging for troubleshooting
3. Test with real BBC disk images
4. Update this README with any format discoveries
5. Follow the MediaTypeBase interface contract

## License

Same as FujiNet firmware project.