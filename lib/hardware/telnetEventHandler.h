#ifndef TELNET_EVENT_HANDLER_H
#define TELNET_EVENT_HANDLER_H

#ifdef USE_NEW_MODEM_IF

#include <stdbool.h>
#include "libtelnet.h"
#include "../../include/debug.h"

class TelnetEventHandler {
public:
    virtual ~TelnetEventHandler() = default;
    virtual void handle_event(telnet_t *telnet, telnet_event_t *ev, void *user_data) = 0;
};

class DefaultTelnetEventHandler : public TelnetEventHandler {
public:
    void handle_event(telnet_t *telnet, telnet_event_t *ev, void *user_data) override;
};


// Example of cusomizing the handling for particular events (e.g. drivewire does nothing on TELNET_EV_DATA)
class EVDataTelnetEventHandler : public TelnetEventHandler {
private:
    DefaultTelnetEventHandler default_handler;
public:
    void handle_event(telnet_t *telnet, telnet_event_t *ev, void *user_data) override {
        if (ev->type == TELNET_EV_DATA) {
            // perform custom handling of TELNET_EV_DATA event... In drivewire case, do nothing!
        } else {
            default_handler.handle_event(telnet, ev, user_data);
        }
    }
};

#endif // USE_NEW_MODEM_IF
#endif // TELNET_EVENT_HANDLER_H