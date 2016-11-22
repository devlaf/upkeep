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

enum protocols 
{
    PROTOCOL_HTTP = 0,
    PROTOCOL_WS_EVENT,
    PROTOCOL_COUNT
};

static bool is_whitelisted(const char* resource_path)
{
    return true;
}

static char* get_mime_type(const char* resource_path)
{
    char *extension = strrchr(resource_path, '.');
                   
    if (extension == NULL)
        return "text/plain";
    if (strcmp(extension, ".png") == 0)
        return "image/png";
    if (strcmp(extension, ".jpg") == 0)
        return "image/jpg";
    if (strcmp(extension, ".gif") == 0)
        return "image/gif";
    if (strcmp(extension, ".html") == 0)
        return "text/html";
    if (strcmp(extension, ".css") == 0)
        return "text/css";
    return "text/plain";
}

static bool serve_file(struct lws* wsi, const char* uri)
{
    // TO FIX: Whitelist not implemented, so requests for ../ etc. will allow access to other files on system.
    // Shouldn't need to alloc for each request

    char* resource_path = (char*)malloc(strlen(directory_of_executing_assembly) + 
        strlen(static_content_subdirectory) + strlen(uri));
    
    sprintf(resource_path, "%s%s%s", directory_of_executing_assembly, 
        static_content_subdirectory, uri);
    
    if (!is_whitelisted(resource_path)) {
        log_warn("Http client request for file at [%s] was rejected, as it is not part of the whitelist.");
        return false;
    }

    char* mime = get_mime_type(resource_path);
    lws_serve_http_file(wsi, resource_path, mime, NULL, 0);

    free(resource_path);
    resource_path = NULL;

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

static bool set_directory_for_executing_assembly()
{
    if (directory_of_executing_assembly != NULL)
        return true;

    directory_of_executing_assembly = (char*)calloc(1, 1024);

    if(getcwd(directory_of_executing_assembly, sizeof(directory_of_executing_assembly)) != NULL)
        return true;
    
    log_error("Failed to retrieve the directory of the executing assembly.");
    directory_of_executing_assembly = NULL;
    return false;
}

void init_webserver()
{
    if (!set_directory_for_executing_assembly())
        return;

    const char* interface = NULL;
    context = build_context(interface);
    if (context == NULL) {
        log_error("Failed to init the libwebsocket server.");
        return;
    }

    start_lws_service_timer();

    log_info("Web interface started.");
}

void shutdown_webserver()
{
    if (service_timer != NULL && uv_is_active((uv_handle_t*)service_timer))
        uv_timer_stop(service_timer);

    if(context)
        lws_context_destroy(context);

    free(directory_of_executing_assembly);
    directory_of_executing_assembly = NULL;

    log_info("Web interface stopped.");
}