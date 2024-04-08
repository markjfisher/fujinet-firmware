#ifndef UART_PC_H
#define UART_PC_H

#ifdef USE_NEW_MODEM_IF

#include <libserialport.h>
#include "uart.h"

class uart_pc : public uart {
private:
    struct sp_port *port;
    std::string port_name;

public:
    uart_pc(const std::string port_name, uint32_t baudrate);
    virtual ~uart_pc();

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

#endif // USE_NEW_MODEM_IF
#endif // UART_PC_H
