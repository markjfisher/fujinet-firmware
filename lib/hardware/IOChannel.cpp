#include "IOChannel.h"

#include <stdarg.h>
#include "../../include/debug.h"
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#define GET_TIMESTAMP() esp_timer_get_time()
#elif defined(ITS_A_UNIX_SYSTEM_I_KNOW_THIS)
#include <sys/time.h>
#define GET_TIMESTAMP() ({ struct timeval _tv; gettimeofday(&_tv, NULL); \
            _tv.tv_sec * ((uint64_t) 1000000) + _tv.tv_usec; })
#endif /* ESP_PLATFORM */

size_t IOChannel::available()
{
    update_fifo();
    //Debug_printf("FN AVAIL: %i\r\n", _fifo.size());
    return _fifo.size();
}

size_t IOChannel::dataIn(void *buffer, size_t length)
{
    size_t rlen, total = 0;
    uint8_t *ptr;
    uint64_t now, start;

    ptr = (uint8_t *) buffer;
    now = start = GET_TIMESTAMP();
    
    // Debug_printf("IOChannel::dataIn - Requesting %zu bytes, timeout: %d ms\n", length, read_timeout_ms);
    // Debug_printf("test\n");
    
    while (length)
    {
        now = GET_TIMESTAMP();
        uint64_t elapsed_us = now - start;
        uint64_t timeout_us = read_timeout_ms * 1000;
        
        // Debug_printf("%zu\n", length);
        if (elapsed_us > timeout_us)
        {
            // Debug_printf("IOChannel::dataIn - TIMEOUT after %llu us (limit: %llu us), read %zu of %zu bytes\n", 
            //             elapsed_us, timeout_us, total, total + length);
            // Debug_printf("B");
            break;
        }
        
        size_t avail = available();
        rlen = std::min(length, avail);
        
        // Debug_printf("1");
        // Debug_printf("IOChannel::dataIn - Available: %zu, requesting: %zu, will read: %zu, total so far: %zu\n", 
        //             avail, length, rlen, total);
        
        if (!rlen)
        {
            // Debug_printf("IOChannel::dataIn - No data available, elapsed: %llu us\n", elapsed_us);
            // Debug_printf("R");
            continue;
        }
        
        memcpy(&ptr[total], _fifo.data(), rlen);
        _fifo.erase(0, rlen);
        total += rlen;
        length -= rlen;

        // Debug_printf("IOChannel::dataIn - Read %zu bytes, total: %zu, remaining: %zu\n", rlen, total, length);

        // We received data, reset timeout
        start = now;
        // Debug_printf("IOChannel::dataIn - Data received, resetting timeout\n");
    }

    // Debug_printf("IOChannel::dataIn - Completed: read %zu bytes total\n", total);
    return total;
}

void IOChannel::discardInput()
{
    uint64_t now, start;

    now = start = GET_TIMESTAMP();
    while (now - start < discard_timeout_ms * 1000)
    {
        now = GET_TIMESTAMP();
        if (_fifo.size())
        {
            _fifo.clear();
            start = now;
        }
    }
    return;
}

size_t IOChannel::read(void *buffer, size_t length)
{
    return dataIn(buffer, length);
}

int IOChannel::read(void)
{
    uint8_t buf[1];
    int result = read(buf, 1);

    if (result < 1)
        return result;
    return buf[0];
}

size_t IOChannel::write(const void *buffer, size_t length)
{
    return dataOut(buffer, length);
}

size_t IOChannel::write(uint8_t c)
{
    uint8_t buf[1];

    buf[0] = c;
    return write(buf, 1);
}

size_t IOChannel::write(const char *s)
{
    return write(s, strlen(s));
}

size_t IOChannel::write(unsigned long n)
{
    return write((uint8_t) n);
}

size_t IOChannel::write(long n)
{
    return write((uint8_t) n);
}

size_t IOChannel::write(unsigned int n)
{
    return write((uint8_t) n);
}

size_t IOChannel::write(int n)
{
    return write((uint8_t) n);
}

size_t IOChannel::printf(const char *fmt...)
{
    char *result = nullptr;
    va_list vargs;

    va_start(vargs, fmt);

    int z = vasprintf(&result, fmt, vargs);

    if (z > 0)
        write(result, z);

    va_end(vargs);

    if (result != nullptr)
        free(result);

    return z >= 0 ? z : 0;
}

size_t IOChannel::_print_number(unsigned long n, uint8_t base)
{
    char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
    char *str = &buf[sizeof(buf) - 1];

    *str = '\0';

    // prevent crash if called with base == 1
    if (base < 2)
        base = 10;

    do
    {
        unsigned long m = n;
        n /= base;
        char c = m - base * n;
        *--str = c < 10 ? c + '0' : c + 'A' - 10;
    } while (n);

    return write(str);
}

size_t IOChannel::print(const char *str)
{
    int z = strlen(str);

    return write(str, z);
    ;
}

size_t IOChannel::print(const std::string &str)
{
    return print(str.c_str());
}

size_t IOChannel::print(int n, int base)
{
    return print((long)n, base);
}

size_t IOChannel::print(unsigned int n, int base)
{
    return print((unsigned long)n, base);
}

size_t IOChannel::print(long n, int base)
{
    if (base == 0)
    {
        return write(n);
    }
    else if (base == 10)
    {
        if (n < 0)
        {
            int t = print('-');
            n = -n;
            return _print_number(n, 10) + t;
        }
        return _print_number(n, 10);
    }
    else
    {
        return _print_number(n, base);
    }
}

size_t IOChannel::print(unsigned long n, int base)
{
    if (base == 0)
    {
        return write(n);
    }
    else
    {
        return _print_number(n, base);
    }
}

size_t IOChannel::println(const char *str)
{
    size_t n = print(str);
    n += println();
    return n;
}

size_t IOChannel::println(std::string str)
{
    size_t n = print(str);
    n += println();
    return n;
}

size_t IOChannel::println(int num, int base)
{
    size_t n = print(num, base);
    n += println();
    return n;
}
