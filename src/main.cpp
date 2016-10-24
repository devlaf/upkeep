#include <iostream>
#include <stdio.h>
#include <time.h>
#include "uv.h"
#include "logger.h"
#include "database.h"
#include "serialization.h"

#define VERSION    0.1

using namespace std;

static char* listen_ip_addr = "0.0.0.0";
static int   listen_port    = 12001;

void shutdown_upkeep()
{
    log_info("Upkeep terminating.");
    exit(0);
}

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) 
{
    *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void on_close (uv_handle_t* handle) 
{ 
    free(handle); 
}

void register_uptime_report (uv_stream_t* client, uptime_report_t* report)
{
    printf("got a report from %s", report->description);
}

static void on_read_unit_complete(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf)
{
    uptime_report_t* uptime_data;

    if (0 == nread)
        goto cleanup;

    if (nread < 0) {
        uv_close((uv_handle_t*) client, on_close);
        auto err = uv_err_name(nread);    // (per the docs) leaks a few bytes of memory for unknown err code
        auto msg = uv_strerror(nread);    // ^^^
        log_error("Error reading message from new connection: LibUV err [%s], LibUv msg [{%s}].", err, msg);
        goto cleanup;
    } 

    uptime_data = deserialize_report(buf->base, nread);
    if (uptime_data != NULL)
        register_uptime_report(client, uptime_data);
        
    cleanup:
    if (buf->base)
        free(buf->base);
}

void on_new_connection(uv_stream_t *server, int status) 
{
    if (status < 0) {
        log_error("on_new_connection -- New connection error %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, alloc_buffer, (uv_read_cb)server->data);
    }
    else {
        uv_close((uv_handle_t*) client, NULL);
    }
}

void listen_for_connections()
{
    static uv_tcp_t server;
    uv_tcp_init(uv_default_loop(), &server);

    struct sockaddr_in addr;
    uv_ip4_addr(listen_ip_addr, listen_port, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    server.data = (void *)on_read_unit_complete;

    int listen_resp = uv_listen((uv_stream_t*) &server, 500, on_new_connection);
    if (listen_resp != 0) {
        log_error("listen_for_connections -- listen error: %s, %s.",
            uv_err_name(listen_resp),  uv_strerror(listen_resp));
        shutdown_upkeep();
    }
}

int main (int argc, char** argv)
{
    if( argc == 2 && (strcmp(argv[1], "-v") == 0) ) {
        printf("Version: [%s].  And it's funny that you think this is versioned in any meaningful way.", VERSION);
        return 0;
    }

    log_info("upkeep version: %f", VERSION);

    init_database();

    listen_for_connections();

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
