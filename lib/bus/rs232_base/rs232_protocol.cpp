#include "rs232_protocol.h"

#include <cstring>

#include "../../../include/debug.h"

// ============================================================================
// Protocol Functions Implementation
// ============================================================================

namespace RS232Protocol {

/**
 * @brief Calculate RS232 checksum
 * 
 * Implements the standard 8-bit wrap-around checksum algorithm used by
 * all RS232-based platforms. The checksum is calculated by summing all
 * bytes and handling overflow by adding the carry back to the sum.
 * 
 * Algorithm:
 * 1. Initialize checksum to 0
 * 2. For each byte in buffer:
 *    a. Add byte to checksum
 *    b. Add high byte (carry) to low byte
 * 3. Return low byte as final checksum
 * 
 * @param buf Buffer to checksum
 * @param len Length of buffer in bytes
 * @return 8-bit checksum value
 */
uint8_t calculate_checksum(uint8_t *buf, unsigned short len)
{
    uint8_t chk = 0;
    
    for (int i = 0; i < len; i++) {
        uint16_t add_current = chk + buf[i];
        chk = (add_current & 0xFF) + (add_current > 0xFF) ? 1 : 0;
    }
    
    return chk;
}

} // namespace RS232Protocol


// ============================================================================
// RS232BusBase Implementation
// ============================================================================

/**
 * @brief Add a device to the bus
 * 
 * Registers a device with the bus and assigns it a device ID.
 * The device is added to the front of the daisy chain for efficient
 * access to recently added devices.
 * 
 * @param pDevice Pointer to device to add
 * @param device_id Device ID to assign (1-255)
 */
void RS232BusBase::addDevice(RS232DeviceBase *pDevice, fujiDeviceID_t device_id)
{
    pDevice->_devnum = device_id;
    pDevice->_bus = this;  // Set bus pointer so device can call bus methods
    _daisyChain.push_front(pDevice);
}

/**
 * @brief Send data to computer (shared implementation)
 *
 * Sends data buffer to computer with checksum and completion/error status.
 * This is the standard RS232 data transmission protocol used by all platforms.
 *
 * @param buf Buffer to send
 * @param len Length of buffer
 * @param err Error flag (true = send ERROR, false = send COMPLETE)
 */
void RS232DeviceBase::bus_to_computer(uint8_t *buf, uint16_t len, bool err)
{
    _bus->sendReplyPacket(id(), !err, buf, len);
}

/**
 * @brief Receive data from computer (shared implementation)
 *
 * Receives data buffer from computer and returns the checksum.
 * This is the standard RS232 data reception protocol used by all platforms.
 *
 * @param buf Buffer to receive into
 * @param len Length to receive
 * @return Checksum byte from computer
 */
uint8_t RS232DeviceBase::bus_to_peripheral(uint8_t *buf, uint16_t len)
{
    // Read data
    _bus->read(buf, len);
    
    // Read checksum
    uint8_t cksum = _bus->read();
    
    return cksum;
}

/**
 * @brief Send ACK to computer (shared implementation)
 *
 * Sends 'A' to indicate command accepted.
 */
void RS232DeviceBase::rs232_ack()
{
    Debug_printv("Sending: ACK");
    _bus->writeByte('A');
    _bus->flushOutput();
}

/**
 * @brief Send NAK to computer (shared implementation)
 *
 * Sends 'N' to indicate command rejected.
 */
void RS232DeviceBase::rs232_nak()
{
    Debug_printv("Sending: NAK");
    _bus->writeByte('N');
    _bus->flushOutput();
}

/**
 * @brief Send COMPLETE to computer (shared implementation)
 *
 * Sends 'C' to indicate command completed successfully.
 */
void RS232DeviceBase::rs232_complete()
{
    Debug_printv("Sending: COMPLETE");
    _bus->writeByte('C');
    _bus->flushOutput();
}

/**
 * @brief Send ERROR to computer (shared implementation)
 *
 * Sends 'E' to indicate command failed.
 */
void RS232DeviceBase::rs232_error()
{
    Debug_printv("Sending: ERROR");
    _bus->writeByte('E');
    _bus->flushOutput();
}

/**
 * @brief Remove a device from the bus
 * 
 * Unregisters a device from the bus. The device will no longer
 * receive commands. Note that this does NOT delete the device object.
 * 
 * @param pDevice Pointer to device to remove
 */
void RS232BusBase::remDevice(RS232DeviceBase *pDevice)
{
    _daisyChain.remove(pDevice);
}

