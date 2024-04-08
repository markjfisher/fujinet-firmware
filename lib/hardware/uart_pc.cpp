#ifdef USE_NEW_MODEM_IF

#include <cstring>
#include <string>
#include <stdexcept>

#include "uart_pc.h"

uart_pc::uart_pc(const std::string port_name, uint32_t baudrate) : uart(baudrate) {
    this->port_name = port_name;
    port = nullptr;
}

uart_pc::~uart_pc() {
    end();
}

void uart_pc::begin() {
    if (sp_get_port_by_name(port_name.c_str(), &port) != SP_OK) {
        throw std::runtime_error("Failed to get port.");
    }
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK) {
        throw std::runtime_error("Failed to open port.");
    }
    sp_set_baudrate(port, get_baudrate());
}

void uart_pc::end() {
    if (port) {
        sp_close(port);
        sp_free_port(port);
        port = nullptr;
    }
}

bool uart_pc::initialized() const {
    return port != nullptr;
}

int uart_pc::available() const {
    return 0; // Placeholder
}

int uart_pc::peek() const {
    return -1; // Placeholder
}

void uart_pc::flush() {
    if (initialized()) {
        // Flush both input and output buffers
        sp_flush(port, SP_BUF_BOTH);
    }
}

void uart_pc::flush_input() {
    if (initialized()) {
        // Flush only the input buffer
        sp_flush(port, SP_BUF_INPUT);
    }
}

int uart_pc::read() {
    uint8_t byte;
    if (sp_blocking_read(port, &byte, 1, 0) > 0) {
        return byte;
    }
    return -1;
}

size_t uart_pc::read_bytes(uint8_t *buffer, size_t length) {
    return sp_blocking_read(port, buffer, length, 0);
}

size_t uart_pc::write(uint8_t byte) {
    return sp_blocking_write(port, &byte, 1, 0);
}

size_t uart_pc::write(const uint8_t *buffer, size_t size) {
    return sp_blocking_write(port, buffer, size, 0);
}

size_t uart_pc::write(const char *s) {
    return write(reinterpret_cast<const uint8_t*>(s), strlen(s));
}

#endif