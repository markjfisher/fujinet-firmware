#ifndef UART_PC_H
#define UART_PC_H

#include "uart.h"
#include <libserialport.h>

class uart_pc : public uart {
private:
    struct sp_port *port;
    char port_name[1024];

public:
    uart_pc(const char* portName, uint32_t baudrate);
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

#endif // UART_PC_H
