#ifndef UART_BASE_H
#define UART_BASE_H

#include <cstdint>
#include <cstddef>

class uart {
public:
  virtual ~uart() = default;
  uart(uint32_t baudrate);

  // Initialization and configuration
  virtual void begin() = 0;
  virtual void end() = 0;
  virtual bool initialized() const = 0;

  void set_baudrate(uint32_t baud);
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

// TODO: are these needed in the interface?
//   virtual bool command_asserted() const = 0;
//   virtual void set_proceed(bool level) = 0;

private:
	uint32_t _baudrate;
};

#endif // UART_BASE_H
