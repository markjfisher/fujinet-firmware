#ifdef USE_NEW_MODEM_IF

#include <cstdint>
#include "uart.h"

uart::uart(uint32_t _baudrate) : baudrate(_baudrate) {}

uint32_t uart::get_baudrate() const { return baudrate; }
void uart::set_baudrate(uint32_t _baudrate) { baudrate = _baudrate; }

size_t uart::_print_number(unsigned long n, uint8_t base) {
    std::string buf;

    // avoid errors if base is 1 or 0
    if (base < 2) {
        base = 10;
    }

    do {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        c = c < 10 ? c + '0' : c + 'A' - 10;
        buf.insert(0, 1, c); // Prepend character to buffer string
    } while (n);

    return write(buf.c_str());
}

size_t uart::print(const char *str) {
    return write(str);
}

size_t uart::print(const std::string &str) {
    return write(str.c_str());
}

size_t uart::print(int n, int base) {
    return print((long) n, base);
}

size_t uart::print(unsigned int n, int base) {
    return print((unsigned long) n, base);
}

size_t uart::print(long n, int base) {
    if (base == 0) {
        return write(n);
    }

    size_t written = 0;

    // Handle negative numbers for base 10
    if (base == 10 && n < 0) {
        written += print("-");
        n = -n;
    }

    written += _print_number(n, base);
    return written;
}

size_t uart::print(unsigned long n, int base) {
    if (base == 0) {
        return write(n);
    }

    return _print_number(n, base);
}

#endif // USE_NEW_MODEM_IF
