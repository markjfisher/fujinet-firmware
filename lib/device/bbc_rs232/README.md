# BBC Micro RS232 Disk Device Implementation

This directory contains the BBC Micro disk device implementation for RS232/RS423 serial connection to FujiNet firmware.

## Overview

This implementation uses the new multi-layer disk abstraction architecture:

```
BBC Micro (RS423) ←→ BBC_RS232 Bus ←→ bbcRS232Disk ←→ DiskBase ←→ MediaTypeSSD/DSD
```

### Multiple Inheritance Architecture

The [`bbcRS232Disk`](disk.h) class uses **multiple inheritance** to combine:
- **[`DiskBase`](../disk_base.h)**: Provides disk logic (mount, unmount, media management)
- **`virtualDevice`** (from [`bbc_rs232.h`](../../bus/bbc_rs232/bbc_rs232.h)): Provides RS232 bus protocol

This allows the disk device to:
- ✅ Reuse common disk logic from DiskBase
- ✅ Reuse media formats (SSD/DSD) shared with other BBC buses
- ✅ Implement RS232-specific protocol methods

## Files

- [`disk.h`](disk.h) - BBC RS232 disk device class declaration
- [`disk.cpp`](disk.cpp) - BBC RS232 disk device implementation
- `README.md` - This file

## Architecture

### Class Hierarchy

```cpp
class bbcRS232Disk : public DiskBase,      // Disk logic
                     public virtualDevice  // RS232 protocol
{
    // Implements DiskBase pure virtuals:
    bool read_sector(...);
    bool write_sector(...);
    void get_status(...);
    MediaTypeBase* create_media_type(...);
    
    // Implements virtualDevice pure virtuals:
    void rs232_process(cmdFrame_t *cmd);
    void rs232_status();
    
    // RS232-specific handlers:
    void rs232_read();
    void rs232_write(bool verify);
    void rs232_format();
};
```

### Data Flow

#### Read Operation
```
BBC Computer
    ↓ sends READ command (0x52) + sector number
BBC_RS232 Bus
    ↓ calls rs232_process()
bbcRS232Disk::rs232_process()
    ↓ calls rs232_read()
bbcRS232Disk::rs232_read()
    ↓ calls read_sector()
DiskBase::read_sector()
    ↓ calls _media->read()
MediaTypeSSD::read()
    ↓ reads from file
    ↑ returns data in sector buffer
bbcRS232Disk::rs232_read()
    ↓ calls bus_to_computer()
BBC_RS232 Bus
    ↓ sends data + checksum
BBC Computer
```

## Supported Commands

The BBC RS232 disk protocol supports these commands:

| Command | Code | Description |
|---------|------|-------------|
| READ    | 0x52 | Read a sector from disk |
| WRITE   | 0x57 | Write a sector to disk |
| STATUS  | 0x53 | Get disk status |
| FORMAT  | 0x46 | Format disk |

### Command Frame Structure

```cpp
struct cmdFrame_t {
    uint8_t device;   // Device ID (0x31-0x3F for disks)
    uint8_t comnd;    // Command byte
    uint32_t aux;     // Auxiliary data (e.g., sector number)
    uint8_t cksum;    // Checksum
};
```

## RS232/RS423 Protocol

### BBC RS423 Specifications

The BBC Micro uses RS423, which is electrically compatible with RS232:
- **Baud Rate**: 9600 (default, configurable)
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: RTS/CTS or XON/XOFF

### Protocol Flow

1. **Command Phase**:
   - BBC sends command frame
   - FujiNet validates checksum
   - FujiNet sends ACK ('A') or NAK ('N')

2. **Data Phase** (if applicable):
   - For READ: FujiNet sends data + checksum
   - For WRITE: BBC sends data + checksum

3. **Completion Phase**:
   - FujiNet sends COMPLETE ('C') or ERROR ('E')

## Usage

### Building

```bash
cd fujinet-firmware
mkdir build && cd build
cmake -DFUJINET_TARGET=BBC_RS232 ..
make
```

### Mounting a Disk

```cpp
#include "bbc_rs232/disk.h"

bbcRS232Disk disk;
fnFile *f = /* open "game.ssd" */;
mediatype_t type = disk.mount(f, "game.ssd", filesize);

if (type != MEDIATYPE_UNKNOWN) {
    // Add to bus
    BBC_RS232.addDevice(&disk, 0x31);  // Device ID 0x31 (disk 1)
}
```

### Processing Commands

Commands are automatically processed by the bus:

```cpp
// In main loop
BBC_RS232.service();  // Checks for commands and routes to devices
```

When a command arrives for device 0x31:
1. Bus calls `bbcRS232Disk::rs232_process(&cmd)`
2. Device processes command (read, write, status, etc.)
3. Device sends response via bus

## Status Bytes

The BBC RS232 disk returns 4 status bytes:

