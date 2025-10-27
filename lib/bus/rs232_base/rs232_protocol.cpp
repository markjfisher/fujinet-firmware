#include "rs232_protocol.h"

#include <cstring>

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
    unsigned int chk = 0;
    
    for (int i = 0; i < len; i++)
        chk = ((chk + buf[i]) >> 8) + ((chk + buf[i]) & 0xff);
    
    return chk;
}

} // namespace RS232Protocol

// ============================================================================
// RS232DeviceBase Implementation
// ============================================================================

/**
 * @brief Get aux bytes as 16-bit value (low)
 * 
 * Combines aux1 and aux2 into a 16-bit value with aux1 as the low byte
 * and aux2 as the high byte.
 * 
 * @return aux1 + (aux2 << 8)
 */
uint16_t RS232DeviceBase::rs232_get_aux16_lo()
{
    return cmdFrame.aux1 | (cmdFrame.aux2 << 8);
}

/**
 * @brief Get aux bytes as 16-bit value (high)
 * 
 * Combines aux3 and aux4 into a 16-bit value with aux3 as the low byte
 * and aux4 as the high byte.
 * 
 * @return aux3 + (aux4 << 8)
 */
uint16_t RS232DeviceBase::rs232_get_aux16_hi()
{
    return cmdFrame.aux3 | (cmdFrame.aux4 << 8);
}

/**
 * @brief Get all aux bytes as 32-bit value
 * 
 * Returns all four aux bytes as a single 32-bit value.
 * The union in cmdFrame_t allows direct access to this value.
 * 
 * @return All aux bytes combined as 32-bit value
 */
uint32_t RS232DeviceBase::rs232_get_aux32()
{
    return cmdFrame.aux;
}

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
void RS232BusBase::addDevice(RS232DeviceBase *pDevice, int device_id)
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
 * @param buff Buffer to send
 * @param len Length of buffer
 * @param err Error flag (true = send ERROR, false = send COMPLETE)
 */
void RS232DeviceBase::bus_to_computer(uint8_t *buff, uint16_t len, bool err)
{
    if (err)
    {
        rs232_error();
        return;
    }
    
    // Send data
    _bus->write(buff, len);
    
    // Send checksum
    uint8_t cksum = RS232Protocol::calculate_checksum(buff, len);
    _bus->write(cksum);
    
    // Send complete
    rs232_complete();
}

/**
 * @brief Receive data from computer (shared implementation)
 *
 * Receives data buffer from computer and returns the checksum.
 * This is the standard RS232 data reception protocol used by all platforms.
 *
 * @param buff Buffer to receive into
 * @param len Length to receive
 * @return Checksum byte from computer
 */
uint8_t RS232DeviceBase::bus_to_peripheral(uint8_t *buff, uint16_t len)
{
    // Read data
    _bus->read(buff, len);
    
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
RS232DeviceBase* RS232BusBase::deviceById(int device_id)
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
void RS232BusBase::changeDeviceId(RS232DeviceBase *pDevice, int device_id)
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
void RS232BusBase::processCommand(RS232Protocol::cmdFrame_t *cmd)
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