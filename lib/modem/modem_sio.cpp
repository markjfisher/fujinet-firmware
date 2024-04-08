#ifdef BUILD_ATARI
#ifdef USE_NEW_MODEM_IF

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "modem_sio.h"
#include "fnConfig.h"
#include "fnSystem.h"

#ifdef ESP_PLATFORM
#include "uart_esp.h"
#else
#include "uart_pc.h"
#endif



modem_sio::modem_sio(std::unique_ptr<TelnetEventHandler> handler, std::unique_ptr<ModemSniffer> sniffer) : modem_if(std::move(handler), std::move(sniffer)) {

#ifdef ESP_PLATFORM
    std::unique_ptr<uart> _uart = std::make_unique<uart_esp>();
#else
    std::unique_ptr<uart> _uart = std::make_unique<uart_pc>(Config.get_serial_port(), static_cast<uint32_t>(std::stoul(Config.get_serial_port_baud())));
#endif
    set_uart(std::move(_uart));
}

void modem_sio::sio_send_firmware(uint8_t loadcommand)
{
    // Map load commands to firmware data and sizes
    const std::map<uint8_t, std::pair<std::string, int>> firmwareMap = {
        {SIO_MODEMCMD_LOAD_RELOCATOR, {FIRMWARE_850RELOCATOR, 333}},
        {SIO_MODEMCMD_LOAD_HANDLER, {FIRMWARE_850HANDLER, 1282}}
    };

    auto it = firmwareMap.find(loadcommand);
    if (it == firmwareMap.end()) {
        return; // Command not found, exit the function
    }

    const auto& [firmware, firmware_size] = it->second;    std::vector<uint8_t> code(firmware_size);
    int codesize = fnSystem.load_firmware(firmware.c_str(), code.data());
    if (codesize <= 0) {
        // NAK if we failed to get this
        sio_nak();
        return;
    }

    sio_ack();

    // TODO: validate this statement as it's a 5 second delay.
    // We need a delay here when working in high-speed mode.
    // Doesn't negatively affect normal speed operation.
    fnSystem.delay_microseconds(DELAY_FIRMWARE_DELIVERY);

    Debug_printf("Modem sending %d bytes of %s code\n",
        codesize,
        loadcommand == SIO_MODEMCMD_LOAD_RELOCATOR ? "relocator" : "handler"
    );

    bus_to_computer(code.data(), codesize, false);

    set_dtr(false);
    set_xmt(false);
    set_rts(false);
}

void modem_sio::sio_poll_1()
{
    /*  From Altirra sources - rs232.cpp
        Send back SIO command for booting. This is a 12 uint8_t + chk block that
        is meant to be written to the SIO parameter block starting at DDEVIC ($0300).

		The boot block MUST start at $0500. There are both BASIC-based and cart-based
        loaders that use JSR $0506 to run the loader.
    */

    sio_ack();

    uint8_t bootBlock[12] = {
        0x50,       // DDEVIC
        0x01,       // DUNIT
        0x21,       // DCOMND = '!' (boot)
        0x40,       // DSTATS
        0x00, 0x05, // DBUFLO, DBUFHI == $0500
        0x08,       // DTIMLO = 8 vblanks
        0x00,       // not used
        0x4D, 0x01, // DBYTLO, DBYTHI = 333 decimal = size of FIRMWARE_850RELOCATOR
        0x00,       // DAUX1
        0x00,       // DAUX2
    };

    Debug_println("Modem acknowledging Type 1 Poll after 10s delay...");
    fnSystem.delay_microseconds(DELAY_FIRMWARE_DELIVERY * 2);
    Debug_println("... sending boot block.");
    bus_to_computer(bootBlock, sizeof(bootBlock), false);
}

void modem_sio::sio_control()
{
    /* AUX1: Set control state
        7: Enable DTR (Data Terminal Ready) change (1=change, 0=ignore)
        6: New DTR state (0=negate, 1=assert)
        5: Enable RTS (Request To Send) change
        4: New RTS state
        3: NA
        2: NA
        1: Enable XMT (Transmit) change
        0: New XMT state
      AUX2: NA
    */

    Debug_println("Modem cmd: CONTROL");

    if (cmdFrame.aux1 & 0x02)
    {
        set_xmt(cmdFrame.aux1 & 0x01 ? true : false);
        Debug_printf("XMT=%d\n", get_xmt());
    }

    if (cmdFrame.aux1 & 0x20)
    {
        set_rts(cmdFrame.aux1 & 0x10 ? true : false);
        Debug_printf("RTS=%d\n", get_rts());
    }

    if (cmdFrame.aux1 & 0x80)
    {
        set_dtr(cmdFrame.aux1 & 0x40 ? true : false);
        Debug_printf("DTR=%d\n", get_dtr());

        if (get_dtr() == 0 && get_tcp_client()->connected())
        {
            get_tcp_client()->stop(); // Hang up if DTR drops.
            set_crx(false);
            set_cmd_mode(true);
        }
    }
    sio_complete();
}


