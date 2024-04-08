#ifdef USE_NEW_MODEM_IF

#include "telnetEventHandler.h"
#include "modem_if.h"

void DefaultTelnetEventHandler::handle_event(telnet_t *telnet, telnet_event_t *ev, void *user_data) {
    modem_if *modem_ = (modem_if *)user_data;

    switch (ev->type)
    {
    case TELNET_EV_DATA:
        if (ev->data.size && modem_->get_uart()->write((uint8_t *)ev->data.buffer, ev->data.size) != ev->data.size)
            Debug_printf("handleEvent(%d) - Could not write complete buffer to BUS.\n", ev->type);
        break;
    case TELNET_EV_SEND:
        modem_->get_tcp_client()->write((uint8_t *)ev->data.buffer, ev->data.size);
        break;
    case TELNET_EV_WILL:
        if (ev->neg.telopt == TELNET_TELOPT_ECHO)
            modem_->set_do_echo(false);
        break;
    case TELNET_EV_WONT:
        if (ev->neg.telopt == TELNET_TELOPT_ECHO)
            modem_->set_do_echo(true);
        break;
    case TELNET_EV_DO:
        break;
    case TELNET_EV_DONT:
        break;
    case TELNET_EV_TTYPE:
        if (ev->ttype.cmd == TELNET_TTYPE_SEND)
            telnet_ttype_is(telnet, modem_->get_term_type().c_str());
        break;
    case TELNET_EV_SUBNEGOTIATION:
        break;
    case TELNET_EV_ERROR:
        Debug_printf("handleEvent ERROR: %s\n", ev->error.msg);
        break;
    default:
        Debug_printf("handleEvent: Uncaught event type: %d", ev->type);
        break;
    }
}

#endif // USE_NEW_MODEM_IF