```
Byte 0: Drive Status
  Bit 7: Reserved
  Bit 6: Double-sided (1=DSD, 0=SSD)
  Bit 5: 80 track (1=80 track, 0=40 track)
  Bit 4: Reserved
  Bit 3: Write protected
  Bit 2-1: Reserved
  Bit 0: Disk present

Byte 1: Number of tracks per side (40 or 80)
Byte 2: Sectors per track (10)
Byte 3: Sector size indicator (0x01 = 256 bytes)
```

## Supported Disk Formats

### SSD - Single-Sided DFS
- **Extension**: `.ssd`
- **Size**: 102,400 bytes (40 track) or 204,800 bytes (80 track)
- **Implementation**: [`MediaTypeSSD`](../../media/bbc/mediatype_ssd.h)

### DSD - Double-Sided DFS
- **Extension**: `.dsd`
- **Size**: 204,800 bytes (40 track) or 409,600 bytes (80 track)
- **Implementation**: [`MediaTypeDSD`](../../media/bbc/mediatype_dsd.h)

## Code Sharing

### Shared with Other BBC Buses

The following components are **shared** across all BBC bus implementations (RS232, User Port, 1MHz):

✅ **Media Layer**: [`MediaTypeSSD`](../../media/bbc/mediatype_ssd.h), [`MediaTypeDSD`](../../media/bbc/mediatype_dsd.h)
- Same SSD/DSD format code
- Same read/write logic
- Same catalog parsing

✅ **DiskBase**: [`lib/device/disk_base.h`](../disk_base.h)
- Mount/unmount logic
- Media lifecycle management
- Type discovery

✅ **Factory Method**: `create_media_type()` implementation is identical

### RS232-Specific

The following are **unique** to the RS232 bus:

❌ **Bus Protocol**: [`lib/bus/bbc_rs232/`](../../bus/bbc_rs232/)
- RS232 timing and handshaking
- ACK/NAK/COMPLETE/ERROR protocol
- Serial communication

❌ **Command Handlers**: `rs232_read()`, `rs232_write()`, etc.
- RS232-specific data transfer
- RS232-specific error handling

## Testing

### Unit Tests

Test the disk device with various scenarios:

1. **Mount Tests**:
   - Valid SSD (40 track, 80 track)
   - Valid DSD (40 track, 80 track)
   - Invalid sizes
   - Missing files

2. **Read Tests**:
   - Read first sector (catalog)
   - Read middle sector
   - Read last sector
   - Read out-of-range sector

3. **Write Tests**:
   - Write to data sector
   - Write with verify
   - Write to read-only disk
   - Write out-of-range sector

4. **Status Tests**:
   - Status with no disk
   - Status with SSD
   - Status with DSD
   - Status with write-protected disk

### Integration Tests

Test with real BBC Micro:

1. Connect BBC Micro to FujiNet via RS423
2. Configure BBC serial port (9600 baud, 8N1)
3. Mount BBC disk image
4. Test disk operations from BBC BASIC
5. Verify data integrity

## Future Enhancements

- [ ] Implement high-speed mode (19200+ baud)
- [ ] Add flow control support
- [ ] Implement disk caching
- [ ] Add ADFS format support
- [ ] Optimize sector transfers
- [ ] Add error recovery

## Comparison with Other BBC Buses

### RS232 (This Implementation)
- **Speed**: Moderate (9600-19200 baud typical)
- **Complexity**: Low (standard serial protocol)
- **Availability**: High (most BBCs have RS423)
- **Status**: ✅ Implemented

### User Port (Future)
- **Speed**: Fast (parallel transfer)
- **Complexity**: Medium (parallel protocol)
- **Availability**: High (all BBCs have user port)
- **Status**: 🔜 Planned
- **Shared Code**: Will reuse same media layer and DiskBase

### 1MHz Bus (Future)
- **Speed**: Very Fast (memory-mapped)
- **Complexity**: High (memory-mapped I/O)
- **Availability**: Medium (requires expansion)
- **Status**: 🔜 Planned
- **Shared Code**: Will reuse same media layer and DiskBase

## References

- [BBC Micro RS423](http://beebwiki.mdfs.net/RS423)
- [BBC DFS Format](http://beebwiki.mdfs.net/Acorn_DFS_disc_format)
- [FujiNet Disk Architecture](../../../docs/arch/disk.md)
- [BBC RS232 Bus](../../bus/bbc_rs232/bbc_rs232.h)
- [Base Disk Class](../disk_base.h)
- [BBC Media Formats](../../media/bbc/README.md)

## Contributing

When modifying this implementation:

1. Maintain compatibility with BBC RS423 protocol
2. Keep RS232-specific code in this directory
3. Put shared logic in DiskBase
4. Put format logic in media layer
5. Add debug logging for troubleshooting
6. Test with real BBC hardware
7. Update this README

## License

Same as FujiNet firmware project.