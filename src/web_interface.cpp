#include <stdio.h>
#include "libwebsockets.h"
#include "serialization.h"
#include "logger.h"
#include "web_interface.h"



// Adapted from example docs here: 
// https://github.com/warmcat/libwebsockets/blob/master/test-server/test-server.c
// There are quite a few oddities about required protocol names and orderings 
// (ex. PROTOCOL_HTTP must always be first, PROTOCOL_COUNT last, etc.)

enum protocols 
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_WS_EVENT,
    PROTOCOL_COUNT
};

static int callback_http (struct libwebsocket_context * context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{

}

static int callback_ws_event (struct libwebsocket_context * context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len)
{

}

static struct lws_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        sizeof (struct per_session_data__http),
        0,
    },
    {
        "ws-event",
        callback_ws_event,
        sizeof(struct per_session_data__ws_event),
        0,
    },
    { NULL, NULL, 0, 0 }
};

void init_webserver()
{
    cost char* interface = NULL;

    // Forgo ssl for the time being
    const char* cert_path = NULL;
    const char* key_path = NULL;

    int opts = 0;   // No special options

    auto context = libwebsocket_create_context( websocket_port, interface, protocols, 
                                                libwebsocket_internal_extensions, 
                                                cert_path, key_path, -1, -1, opts);    

    if (NULL == context) {
        log_error("Failed to init the libwebsocket server.");
        return;
    }

    // https://gist.github.com/martinsik/3654228
}