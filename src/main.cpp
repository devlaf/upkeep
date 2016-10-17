#include <iostream>
#include <stdio.h>
#include <time.h>
#include "uv.h"
#include "logger.h"
#include "database.h"

#define VERSION	0.1

using namespace std;

static char* listen_ip_addr = "0.0.0.0";
static int   listen_port    = 19001;

void shutdown_upkeep()
{
	log_info("Upkeep terminating.");
	exit(0);
}



void on_new_connection(uv_stream_t *server, int status) {
	if (status < 0) {
        log_error("on_new_connection -- New connection error %s\n", uv_strerror(status));
        return;
    }

    uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
    uv_tcp_init(uv_default_loop(), client);
    if (uv_accept(server, (uv_stream_t*) client) == 0) {
        uv_read_start((uv_stream_t*) client, on_buf_alloc, (uv_read_cb)server->data);
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