void modem_sio::sio_config()
{
    Debug_println("Modem cmd: CONFIGURE");
    sio_complete();
    if (baud_lock) return;

    /* AUX1:
         7: Stop bits (0=1, 1=2)
         6: NA
       4,5: Word size (00=5,01=6,10=7,11=8)
       3-0: Baud rate:
    */
    static const std::map<uint8_t, int> code_to_baud = {
        {0x08, 300},
        {0x09, 600},
        {0x0A, 1200},
        {0x0B, 1800},
        {0x0C, 2400},
        {0x0D, 4800},
        {0x0E, 9600},
        {0x0F, 19200}
    };

    uint8_t baud_aux1 = cmdFrame.aux1 & 0x0F;
    unsigned int new_word_size = ((cmdFrame.aux1 & 0x30) >> 4) + 5; // convert 2 bits to size
    unsigned int new_stop_bits = (cmdFrame.aux1 >> 7) + 1; // convert high bit to count

    int new_baud = code_to_baud.count(baud_aux1) ? code_to_baud.at(baud_aux1) : -1;
    if (new_baud == -1) {
        Debug_printf("Unexpected baud value: %hu\n", baud_aux1);
        new_baud = 300;
    }

    set_modem_baud(new_baud);
    set_word_size(new_word_size);
    set_stop_bits(new_stop_bits);
}

void modem_sio::sio_set_dump()
{
    get_modem_sniffer()->setEnable(cmdFrame.aux1);
    sio_complete();
}

void modem_sio::sio_listen()
{
    if (get_listen_port() != 0)
    {
        get_tcp_client()->stop();
        get_tcp_server()->stop();
    }

    ma_blink();

    unsigned short new_port = sio_get_aux();

    if (new_port < 1) {
        sio_nak();
        return;
    }
    set_listen_port(new_port);
    sio_ack();

    get_tcp_server()->setMaxClients(1);
    int res = get_tcp_server()->begin(new_port);
    if (res == 0)
        sio_error();
    else
        sio_complete();
}

void modem_sio::sio_unlisten()
{
    sio_ack();
    ma_blink();
    get_tcp_client()->stop();
    get_tcp_server()->stop();
    sio_complete();
}

void modem_sio::sio_baudlock()
{
    sio_ack();
    set_modem_baud(sio_get_aux());
    set_baud_lock(get_modem_baud() > 0);

    ma_blink();

    Debug_printf("baud_lock: %d\n", get_baud_lock());
    sio_complete();
}

void modem_sio::sio_autoanswer()
{
    sio_ack();
    set_auto_answer(cmdFrame.aux1 > 0);

    ma_blink();

    Debug_printf("auto_answer: %d\n", get_auto_answer());
    sio_complete();
}

void modem_sio::sio_status()
{
    // Let's generously say that modem_status[0] is for "future expansions"
    uint8_t modem_status[2] = {0x00, 0x00};
    Debug_println("Modem cmd: STATUS");

    /* AUX1: NA
       AUX2: NA
       First payload uint8_t = error status bits
      Second payload uint8_t = handshake state bits
             00 Always low since last check
             01 Currently low, but has been high since last check
             10 Currently high, but has been low since last check
             11 Always high since last check
        7,6: DSR state
        5,4: CTS state
        3,2: CTX state
          1: 0
          0: RCV state (0=space, 1=mark)
    */

    bool isConnected = get_tcp_client()->connected();
    bool isAvailable = get_tcp_client()->available() > 0;

    // only check hasClient if absolutely necessary
    if (!isConnected || !isAvailable || get_auto_answer()) {
        bool hasClient = get_tcp_server()->hasClient();
        isConnected |= hasClient;
        isAvailable |= hasClient;

        if (get_auto_answer() && hasClient) {
            set_modem_active(true);
            set_answered(false);
            set_answer_timer(fnSystem.millis());
        }
    }

    modem_status[1] = (isConnected ? 0b11001100 : 0) |
                      (isAvailable ? 0b00000001 : 0);

    Debug_printf("modem::sio_status(%02x,%02x)\n", modem_status[0], modem_status[1]);
    bus_to_computer(modem_status, sizeof(modem_status), false);
}

