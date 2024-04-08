#ifndef MODEM_SIO_H
#define MODEM_SIO_H

#ifdef BUILD_ATARI
#ifdef USE_NEW_MODEM_IF

#define MODEM_TYPE modem_sio

#include "modem_if.h"

#if defined(ESP_PLATFORM)
#include "uart_esp.h"
#else
#include "uart_pc.h"
#endif

#include "fnConfig.h"

#define SIO_MODEMCMD_LOAD_RELOCATOR 0x21
#define SIO_MODEMCMD_LOAD_HANDLER 0x26
#define SIO_MODEMCMD_TYPE1_POLL 0x3F
#define SIO_MODEMCMD_TYPE3_POLL 0x40
#define SIO_MODEMCMD_CONTROL 0x41
#define SIO_MODEMCMD_CONFIGURE 0x42
#define SIO_MODEMCMD_SET_DUMP 0x44
#define SIO_MODEMCMD_LISTEN 0x4C
#define SIO_MODEMCMD_UNLISTEN 0x4D
#define SIO_MODEMCMD_BAUDRATELOCK 0x4E
#define SIO_MODEMCMD_AUTOANSWER 0x4F
#define SIO_MODEMCMD_STATUS 0x53
#define SIO_MODEMCMD_WRITE 0x57
#define SIO_MODEMCMD_STREAM 0x58

#define FIRMWARE_850RELOCATOR "/850relocator.bin"
#define FIRMWARE_850HANDLER "/850handler.bin"

#define DELAY_FIRMWARE_DELIVERY 5000

class modem_sio : public virtualDevice, modem_if {
public:
    modem_sio(std::unique_ptr<TelnetEventHandler> handler, std::unique_ptr<ModemSniffer> sniffer);
    // ~modem_sio() override;

    using TelnetHandlerType = DefaultTelnetEventHandler;

    bool get_baud_lock() const { return baud_lock; }
    void set_baud_lock(const bool locked) { baud_lock = locked; }

private:
    int count_PollType1 = 0;

    int count_ReqRelocator = 0;
    int count_ReqHandler = 0;
    bool firmware_sent = false;
    bool baud_lock = false;

    void sio_send_firmware(uint8_t loadcommand); // $21 and $26: Booter/Relocator download; Handler download
    void sio_poll_1();                           // $3F, '?', Type 1 Poll
    // NO LONGER REQUIRED: void sio_poll_3(uint8_t device, uint8_t aux1, uint8_t aux2); // $40, '@', Type 3 Poll
    void sio_control();                          // $41, 'A', Control
    void sio_config();                           // $42, 'B', Configure
    void sio_set_dump();                         // $$4, 'D', Dump
    void sio_listen();                           // $4C, 'L', Listen
    void sio_unlisten();                         // $4D, 'M', Unlisten
    void sio_baudlock();                         // $4E, 'N', Baud lock
    void sio_autoanswer();                       // $4F, 'O', auto answer
    void sio_status() override;                  // $53, 'S', Status
    void sio_write();                            // $57, 'W', Write
    void sio_stream();                           // $58, 'X', Concurrent/Stream
    void sio_process(uint32_t commanddata, uint8_t checksum) override;

};

#endif // USE_NEW_MODEM_IF
#endif // BUILD_ATARI
#endif // MODEM_SIO_H
