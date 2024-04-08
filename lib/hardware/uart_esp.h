#ifndef UART_ESP_H
#define UART_ESP_H

#ifdef USE_NEW_MODEM_IF

#ifdef ESP_PLATFORM

#include <driver/uart.h>
#include "uart.h"

#define UART_DEBUG UART_NUM_0
#ifdef BUILD_RS232
#define UART_SIO UART_NUM_1
#else
#define UART_SIO UART_NUM_2
#endif

class uart_esp : public uart {
private:
    uart_port_t port;
    QueueHandle_t uart_q;
    bool is_initialized = false;

public:
    uart_esp(uart_port_t _port, uint32_t _baudrate);
    virtual ~uart_esp();

    void begin() override;
    void end() override;
    bool initialized() const override;

    int available() const override;
    int peek() const override;
    void flush() override;
    void flush_input() override;

    int read() override;
    size_t read_bytes(uint8_t *buffer, size_t length) override;

    size_t write(uint8_t) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t write(const char *s) override;
};

#endif // ESP_PLATFORM
#endif // USE_NEW_MODEM_IF
#endif // UART_ESP_H
