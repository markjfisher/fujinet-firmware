#ifdef USE_NEW_MODEM_IF

#include <cstring>
#include <soc/uart_reg.h>
#include <hal/gpio_types.h>

#include "../../include/pinmap.h"
#include "uart_esp.h"

#define MAX_FLUSH_WAIT_TICKS 200
#define MAX_READ_WAIT_TICKS 200

uart_esp::uart_esp(uart_port_t _port, uint32_t _baudrate) : uart(_baudrate), uart_q(NULL), port(_port) {}

uart_esp::~uart_esp() {
    end();
}

void uart_esp::begin() {
    if (uart_q) end();

    uart_config_t uart_config = {
        .baud_rate = baud,
        .data_bits = UART_DATA_8_BITS,
#ifdef BUILD_LYNX
        .parity = UART_PARITY_ODD,
#else
        .parity = UART_PARITY_DISABLE,
#endif /* BUILD_LYNX */
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122, // No idea what this is for, but shouldn't matter if flow ctrl is disabled?
        .source_clk = UART_SCLK_DEFAULT
    };

    uart_param_config(port, &uart_config);

    int tx, rx;
    if (port == 0)
    {
        rx = PIN_UART0_RX;
        tx = PIN_UART0_TX;
    }
    else if (port == 1)
    {
        rx = PIN_UART1_RX;
        tx = PIN_UART1_TX;
    }
#ifndef BUILD_RS232
    else if (port == 2)
    {
        rx = PIN_UART2_RX;
        tx = PIN_UART2_TX;
    }
#endif /* BUILD_RS232 */
    else
    {
        return;
    }

    uart_set_pin(port, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

#ifdef BUILD_ADAM
    if (port == 2)
        uart_set_line_inverse(port, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV);
#endif /* BUILD_ADAM */

#ifdef BUILD_COCO
    if (port == 2)
        uart_set_line_inverse(port, UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV);
#endif /* BUILD_COCO */

    // Arduino default buffer size is 256
    int uart_buffer_size = 256;
    int uart_queue_size = 10;
    int intr_alloc_flags = 0;

    // Install UART driver using an event queue here
    // uart_driver_install(port, uart_buffer_size, uart_buffer_size, uart_queue_size, &_uart_q, intr_alloc_flags);
    uart_driver_install(port, uart_buffer_size, 0, uart_queue_size, NULL, intr_alloc_flags);

#ifdef BUILD_ADAM
    uart_intr_config_t uart_intr;
    uart_intr.intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M | UART_FRM_ERR_INT_ENA_M | UART_RXFIFO_OVF_INT_ENA_M | UART_BRK_DET_INT_ENA_M | UART_PARITY_ERR_INT_ENA_M;
    uart_intr.rxfifo_full_thresh = 1;        // UART_FULL_THRESH_DEFAULT,  //120 default!! aghh! need receive 120 chars before we see them
    uart_intr.rx_timeout_thresh = 10;        // UART_TOUT_THRESH_DEFAULT,  //10 works well for my short messages I need send/receive
    uart_intr.txfifo_empty_intr_thresh = 2; // UART_EMPTY_THRESH_DEFAULT
    uart_intr_config(port, &uart_intr);
#endif /* BUILD_ADAM */

#ifdef BUILD_LYNX
    uart_intr_config_t uart_intr;
    uart_intr.intr_enable_mask = UART_RXFIFO_FULL_INT_ENA_M | UART_RXFIFO_TOUT_INT_ENA_M | UART_FRM_ERR_INT_ENA_M | UART_RXFIFO_OVF_INT_ENA_M | UART_BRK_DET_INT_ENA_M | UART_PARITY_ERR_INT_ENA_M;
    uart_intr.rxfifo_full_thresh = 1;        // UART_FULL_THRESH_DEFAULT,  //120 default!! aghh! need receive 120 chars before we see them
    uart_intr.rx_timeout_thresh = 10;        // UART_TOUT_THRESH_DEFAULT,  //10 works well for my short messages I need send/receive
    uart_intr.txfifo_empty_intr_thresh = 2; // UART_EMPTY_THRESH_DEFAULT
    uart_intr_config(port, &uart_intr);
#endif /* BUILD_LYNX */

    is_initialized = true;
}

void uart_esp::end() {
    uart_driver_delete(port);
    if (uart_q)
        vQueueDelete(uart_q);
    uart_q = NULL;
}

bool uart_esp::initialized() const {
    return is_initialized;
}

int uart_esp::available() const {
    size_t result;
    if (ESP_FAIL == uart_get_buffered_data_len(port, &result))
        return -1;
    return result;
}

// Not implemented
int uart_esp::peek() const {
    return 0;
}

void uart_esp::flush() {
    uart_wait_tx_done(port, MAX_FLUSH_WAIT_TICKS);
}

void uart_esp::flush_input() {
}

int uart_esp::read() {
    uint8_t byte;
    int result = uart_read_bytes(port, &byte, 1, MAX_READ_WAIT_TICKS);
    if (result < 1) {
#ifdef DEBUG
        if (result == 0)
            Debug_println("### UART read() TIMEOUT ###");
        else
            Debug_printf("### UART read() ERROR %d ###\r\n", result);
#endif
        return -1;
    }

    return byte;
}

size_t uart_esp::read_bytes(uint8_t *buffer, size_t length) {
    int result = uart_read_bytes(port, buffer, length, MAX_READ_WAIT_TICKS);
#ifdef DEBUG
    if (result < length) {
        if (result < 0)
            Debug_printf("### UART readBytes() ERROR %d ###\r\n", result);
        // else
        //     Debug_println("### UART readBytes() TIMEOUT ###");
    }
#endif
    return result;
}

size_t uart_esp::write(uint8_t byte) {
    return uart_write_bytes(port, (const char *)&byte, 1);
}

size_t uart_esp::write(const uint8_t *buffer, size_t size) {
    return uart_write_bytes(port, (const char *)buffer, size);
}

size_t uart_esp::write(const char *s) {
    return uart_write_bytes(port, s, strlen(s));
}

void uart_esp::set_baudrate(uint32_t baud) override {
    baudrate = baud;
#ifdef DEBUG
    // this should match baudrate, but we'll ask the device for current baudrate anyway.
    uint32_t before;
    uart_get_baudrate(port, &before);
#endif
    uart_set_baudrate(port, baudrate);
#ifdef DEBUG
    Debug_printf("set_baudrate change from %d to %d\r\n", before, baud);
#endif
}

#endif
