#ifndef BBC_RS232_H
#define BBC_RS232_H

// Include shared RS232 protocol base
#include "../rs232_base/rs232_protocol.h"

#ifdef ESP_PLATFORM
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

// Use the same channel types as regular RS232
#include "../../hardware/ACMChannel.h"
#include "../../hardware/UARTChannel.h"
#include "../../hardware/TTYChannel.h"

/**
 * @brief BBC RS232/RS423 Bus Implementation
 * 
 * This bus handles communication between BBC Micro computers and FujiNet
 * via RS232/RS423 serial connection. The BBC Micro uses RS423 which is
 * electrically compatible with RS232 but uses different voltage levels.
 * 
 * BBC RS423 Specifications:
 * - Baud rates: 75, 150, 300, 1200, 2400, 4800, 9600, 19200
 * - Data bits: 7 or 8
 * - Stop bits: 1 or 2
 * - Parity: None, Even, Odd
 * - Flow control: RTS/CTS or XON/XOFF
 * 
 * Default configuration:
 * - 9600 baud
 * - 8 data bits
 * - 1 stop bit
 * - No parity
 */

#define BBC_RS232_BAUDRATE 19200

// Use shared protocol definitions
using RS232Protocol::cmdFrame_t;

// Forward declarations
class bbcRS232Fuji;
class systemBus;

/**
 * @brief BBC RS232 virtual device class
 *
 * Inherits from RS232DeviceBase to get ALL protocol functionality.
 * No BBC-specific code needed - everything is inherited!
 */
class virtualDevice : public RS232DeviceBase
{
protected:
    friend systemBus;

    /**
     * @brief Get the systemBus object
     */
    systemBus rs232_get_bus();
};

/**
 * @brief BBC RS232 system bus manager
 *
 * Inherits from RS232BusBase to get shared bus management functionality.
 * Implements BBC-specific bus operations (setup, service, shutdown).
 */
class systemBus : public RS232BusBase
{
private:
    // BBC-specific device pointer (only Fuji for now)
    bbcRS232Fuji *_fujiDev = nullptr;
    
    int _rs232BaudHigh = BBC_RS232_BAUDRATE;

public:
    /**
     * @brief Initialize the bus (override from base)
     *
     * Only method BBC must implement - initializes the serial port.
     */
    void setup() override;
    
    /**
     * @brief Set baud rate
     * @param baud New baud rate
     */
    void setBaudrate(int baud);
    
    /**
     * @brief Toggle between standard and high speed
     */
    void toggleBaudrate();
};

extern systemBus SYSTEM_BUS;

#endif // BBC_RS232_H