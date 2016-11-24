#include <stdio.h>
#include <stdbool.h>
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
static char* directory_of_executing_assembly = NULL;
static bool running = false;
static uv_rwlock_t running_flag_lock;

enum protocols 
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_WS_EVENT,
    PROTOCOL_COUNT
};

typedef struct resource {
    char* uri;
    char* resource_path;
    char* mime_type;
} resource;

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
    switch(reason) {
        case LWS_CALLBACK_HTTP: {
            char* requested_uri = (char*)in;

            log_info("Client requested URI: %s", requested_uri);

            if (strcmp(requested_uri, "/") == 0) 
                requested_uri = "/index.html";
            
            if (!serve_file(wsi, (const char*)requested_uri))
                return 1;
            
            break;
        }

        case LWS_CALLBACK_HTTP_BODY:
        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
        case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        case LWS_CALLBACK_LOCK_POLL:
        case LWS_CALLBACK_UNLOCK_POLL:
        case LWS_CALLBACK_ADD_POLL_FD:
        case LWS_CALLBACK_DEL_POLL_FD:
        case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
        case LWS_CALLBACK_GET_THREAD_ID:
        case LWS_CALLBACK_OPENSSL_PERFORM_CLIENT_CERT_VERIFICATION:
        case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
            break;
    }

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

    uv_rwlock_wrlock(&running_flag_lock);

    if (running) {
        log_info("Webserver already started.");
        retval = true;
    }
    running = true;

    uv_rwlock_wrunlock(&running_flag_lock);
    
    return retval;
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

    free(directory_of_executing_assembly);
    directory_of_executing_assembly = NULL;

    uv_rwlock_wrlock(&running_flag_lock);
    running = false;
    uv_rwlock_wrunlock(&running_flag_lock);

    log_info("Web interface stopped.");
}