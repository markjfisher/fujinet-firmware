# RS232 Protocol Base Layer

## Overview

This directory contains the shared RS232 protocol code used by all RS232-based FujiNet platforms (Atari, BBC, etc.). By extracting common protocol functionality into this base layer, we eliminate code duplication and provide a consistent foundation for RS232 implementations.

## Purpose

The rs232_base layer provides:

1. **Protocol Definitions**: Command frame structure, timing constants, direction flags
2. **Checksum Algorithm**: Standard 8-bit wrap-around checksum used by all platforms
3. **Base Device Class**: Common interface for all RS232 virtual devices
4. **Base Bus Class**: Common bus management functionality
5. **Helper Methods**: Aux byte accessors, device management

## What's Shared vs Platform-Specific

### Shared in rs232_base (100% reuse)

✅ **Protocol Functions:**
- `RS232Protocol::calculate_checksum()` - 8-bit checksum algorithm
- Command frame structure (`cmdFrame_t`)
- Timing constants (DEFAULT_DELAY_T4, DEFAULT_DELAY_T5)

✅ **Device Base Class (`RS232DeviceBase`):**
- Aux byte helper methods (`rs232_get_aux16_lo()`, etc.)
- Device ID management
- Command frame storage
- Virtual method interface (ACK/NAK/COMPLETE/ERROR)

✅ **Bus Base Class (`RS232BusBase`):**
- Device registration (`addDevice()`, `remDevice()`)
- Device lookup (`deviceById()`)
- Device ID management (`changeDeviceId()`)
- Device counting (`numDevices()`)
- Baud rate management

### Platform-Specific (must be implemented)

❌ **Device Implementation:**
- `rs232_ack()` - Send ACK (platform-specific serial write)
- `rs232_nak()` - Send NAK (platform-specific serial write)
- `rs232_complete()` - Send COMPLETE (platform-specific serial write)
- `rs232_error()` - Send ERROR (platform-specific serial write)
- `rs232_process()` - Command processing logic
- `rs232_status()` - Status reporting

❌ **Bus Implementation:**
- `setup()` - Initialize serial port and hardware
- `service()` - Main service loop (check for commands, route to devices)
- `shutdown()` - Clean shutdown

❌ **Device Types:**
- Each platform implements only the devices it needs
- BBC: disk only (initially)
- Atari: disk, modem, printer, CPM, network, etc.

## File Structure

```
lib/bus/rs232_base/
├── rs232_protocol.h      # Header with protocol definitions and base classes
├── rs232_protocol.cpp    # Implementation of shared functionality
└── README.md             # This file
```

## Usage Guide

### For Platform Implementers

To add RS232 support for a new platform:

#### Step 1: Create Platform Bus Directory

```bash
mkdir -p lib/bus/yourplatform_rs232
```

#### Step 2: Create Platform Bus Header

```cpp
// lib/bus/yourplatform_rs232/yourplatform_rs232.h
#ifndef YOURPLATFORM_RS232_H
#define YOURPLATFORM_RS232_H

#include "../rs232_base/rs232_protocol.h"
#include "../../hardware/UARTChannel.h"  // Or appropriate channel type

// Device class - inherits from RS232DeviceBase
class virtualDevice : public RS232DeviceBase
{
protected:
    // Implement protocol methods (platform-specific serial I/O)
    void rs232_ack() override;
    void rs232_nak() override;
    void rs232_complete() override;
    void rs232_error() override;
    
    // Pure virtuals from base (must implement in device subclasses)
    // virtual void rs232_process(RS232Protocol::cmdFrame_t *cmd) = 0;
    // virtual void rs232_status() = 0;
};

// Bus class - inherits from RS232BusBase
class systemBus : public RS232BusBase
{
private:
    UARTChannel _port;  // Or appropriate channel type
    
    // Platform-specific members
    // (e.g., modem device, CPM device, etc.)
    
    void _rs232_process_cmd();  // Internal command processing

public:
    // Implement pure virtuals from base
    void setup() override;
    void service() override;
    void shutdown() override;
    
    // Direct serial port access (for special cases)
    size_t read(void *buffer, size_t length) { return _port.read(buffer, length); }
    size_t write(const void *buffer, size_t length) { return _port.write(buffer, length); }
    void flush() { _port.flush(); }
    // ... other serial methods as needed
};

extern systemBus YOURPLATFORM_RS232;

#endif // YOURPLATFORM_RS232_H
```

