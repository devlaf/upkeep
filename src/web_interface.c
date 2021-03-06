/*
*   A web interface that will push uptime reports to connected 
*   clients.  This both serves static HTTP content and provides
*   a websocket server to keep connected clients up-to-date.
*
*   Note:
*   Working with the libwebsockets libs can be somewhat
*   nonintuitive at times. There are a few oddities 
*   regarding required protocol names and orderings (ex. 
*   PROTOCOL_HTTP must always have value zero in required 
*   "protocols" enum, PROTOCOL_COUNT must be last, etc.)
*   Good resources for some of this stuff can be found in
*   the examples on the official github page: 
*   https://github.com/warmcat/libwebsockets/blob/master/test-server/test-server.c
*   The archived mailing list can also be helpful.
*/

#include <stdio.h>
#include <stdbool.h>
#include "libwebsockets.h"
#include "uv.h"
#include "serialization.h"
#include "database.h"
#include "logger.h"
#include "web_interface.h"

typedef struct resource {
    char* uri;
    char* resource_path;
    char* mime_type;
} resource;

typedef struct per_session_data__ws_event {
    int ring_buffer_pos;
} per_session_data__ws_event;

// This ringbuffer is used to keep track of data that should
// be broadcast to all connected websocket clients.
static uint8_t* ws_ring_buffer[10];
static int ws_ringbuffer_current = 0;

struct lws_context* context;
static uv_timer_t* service_timer;
static const int service_timer_interval_ms = 500;
static char* directory_of_executing_assembly = NULL;
static bool running = false;
static uv_rwlock_t running_lock;

enum protocols 
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_WS_EVENT,
    PROTOCOL_COUNT
};

resource whitelist[] = {
    {"/index.html", NULL, "text/html"},
    {"/icon.png", NULL, "image/png"}
};

static resource* search_whitelist(const char* uri)
{
    for (int i=0; i<(sizeof(whitelist) / sizeof(resource)); i++) {
        if(strcmp(uri, whitelist[i].uri) == 0)
            return &whitelist[i];
    }

    return NULL;
}

static bool serve_file(struct lws* wsi, const char* uri)
{
    resource* file_data = search_whitelist(uri);
    if (NULL == file_data) {
        log_warn("Http client request for file at [%s] was rejected, as it is not part of the whitelist.", file_data->resource_path);
        return false;
    }

    lws_serve_http_file(wsi, file_data->resource_path, file_data->mime_type, NULL, 0);

    return true;
}

static int callback_http (struct lws* wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    if (reason != LWS_CALLBACK_HTTP)
        return 0;

    char* requested_uri = (char*)in;

    log_info("Client requested URI: %s", requested_uri);

    if (strcmp(requested_uri, "/") == 0) 
        requested_uri = "/index.html";
            
    if (!serve_file(wsi, (const char*)requested_uri))
        return 1;
    
    return 0;
}

static unsigned char* generate_lws_padded_msg(uint8_t* msg, int len)
{
    unsigned char* buf = (unsigned char*)malloc(LWS_SEND_BUFFER_PRE_PADDING + 
        len + LWS_SEND_BUFFER_POST_PADDING);
    memcpy(buf + LWS_SEND_BUFFER_PRE_PADDING, msg, len);
    return buf + LWS_SEND_BUFFER_PRE_PADDING;
}

static void free_lws_padded_msg(unsigned char* msg)
{
    if (msg != NULL) {
        unsigned char* origin = msg - LWS_SEND_BUFFER_PRE_PADDING;
        free(origin);
        origin = NULL;
    }
}

static void send_record(uptime_entry_t* data, void* wsi)
{
    uint8_t* serialized = serialize_report(data);
    int len = sizeof(serialized)/sizeof(uint8_t);
    unsigned char* buf = generate_lws_padded_msg(serialized, len);
    int result = lws_write((struct lws*)wsi, buf, len, LWS_WRITE_TEXT);

    free_lws_padded_msg(buf);
    free(serialized);
}

