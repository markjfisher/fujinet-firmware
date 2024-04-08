#ifndef MODEM_IF_H
#define MODEM_IF_H

#ifdef USE_NEW_MODEM_IF

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "modem_constants.h"

#include "bus.h"
#include "fnTcpClient.h"
#include "fnTcpServer.h"
#include "modem-sniffer.h"
#include "libtelnet.h"

#include "uart.h"
#include "telnetEventHandler.h"

using CommandHandler = std::function<void()>;

// not a fan of hardcoded constants, this should be configured
// milliseconds to wait before issuing CONNECT command, to simulate carrier negotiation.
#define ANSWER_TIMER_MS 2000
#define TX_BUF_SIZE 256

struct TelnetDeleter {
    void operator()(telnet_t* ptr) const {
        if (ptr != nullptr) {
            telnet_free(ptr);
        }
    }
};

class modem_if {

public:
    virtual ~modem_if();
    modem_if(std::unique_ptr<TelnetEventHandler> handler, std::unique_ptr<ModemSniffer> sniffer);

    template<typename ModemType>
    static std::unique_ptr<modem_if> create_modem(std::unique_ptr<ModemSniffer> sniffer) {
        auto handler = std::make_unique<typename ModemType::TelnetHandlerType>();
        return std::make_unique<ModemType>(std::move(handler), std::move(sniffer));
    }

    void modem_command();

    ModemSniffer *get_modem_sniffer() { return modem_sniffer.get(); }

    void set_uart(std::unique_ptr<uart> uart_obj) { uart_instance = std::move(uart_obj); }
    uart* get_uart() const { return uart_instance.get(); }

    fnTcpClient* get_tcp_client() { return tcp_client.get(); }
    fnTcpServer* get_tcp_server() { return tcp_server.get(); }

    unsigned int get_modem_baud() const { return modem_baud; }
    void set_modem_baud(const unsigned int baud) { modem_baud = baud; }

    unsigned int get_word_size() const { return word_size; }
    void set_word_size(const unsigned int size) { word_size = size; }

    unsigned int get_stop_bits() const { return stop_bits; }
    void set_stop_bits(const unsigned int bits) { stop_bits = bits; }

    bool get_dtr() const { return DTR; }
    void set_dtr(const bool dtr) { DTR = dtr; }

    bool get_xmt() { return XMT; }
    void set_xmt(const bool xmt) { XMT = xmt; }

    bool get_rts() const { return RTS; }
    void set_rts(const bool rts) { RTS = rts; }

    bool get_crx() const { return CRX; }
    void set_crx(const bool crx) { CRX = crx; }

    bool get_do_echo() const { return do_echo; }
    void set_do_echo(const bool _do_echo) { do_echo = _do_echo; }

    std::string get_term_type() { return term_type; }
    void set_term_type(const std::string _term_type) { term_type = _term_type; }

    unsigned short get_listen_port() const { return listen_port; }
    void set_listen_port(const unsigned short port) { listen_port = port; }

    bool get_auto_answer() const { return auto_answer; }
    void set_auto_answer(const bool answer) { auto_answer = answer; }

    bool get_answer_hack() const { return answer_hack; }
    void set_answer_hack(const bool hack) { answer_hack = hack; }

    bool get_cmd_mode() const { return cmd_mode; }
    void set_cmd_mode(const bool mode) { cmd_mode = mode; }

    bool get_output_cmd() const { return output_cmd; }
    void set_output_cmd(const bool output) { output_cmd = output; }

    bool get_modem_active() const { return modem_active; }
    void set_modem_active(const bool active) { modem_active = active; }

    bool get_answered() const { return answered; }
    void set_answered(const bool a) { answered = a; }

    long get_answer_timer() const { return answer_timer; }
    void set_answer_timer(const long timer) { answer_timer = timer; }

    void set_cmd(const char *buf, size_t length) { cmd.assign(buf, length); }
    const std::string& get_cmd() const { return cmd; }

    // "modem activity" side effect functions that implementations can override if they want to (e.g.) blink lights
    virtual void ma_start() {};
    virtual void ma_stop() {};
    virtual void ma_blink(int count = 1) {};

private:
    /* Modem Active Variables */
    std::string cmd = "";          // Gather a new AT command to this string from serial

    std::unique_ptr<fnTcpClient> tcp_client;
    std::unique_ptr<fnTcpServer> tcp_server;
    std::unique_ptr<ModemSniffer> modem_sniffer;
    std::unique_ptr<uart> uart_instance = nullptr;
    std::unique_ptr<TelnetEventHandler> telnet_event_handler;
    std::unique_ptr<telnet_t, TelnetDeleter> telnet;

    unsigned short listen_port = 0; // Listen to this if not connected. Set to zero to disable.
    unsigned int modem_baud = 300;
    unsigned int stop_bits = 1;
    unsigned int word_size = 8;
    bool has_parity = false;
    bool DTR = false;
    bool RTS = false;
    bool XMT = false;
    bool CRX = false;

    bool cmd_mode = true;           // Are we in AT command mode or connected mode
    bool answer_hack = false;
    bool command_echo = true;
    bool auto_answer = false;
    bool modem_active = false;
    bool output_cmd = true;
    bool was_last_cmd_atascii = false;

