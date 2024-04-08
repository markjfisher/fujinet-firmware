#ifndef UART_BASE_H
#define UART_BASE_H

#ifdef USE_NEW_MODEM_IF


#include <cstdint>
#include <cstddef>
#include <string>

class uart {
public:
  virtual ~uart() = default;
  uart(uint32_t _baudrate);

  // Initialization and configuration
  virtual void begin() = 0;
  virtual void end() = 0;
  virtual bool initialized() const = 0;

  virtual void set_baudrate(uint32_t baud);
  uint32_t get_baudrate() const;

  // Data availability and manipulation
  virtual int available() const = 0;
  virtual int peek() const = 0;
  virtual void flush() = 0;
  virtual void flush_input() = 0;

  // Reading data
  virtual int read() = 0;
  virtual size_t read_bytes(uint8_t* buffer, size_t length) = 0;

  // Writing data
  virtual size_t write(uint8_t data) = 0;
  virtual size_t write(const uint8_t* buffer, size_t size) = 0;
  virtual size_t write(const char* str) = 0;

  // print convenience functions that utilize the write functions above
  size_t print(const char *str);
  size_t print(const std::string &str);
  size_t print(int n, int base = 10);
  size_t print(unsigned int n, int base = 10);
  size_t print(long n, int base = 10);
  size_t print(unsigned long n, int base = 10);

// TODO: are these needed in the interface?
//   virtual bool command_asserted() const = 0;
//   virtual void set_proceed(bool level) = 0;

private:
	uint32_t baudrate;

  size_t _print_number(unsigned long n, uint8_t base);

};


#endif // USE_NEW_MODEM_IF
#endif // UART_BASE_H