static int callback_ws_event (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    per_session_data__ws_event* psd = (per_session_data__ws_event*)user;

    switch(reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            psd->ring_buffer_pos = ws_ringbuffer_current;

            // Send all existing data in DB
            uptime_record* to_send = get_uptime_record();
            uptime_record_foreach(to_send, send_record, (void*)wsi);
            free_uptime_record(to_send);
        }
        case LWS_CALLBACK_CLOSED: {
            log_info("Websocket connection closed by client.");
        }
        case LWS_CALLBACK_RECEIVE: {
            // As this websocket interface is supposed to be one-way, all
            // incoming wsi messages will be dropped on the floor.
            break;
        }
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            while(psd->ring_buffer_pos != ws_ringbuffer_current) {
                unsigned char* to_write = (unsigned char*) ws_ring_buffer[psd->ring_buffer_pos];
                lws_write(wsi, (sizeof(to_write) / sizeof(char*)), to_write, LWS_WRITE_TEXT);
                psd->ring_buffer_pos = (psd->ring_buffer_pos) % 10;
            }
            break;
        }
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "http-only",
        callback_http,
        0,
    },
    {
        "ws-event",
        callback_ws_event,
        sizeof(per_session_data__ws_event),
        0,
    },
    { NULL, NULL, 0, 0 }
};

static struct lws_context* build_context(const char* interface)
{
    struct lws_context_creation_info info;
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
    // A call to lws_service(...) will respond to any queued lws requests and kick off the callback_http and callback_ws events.
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
}

static void populate_whitelist() 
{
    for (int i = 0; i < (sizeof(whitelist)/sizeof(resource)); i++) {
        char* uri = whitelist[i].uri;
        char* resource_path = (char*)malloc(strlen(directory_of_executing_assembly) + strlen(static_content_subdirectory) + strlen(uri));
        sprintf(resource_path, "%s%s%s", directory_of_executing_assembly, static_content_subdirectory, uri);
        whitelist[i].resource_path = resource_path;
    }
}

static void cleanup_whitelist() 
{
    for (int i = 0; i < (sizeof(whitelist)/sizeof(resource)); i++) {
        if (whitelist[i].resource_path != NULL) {
            free(whitelist[i].resource_path);
            whitelist[i].resource_path = NULL;
        }
    }
}

static void cleanup_ringbuffer()
{
    for(int i=0; i<(sizeof(ws_ring_buffer)/sizeof(uint8_t*)); i++) {
        if(ws_ring_buffer[i] != NULL) {
            free_lws_padded_msg(ws_ring_buffer[i]);
            ws_ring_buffer[i] = NULL;
        }
    }
    ws_ringbuffer_current = 0;
}

static bool set_directory_of_executing_assembly()
{
    directory_of_executing_assembly = getcwd(NULL, 0);
    if (NULL == directory_of_executing_assembly) {
        log_error("Could not start webserver: Failed to retrieve the directory of the executing assembly.");
        return false;
    }
    return true;
}

static bool server_already_running()
{
    bool retval = false;

    uv_rwlock_wrlock(&running_lock);

    if (running) {
        log_info("Webserver already started.");
        retval = true;
    } 
    running = true;

    uv_rwlock_wrunlock(&running_lock);
    
    return retval;
}

void broadcast_report(uptime_report_t* data) 
{
    uint8_t* serialized = serialize_report(data);
    int len = sizeof(serialized)/sizeof(uint8_t);
    unsigned char* buf = generate_lws_padded_msg(serialized, len);

    int ringbuffer_next = (ws_ringbuffer_current + 1) % 10;

    if (ws_ring_buffer[ringbuffer_next] != NULL)
        free_lws_padded_msg(ws_ring_buffer[ringbuffer_next]);

    ws_ring_buffer[ringbuffer_next] = buf;
    ws_ringbuffer_current = ringbuffer_next;

    lws_callback_on_writable_all_protocol(context, protocols);
}

void init_webserver()
{
    if (server_already_running())
        return; 

    if (!set_directory_of_executing_assembly())
        return;

    const char* interface = NULL;
    context = build_context(interface);
    if (context == NULL) {
        log_error("Could not start webserver: Failed to init the libwebsocket server.");
        return;
    }

    populate_whitelist();

    start_lws_service_timer();

    log_info("Web interface started.");
}

void shutdown_webserver()
{
    if (service_timer != NULL && uv_is_active((uv_handle_t*)service_timer))
        uv_timer_stop(service_timer);

    if(context)
        lws_context_destroy(context);

    cleanup_whitelist();
    cleanup_ringbuffer();

    free(directory_of_executing_assembly);
    directory_of_executing_assembly = NULL;

    uv_rwlock_wrlock(&running_lock);
    running = false;
    uv_rwlock_wrunlock(&running_lock);

    log_info("Web interface stopped.");
}