    bool use_telnet = false;
    bool do_echo = false;

    bool use_numeric_result_code = false;
    long answer_timer = 0;
    bool answered = false;

    std::string term_type;
    std::unordered_map<std::string, CommandHandler> command_handlers;

    void handle_at();
    void handle_net(bool use_telnet);
    void handle_answer();
    void handle_set_ip();
    void handle_help();
    void handle_hangup();
    void handle_dial();
    void handle_wifi_list();
    void handle_wifi_connect();
    void handle_get();
    void handle_port();
    void handle_v0();
    void handle_v1();
    void handle_e(bool command_echo);
    void handle_s0(bool auto_answer);
    void handle_sniff(bool enable);
    void handle_term(std::string type);
    void handle_cpm();
    void handle_pb_list();
    void handle_pb_clear();
    void handle_pb();
    void handle_o();

    void handle_ignored();

    // low level implementation specific printing functions
    virtual void at_connect_resultCode() = 0;
    virtual void at_cmd_resultCode(int result_code) = 0;
    virtual void at_cmd_println() = 0;
    virtual void at_cmd_println(const char *s, bool addEol = true) = 0;
    virtual void at_cmd_println(int i, bool addEol = true) = 0;
    virtual void at_cmd_println(std::string s, bool addEol = true) = 0;

    enum ResultCode {
        RESULT_CODE_OK             = 0,
        RESULT_CODE_CONNECT        = 1,
        RESULT_CODE_RING           = 2,
        RESULT_CODE_NO_CARRIER     = 3,
        RESULT_CODE_ERROR          = 4,
        RESULT_CODE_CONNECT_1200   = 5,
        RESULT_CODE_BUSY           = 7,
        RESULT_CODE_NO_ANSWER      = 8,
        RESULT_CODE_CONNECT_2400   = 10,
        RESULT_CODE_CONNECT_9600   = 13,
        RESULT_CODE_CONNECT_4800   = 18,
        RESULT_CODE_CONNECT_19200  = 85
    };

    std::map<ResultCode, std::string> result_code_messages = {
        {RESULT_CODE_OK,            "OK"             },
        {RESULT_CODE_CONNECT,       "CONNECT"        },
        {RESULT_CODE_RING,          "RING"           },
        {RESULT_CODE_NO_CARRIER,    "NO CARRIER"     },
        {RESULT_CODE_ERROR,         "ERROR"          },
        {RESULT_CODE_CONNECT_1200,  "CONNECT 1200"   },
        {RESULT_CODE_BUSY,          "BUSY"           },
        {RESULT_CODE_NO_ANSWER,     "NO ANSWER"      },
        {RESULT_CODE_CONNECT_2400,  "CONNECT 2400"   },
        {RESULT_CODE_CONNECT_9600,  "CONNECT 9600"   },
        {RESULT_CODE_CONNECT_4800,  "CONNECT 4800"   },
        {RESULT_CODE_CONNECT_19200, "CONNECT 19200"  }
    };

    enum HelpMessageKey {
        HELPPORT1, HELPPORT2, HELPPORT3, HELPPORT4,
        HELPSCAN1, HELPSCAN2, HELPSCAN3, HELPSCAN4, HELPSCAN5,
        HELPNOWIFI, HELPWIFICONNECTING
    };

    std::map<HelpMessageKey, std::string> help_messages = {
        {HELPPORT1,          "Listening to connections on port "    },
        {HELPPORT2,          "which result in RING that you can"    },
        {HELPPORT3,          "answer with ATA."                     },
        {HELPPORT4,          "No incoming connections are enabled." },
        {HELPSCAN1,          "Scanning..."                          },
        {HELPSCAN2,          "No networks found"                    },
        {HELPSCAN3,          " networks found:"                     },
        {HELPSCAN4,          " (open)"                              },
        {HELPSCAN5,          " (encrypted)"                         },
        {HELPNOWIFI,         "WiFi is not connected."               },
        {HELPWIFICONNECTING, "Connecting to "                       },
    };

    void send_help_message(HelpMessageKey key, bool addEol = true);
    void send_response(ResultCode code);

    static void telnet_event_handler_fn(telnet_t *telnet, telnet_event_t *ev, void *user_data);

    void handle_add_entry(const std::string& phnumber, const std::string& rest);
    void handle_delete_entry(const std::string& phnumber);

    void just_eol();

    // reduce the boiler plate behind all the at_cmd_println functions.
    template<typename T>
    void at_cmd_println_common(T value, bool add_eol, bool is_no_value = false) {
        if (!output_cmd) return;

        ma_start();

        auto print_and_eol = [this, add_eol](auto val, bool no_val) {
            if (!no_val) {
                get_uart()->print(val);
            }
            if (add_eol) just_eol();
        };

        print_and_eol(value, is_no_value);

        get_uart()->flush();
        ma_stop();
    }

protected:
    uint8_t tx_buf[TX_BUF_SIZE];

};

#endif // USE_NEW_MODEM_IF
#endif // MODEM_IF_H