#include "uart.h"

uart::uart(uint32_t baudrate) : _baudrate(baudrate) {}

uint32_t uart::get_baudrate() const { return _baudrate; }
void uart::set_baudrate(uint32_t baudrate) { _baudrate = baudrate; }
