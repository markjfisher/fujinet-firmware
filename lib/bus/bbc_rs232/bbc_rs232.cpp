#include "bbc_rs232.h"

#include <cstring>
#include "fnConfig.h"

#include "../../include/debug.h"


// ========================================================================
// virtualDevice methods
// ========================================================================

/**
 * @brief Get the system bus
 */
systemBus virtualDevice::rs232_get_bus()
{
    return SYSTEM_BUS;
}

// ========================================================================
// systemBus methods
// ========================================================================

/**
 * @brief Initialize the BBC RS232 bus
 */
void systemBus::setup()
{
    Debug_printf("BBC_RS232::setup(): Baud rate: %u\n", Config.get_rs232_baud());

    // Set up UART
#ifndef FUJINET_OVER_USB
    _port.begin(ChannelConfig().baud(Config.get_rs232_baud()).deviceID(SERIAL_DEVICE));

#ifdef ESP_PLATFORM
    // TODO: (Fenrock) Is fnSystem available yet?
    // // INT PIN
    // fnSystem.set_pin_mode(PIN_RS232_RI, gpio_mode_t::GPIO_MODE_OUTPUT_OD, SystemManager::pull_updown_t::PULL_UP);
    // fnSystem.digital_write(PIN_RS232_RI, DIGI_HIGH);
    // PROC PIN
    fnSystem.set_pin_mode(PIN_RS232_RI, gpio_mode_t::GPIO_MODE_OUTPUT, SystemManager::pull_updown_t::PULL_UP);
    fnSystem.digital_write(PIN_RS232_RI, DIGI_HIGH);
    // INVALID PIN
    //fnSystem.set_pin_mode(PIN_RS232_INVALID, PINMODE_INPUT | PINMODE_PULLDOWN); // There's no PULLUP/PULLDOWN on pins 34-39
    fnSystem.set_pin_mode(PIN_RS232_INVALID, gpio_mode_t::GPIO_MODE_INPUT);
    // CMD PIN
    //fnSystem.set_pin_mode(PIN_RS232_DTR, PINMODE_INPUT | PINMODE_PULLUP); // There's no PULLUP/PULLDOWN on pins 34-39
    fnSystem.set_pin_mode(PIN_RS232_DTR, gpio_mode_t::GPIO_MODE_INPUT);
    // CKI PIN
    //fnSystem.set_pin_mode(PIN_CKI, PINMODE_OUTPUT);
    // CKO PIN

    fnSystem.set_pin_mode(PIN_RS232_CTS, gpio_mode_t::GPIO_MODE_OUTPUT);
    fnSystem.digital_write(PIN_RS232_CTS,DIGI_LOW);

    fnSystem.set_pin_mode(PIN_RS232_DSR,gpio_mode_t::GPIO_MODE_OUTPUT);
    fnSystem.digital_write(PIN_RS232_DSR,DIGI_LOW);
#endif /* ESP_PLATFORM */
#else /* FUJINET_OVER_USB */
    _port.begin();
#endif /* FUJINET_OVER_USB */

    _rs232Baud = BBC_RS232_BAUDRATE;
}

/**
 * @brief Set baud rate
 */
void systemBus::setBaudrate(int baud)
{
    Debug_printf("Setting baud rate to %d\n", baud);
    _rs232Baud = baud;
    _port.setBaudrate(baud);
}

/**
 * @brief Toggle between standard and high speed
 */
void systemBus::toggleBaudrate()
{
    // this is some left over from atari HSIO stuff that shouldn't be part of this protcol
}