/**
 * @brief Find device by ID
 * 
 * Searches the daisy chain for a device with the given ID.
 * Returns nullptr if no device with that ID is found.
 * 
 * @param device_id Device ID to find
 * @return Pointer to device or nullptr if not found
 */
RS232DeviceBase* RS232BusBase::deviceById(fujiDeviceID_t device_id)
{
    for (auto dev : _daisyChain)
    {
        if (dev->id() == device_id)
            return dev;
    }
    return nullptr;
}

/**
 * @brief Change device ID
 * 
 * Changes the ID of a registered device. The device must already
 * be registered with the bus.
 * 
 * @param pDevice Pointer to device
 * @param device_id New device ID
 */
void RS232BusBase::changeDeviceId(RS232DeviceBase *pDevice, fujiDeviceID_t device_id)
{
    pDevice->_devnum = device_id;
}

/**
 * @brief Get number of devices on bus
 * 
 * Counts the number of devices in the daisy chain by iterating
 * through the entire list.
 * 
 * Note: This is O(n) operation. Avoid calling frequently if possible.
 * 
 * @return Device count
 */
int RS232BusBase::numDevices()
{
    int count = 0;
    for (auto dev : _daisyChain)
    {
        count++;
    }
    return count;
}

/**
 * @brief Process a command frame (shared implementation)
 *
 * This method contains the common logic for processing RS232 commands:
 * 1. Verify checksum
 * 2. Find target device
 * 3. Route command to device
 *
 * Platform-specific implementations just need to:
 * - Read the command frame from their serial port
 * - Call this method to process it
 *
 * @param cmd Pointer to command frame to process
 */
void RS232BusBase::processCommand(FujiBusPacket &packet)
{
    // Verify checksum
    uint8_t calc_cksum = RS232Protocol::calculate_checksum(
        (uint8_t*)cmd, sizeof(RS232Protocol::cmdFrame_t) - 1);
    
    if (calc_cksum != cmd->cksum)
    {
        // Checksum error - silently ignore
        return;
    }
    
    // Find device
    RS232DeviceBase *dev = deviceById(cmd->device);
    
    if (dev == nullptr)
    {
        // Device not found - silently ignore
        return;
    }
    
    // Set active device
    _activeDev = dev;
    
    // Process command
    dev->rs232_process(cmd);
    
    _command_frame_counter++;
}

/**
 * @brief Service the bus (shared implementation)
 *
 * Checks for incoming serial data and processes commands.
 * Simple platforms can use this default implementation.
 * Complex platforms (with modem/CPM modes) can override.
 */
void RS232BusBase::service()
{
    // Check for incoming commands
    if (_port.available() >= sizeof(RS232Protocol::cmdFrame_t))
    {
        RS232Protocol::cmdFrame_t cmd;
        
        // Read command frame
        size_t bytes_read = _port.read(&cmd, sizeof(RS232Protocol::cmdFrame_t));
        
        if (bytes_read == sizeof(RS232Protocol::cmdFrame_t))
        {
            // Process using shared command routing
            processCommand(&cmd);
        }
    }
}

/**
 * @brief Shutdown the bus (shared implementation)
 *
 * Shuts down all devices and closes the serial port.
 * Platforms can override if they need special cleanup.
 */
void RS232BusBase::shutdown()
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

void RS232BusBase::sendReplyPacket(fujiDeviceID_t source, bool ack, void *data, size_t length)
{
    FujiBusPacket packet(source, ack ? FUJICMD_ACK : FUJICMD_NAK, ack ? std::string(static_cast<const char*>(data), length) : "");
    std::string encoded = packet.serialize();
    _port.write(encoded.data(), encoded.size());
}

// Default implementations but abort so we can see where they are called from
size_t RS232BusBase::read(void *buffer, size_t length)
{
    abort();
    return _port.read(buffer, length);
}

size_t RS232BusBase::read()
{
    abort();
    return _port.read();
}

size_t RS232BusBase::write(const void *buffer, size_t length)
{
    abort();
    return _port.write(buffer, length);
}

size_t RS232BusBase::write(int n)
{
    abort();
    return _port.write(n);
}

size_t RS232BusBase::available()
{
    abort();
    return _port.available();
}

void RS232BusBase::flushOutput()
{
    abort();
    _port.flushOutput();
}

size_t RS232BusBase::print(int n, int base)
{
    abort();
    return _port.print(n, base);
}

size_t RS232BusBase::print(const char *str)
{
    abort();
    return _port.print(str);
}

size_t RS232BusBase::print(const std::string &str)
{
    abort();
    return _port.print(str);
}