#### Step 3: Implement Platform Bus

```cpp
// lib/bus/yourplatform_rs232/yourplatform_rs232.cpp
#include "yourplatform_rs232.h"

// Global bus instance
systemBus YOURPLATFORM_RS232;

// Implement protocol methods using platform-specific serial I/O
void virtualDevice::rs232_ack()
{
    YOURPLATFORM_RS232.write('A');
    YOURPLATFORM_RS232.flush();
}

void virtualDevice::rs232_nak()
{
    YOURPLATFORM_RS232.write('N');
    YOURPLATFORM_RS232.flush();
}

void virtualDevice::rs232_complete()
{
    YOURPLATFORM_RS232.write('C');
    YOURPLATFORM_RS232.flush();
}

void virtualDevice::rs232_error()
{
    YOURPLATFORM_RS232.write('E');
    YOURPLATFORM_RS232.flush();
}

// Implement bus methods
void systemBus::setup()
{
    // Initialize serial port
    _port.begin(9600);  // Or appropriate baud rate
    _rs232Baud = 9600;
    
    // Initialize hardware (pins, etc.)
    // ...
}

void systemBus::service()
{
    // Check for incoming commands
    if (_port.available() >= sizeof(RS232Protocol::cmdFrame_t))
    {
        _rs232_process_cmd();
    }
    
    // Handle platform-specific modes (modem, CPM, etc.)
    // ...
}

void systemBus::_rs232_process_cmd()
{
    RS232Protocol::cmdFrame_t cmd;
    
    // Read command frame
    _port.read(&cmd, sizeof(cmd));
    
    // Verify checksum
    uint8_t calc_cksum = RS232Protocol::calculate_checksum(
        (uint8_t*)&cmd, sizeof(cmd) - 1);
    
    if (calc_cksum != cmd.cksum)
    {
        // Checksum error - ignore command
        return;
    }
    
    // Find device
    RS232DeviceBase *dev = deviceById(cmd.device);
    if (dev == nullptr)
    {
        // Device not found
        return;
    }
    
    // Set active device and process command
    _activeDev = dev;
    dev->rs232_process(&cmd);
    
    _command_frame_counter++;
}

void systemBus::shutdown()
{
    shuttingDown = true;
    
    // Shutdown all devices
    for (auto dev : _daisyChain)
    {
        dev->shutdown();
    }
    
    // Close serial port
    _port.end();
}
```

#### Step 4: Create Platform Devices

```cpp
// lib/device/yourplatform_rs232/disk.h
#ifndef YOURPLATFORM_DISK_H
#define YOURPLATFORM_DISK_H

#include "../../bus/rs232_base/rs232_protocol.h"
#include "../disk_base.h"

class yourplatformRS232Disk : public DiskBase,
                               public RS232DeviceBase
{
protected:
    // Implement DiskBase pure virtuals
    bool read_sector(uint32_t sector_num, uint8_t *buffer, 
                     uint16_t *count) override;
    bool write_sector(uint32_t sector_num, uint8_t *buffer, 
                      uint16_t *count, bool verify) override;
    MediaTypeBase* create_media_type(mediatype_t disk_type) override;
    
    // Implement RS232DeviceBase pure virtuals
    void rs232_process(RS232Protocol::cmdFrame_t *cmd) override;
    void rs232_status() override;
    
    // Platform-specific command handlers
    void rs232_read();
    void rs232_write(bool verify);
};

#endif // YOURPLATFORM_DISK_H
```

### Key Points

1. **Inherit from Base Classes**: Your platform's `virtualDevice` and `systemBus` should inherit from `RS232DeviceBase` and `RS232BusBase`

2. **Use Shared Protocol**: Use `RS232Protocol::calculate_checksum()` and `RS232Protocol::cmdFrame_t`

3. **Implement Pure Virtuals**: You must implement all pure virtual methods from the base classes

4. **Platform-Specific I/O**: Protocol methods (ACK/NAK/etc.) use your platform's serial I/O

5. **Device Management**: Use inherited methods (`addDevice()`, `deviceById()`, etc.)

## Benefits

### Code Reuse
- ~200 lines of shared protocol code
- No duplication of checksum algorithm
- No duplication of device management
- No duplication of aux byte helpers