void modem_sio::sio_write()
{
    uint8_t ck;

    Debug_println("Modem cmd: WRITE");

    /* AUX1: Bytes in payload, 0-64
       AUX2: NA
       Payload always padded to 64 bytes
    */
    if (cmdFrame.aux1 == 0)
    {
        sio_complete();
        return;
    }

    memset(tx_buf, 0, sizeof(tx_buf));

    ck = bus_to_peripheral(tx_buf, 64);
    if (ck != sio_checksum(tx_buf, 64))
    {
        sio_error();
        return;
    }

    if (get_cmd_mode())
    {
        set_output_cmd(false);
        set_cmd((char *)tx_buf, cmdFrame.aux1);

        if (get_cmd() == "ATA\r")
            set_answer_hack(true);
        else
            modem_command();

        set_output_cmd(true);
    }
    else
    {
        ma_blink();
        if (get_tcp_client()->connected())
            get_tcp_client()->write(tx_buf, cmdFrame.aux1);
    }

    sio_complete();
}

void modem_sio::sio_stream()
{
    Debug_println("Modem cmd: STREAM");

    // Pokey baud rates ($D200-$D208)
    static const std::map<unsigned int, std::vector<unsigned char>> baud_rate_map = {
        {   300, {0xA0, 0x0B}},
        {   600, {0xCC, 0x05}},
        {  1200, {0xE3, 0x02}},
        {  1800, {0xEA, 0x01}},
        {  2400, {0x6E, 0x01}},
        {  4800, {0xB3, 0x00}},
        {  9600, {0x56, 0x00}},
        // { 19200, {0x28, 0x00}}, // default is this, so no need to change
    };

    // Default response for 19200 baud rate
    std::vector<unsigned char> response = {0x28, 0xA0, 0x00, 0xA0, 0x28, 0xA0, 0x00, 0xA0, 0x78};

    auto it = baud_rate_map.find(get_modem_baud());
    if (it != baud_rate_map.end()) {
        response[0] = response[4] = it->second[0];
        response[2] = response[6] = it->second[1];
    }

    bus_to_computer(response.data(), response.size(), false);
    
    fnSystem.delay_microseconds(DELAY_FIRMWARE_DELIVERY); // macOS workaround (flush on uart was not working) - TODO may not be an issue if move to libserial
    get_uart()->set_baudrate(get_modem_baud());
    set_modem_active(true);
    Debug_printf("Modem streaming at %u baud\n", get_modem_baud());
}

void modem_sio::sio_process(uint32_t commanddata, uint8_t checksum)
{
    cmdFrame.commanddata = commanddata;
    cmdFrame.checksum = checksum;

    if (!Config.get_modem_enabled()) {
        Debug_println("modem_sio::sio_process - modem disabled, ignoring call");
        return;
    }

    Debug_println("modem_sio::sio_process() called");

    switch (cmdFrame.comnd)
    {
    case SIO_MODEMCMD_LOAD_RELOCATOR:
        count_ReqRelocator++;
        Debug_printf("MODEM $21 RELOCATOR #%d\n", count_ReqRelocator);
        sio_send_firmware(cmdFrame.comnd);
        break;

    case SIO_MODEMCMD_LOAD_HANDLER:
        count_ReqHandler++;
        Debug_printf("MODEM $26 HANDLER DL #%d\n", count_ReqHandler);
        sio_send_firmware(cmdFrame.comnd);
        break;

    case SIO_MODEMCMD_TYPE1_POLL:
        count_PollType1++;
        Debug_printf("MODEM TYPE 1 POLL #%d\n", count_PollType1);
        // The 850 is only supposed to respond to this if AUX1 = 1 or on the Xth poll attempt
        if (cmdFrame.aux1 == 1 || count_PollType1 == 16)
        {
            sio_poll_1();
            count_PollType1 = 0; // Reset the counter so we can respond again if asked
        }
        break;

    //// Used to handle this, now going to just drop through to NAK
    // case SIO_MODEMCMD_TYPE3_POLL:
    //     break;

    case SIO_MODEMCMD_CONTROL:
        sio_ack();
        sio_control();
        break;
    case SIO_MODEMCMD_CONFIGURE:
        sio_ack();
        sio_config();
        break;
    case SIO_MODEMCMD_SET_DUMP:
        sio_ack();
        sio_set_dump();
        break;
    case SIO_MODEMCMD_LISTEN:
        sio_listen();
        break;
    case SIO_MODEMCMD_UNLISTEN:
        sio_unlisten();
        break;
    case SIO_MODEMCMD_BAUDRATELOCK:
        sio_baudlock();
        break;
    case SIO_MODEMCMD_AUTOANSWER:
        sio_autoanswer();
        break;
    case SIO_MODEMCMD_STATUS:
        sio_ack();
        sio_status();
        break;
    case SIO_MODEMCMD_WRITE:
        sio_late_ack();
        sio_write();
        break;
    case SIO_MODEMCMD_STREAM:
        sio_ack();
        sio_stream();
        break;
    default:
        sio_nak();
    }

}

#endif // USE_NEW_MODEM_IF
#endif // BUILD_ATARI
