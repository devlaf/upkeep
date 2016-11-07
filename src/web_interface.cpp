#include <stdio.h>
#include "libwebsockets.h"
#include "uv.h"
#include "serialization.h"
#include "logger.h"
#include "web_interface.h"



// Adapted from example docs here: 
// https://github.com/warmcat/libwebsockets/blob/master/test-server/test-server.c
// There are quite a few oddities about required protocol names and orderings 
// (ex. PROTOCOL_HTTP must always be first, PROTOCOL_COUNT last, etc.)

struct lws_context* context;
static uv_timer_t* service_timer;
static int service_timer_interval_ms = 500;   

enum protocols 
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_WS_EVENT,
    PROTOCOL_COUNT
};

static int callback_http (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    return 0;
}

static int callback_ws_event (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        //sizeof (struct per_session_data__http),
        0,
    },
    {
        "ws-event",
        callback_ws_event,
        //sizeof(struct per_session_data__ws_event),
        0,
    },
    { NULL, NULL, 0, 0 }
};

static lws_context* build_context(const char* interface)
{
    lws_context_creation_info info;
    memset(&info, 0, sizeof info);

    info.port = websocket_port;
    info.iface = interface;
    info.protocols = protocols;
    info.extensions = lws_get_internal_extensions();
    info.ssl_cert_filepath = NULL;          // Forgo ssl for the time being
    info.ssl_private_key_filepath = NULL;   // ^^^
    info.gid = -1;
    info.uid = -1;
    info.options = 0;                       // No special options

    return lws_create_context(&info);
}

static void on_lws_service_timer(uv_timer_t* handle)
{
    lws_service(context, 0);
}

static void start_lws_service_timer()
{
    if (NULL == service_timer)
        service_timer = (uv_timer_t*)malloc(sizeof(uv_timer_t));

    if (uv_is_active((uv_handle_t*)service_timer))
        return;

    uv_timer_init(uv_default_loop(), service_timer);
    uv_timer_start(service_timer, on_lws_service_timer, service_timer_interval_ms, service_timer_interval_ms);

    log_info("Web interface started.");
}

void init_webserver()
{
    const char* interface = NULL;

    context = build_context(interface);
    if (context == NULL) {
        log_error("Failed to init the libwebsocket server.");
        return;
    }

    start_lws_service_timer();
}

void shutdown_webserver()
{
    if (service_timer != NULL && uv_is_active((uv_handle_t*)service_timer))
        uv_timer_stop(service_timer);

    if(context)
        lws_context_destroy(context);

    log_info("Web interface stopped.");
}