### Maintainability
- Fix protocol bugs once, benefit everywhere
- Consistent behavior across platforms
- Clear separation of concerns

### Extensibility
- Easy to add new RS232 platforms
- Minimal boilerplate required
- Focus on platform-specific logic

### Type Safety
- Strong typing prevents mixing incompatible types
- Compile-time checking of virtual method implementation
- Clear interface contracts

## Examples

### BBC RS232 Implementation

See [`lib/bus/bbc_rs232/`](../bbc_rs232/) for a complete example of using rs232_base.

Key features:
- Inherits from `RS232DeviceBase` and `RS232BusBase`
- Implements only disk device initially
- Uses shared checksum and device management
- Platform-specific service loop

### Atari RS232 Implementation

See [`lib/bus/rs232/`](../rs232/) for a more complex example with multiple device types.

Key features:
- Inherits from `RS232DeviceBase` and `RS232BusBase`
- Implements disk, modem, printer, CPM, network devices
- Uses shared checksum and device management
- Platform-specific modem and CPM mode handling

## Testing

### Unit Tests

Test the shared protocol code:

```cpp
#include "rs232_protocol.h"
#include <gtest/gtest.h>

TEST(RS232Protocol, ChecksumCalculation)
{
    uint8_t data[] = {0x31, 0x52, 0x00, 0x00, 0x00, 0x00};
    uint8_t expected = 0x83;
    ASSERT_EQ(RS232Protocol::calculate_checksum(data, 6), expected);
}

TEST(RS232Protocol, ChecksumEmpty)
{
    uint8_t data[] = {};
    ASSERT_EQ(RS232Protocol::calculate_checksum(data, 0), 0);
}

TEST(RS232Protocol, ChecksumSingleByte)
{
    uint8_t data[] = {0xFF};
    ASSERT_EQ(RS232Protocol::calculate_checksum(data, 1), 0xFF);
}
```

### Integration Tests

Test platform-specific implementations:

```cpp
TEST(BBCBus, DeviceRegistration)
{
    systemBus bus;
    bbcRS232Disk disk;
    
    bus.addDevice(&disk, 0x31);
    ASSERT_EQ(bus.numDevices(), 1);
    ASSERT_EQ(bus.deviceById(0x31), &disk);
}

TEST(BBCBus, CommandProcessing)
{
    // Test command routing, checksum validation, etc.
}
```

## Migration Guide

### Migrating Existing Platform to rs232_base

If you have an existing RS232 platform implementation:

1. **Backup**: Create a git branch for the migration
2. **Include**: Add `#include "../rs232_base/rs232_protocol.h"`
3. **Inherit**: Change classes to inherit from base classes
4. **Remove**: Delete duplicated code (checksum, device management, etc.)
5. **Update**: Use `RS232Protocol::` namespace for shared functions
6. **Test**: Verify all functionality still works
7. **Document**: Update platform documentation

See [`docs/arch/rs232_refactoring_plan.md`](../../../docs/arch/rs232_refactoring_plan.md) for detailed migration steps.

## Common Pitfalls

### 1. Forgetting to Implement Pure Virtuals

**Error**: Compiler complains about abstract class

**Solution**: Implement all pure virtual methods:
- `rs232_ack()`, `rs232_nak()`, `rs232_complete()`, `rs232_error()`
- `rs232_process()`, `rs232_status()`
- `setup()`, `service()`, `shutdown()`

### 2. Not Using Shared Checksum

**Error**: Implementing own checksum function

**Solution**: Use `RS232Protocol::calculate_checksum()`

### 3. Duplicating Device Management

**Error**: Implementing own `addDevice()`, `deviceById()`, etc.

**Solution**: Use inherited methods from `RS232BusBase`

### 4. Wrong Namespace

**Error**: `cmdFrame_t` not found

**Solution**: Use `RS232Protocol::cmdFrame_t` or add `using namespace RS232Protocol;`

## Future Enhancements

- [ ] Add unit tests for rs232_base
- [ ] Add timing variation support
- [ ] Add high-speed mode helpers
- [ ] Add command logging/debugging helpers
- [ ] Add performance profiling hooks

## References

- [RS232 Refactoring Plan](../../../docs/arch/rs232_refactoring_plan.md)
- [BBC Architecture](../../../docs/arch/bbc.md)
- [Disk Architecture](../../../docs/arch/disk.md)

## License

Same as FujiNet firmware project.