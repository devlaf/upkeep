#include <iostream>
#include <stdio.h>
#include "uv.h"
#include "logger.h"
#include "database.h"
#include "serialization.h"
#include "time_utils.h"
#include "web_interface.h"

#define VERSION    0.1

using namespace std;

static char* listen_ip_addr = "0.0.0.0";
static int   listen_port    = 12001;

static uv_timer_t* outage_timer;
static int outage_timer_interval_ms = 60000;   
static int outage_timer_threshold_sec = 120; // Expect devices to update at 
                                             // least once every 120 seconds.

void shutdown_upkeep(int return_code)
{
    log_info("Upkeep terminating.");
    force_log_flush();

    if(outage_timer) {
        if (uv_is_active((uv_handle_t*)outage_timer))
            uv_timer_stop(outage_timer);
        free(outage_timer);
    }

    shutdown_webserver();

    exit(0);
}

void register_interrupt_handlers()
{
    signal(SIGINT, shutdown_upkeep);
    signal(SIGTERM, shutdown_upkeep);
    signal(SIGHUP, shutdown_upkeep);
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) 
{
    *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}

void on_close (uv_handle_t* handle) 
{ 
    free(handle); 
}

void on_device_reboot(uv_work_t* req)
{
    uptime_entry_t* data = (uptime_entry_t*)req->data;
    printf("aaaahhhh");
}

void on_device_reboot_processed(uv_work_t* req, int status)
{
    free_uptime_entry_t((uptime_entry_t*)req->data);
}

void on_device_timeout(uptime_entry_t* record)
{
    printf("weeeewoooo");
}

void on_outage_timer(uv_timer_t* handle)
{
    uptime_record* record = get_uptime_record();

    for(uptime_record::iterator i = record->begin();  i != record->end(); i++) {

        uptime_entry_t* entry = *i;
        if ((get_current_time() - entry->last_update) > outage_timer_threshold_sec ) {
            on_device_timeout(entry);
        }
    }

    free_uptime_record(record);
}

void start_outage_timer()
{
    if (NULL == outage_timer)
        outage_timer = (uv_timer_t*)malloc(sizeof(uv_timer_t));

    if (uv_is_active((uv_handle_t*)outage_timer))
        return;

    uv_timer_init(uv_default_loop(), outage_timer);
    uv_timer_start(outage_timer, on_outage_timer, outage_timer_interval_ms, outage_timer_interval_ms);
}

uptime_entry_t* store_uptime_report_in_db(uptime_report_t* report)
{
    uptime_entry_t* entry = (uptime_entry_t*)malloc(sizeof(uptime_entry_t));

    entry->mac_address = strdup(report->mac_address);
    entry->description = strdup(report->description);
    entry->uptime = report->uptime;
    entry->last_update = get_current_time();
    
    insert_uptime_entry(entry);
}

void register_uptime_report (uptime_report_t* report)
{
    time_t current_time = get_current_time();
    
    char* current_time_str = print_time_local(current_time);
    log_info("Report recieved from [%s] at time %s", report->description, current_time_str);
    free(current_time_str);

    uint32_t last_recorded_uptime = get_last_known_uptime(report->mac_address);

    uptime_entry_t* entry = store_uptime_report_in_db(report);

    if(last_recorded_uptime > entry->uptime || entry->uptime < 5000) {
        log_info("Detected reboot for device [%s].  Old uptime: %d.  New uptime: %d", 
            entry->description, last_recorded_uptime, entry->uptime);
        
        uv_work_t* req = (uv_work_t*)malloc(sizeof(uv_work_t));
        req->data = entry;
        uv_queue_work(uv_default_loop(), req, on_device_reboot, on_device_reboot_processed);
    } else {
        free_uptime_entry_t(entry);
    }
}

void on_read_unit_complete(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf)
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
    if (uptime_data != NULL) {
        register_uptime_report(uptime_data);
        free_uptime_report_t(uptime_data);
    }
        
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
    
    if (uv_accept(server, (uv_stream_t*) client) == 0) 
        uv_read_start((uv_stream_t*) client, alloc_buffer, (uv_read_cb)server->data);
    else
        uv_close((uv_handle_t*) client, NULL);

    start_outage_timer();
}

void listen_for_connections()
{
    static uv_tcp_t server;
    uv_tcp_init(uv_default_loop(), &server);

    struct sockaddr_in addr;
    uv_ip4_addr(listen_ip_addr, listen_port, &addr);
    uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);

    server.data = (void*)on_read_unit_complete;

    int listen_resp = uv_listen((uv_stream_t*) &server, 500, on_new_connection);
    if (listen_resp != 0) {
        log_error("listen_for_connections -- listen error: %s, %s.",
            uv_err_name(listen_resp),  uv_strerror(listen_resp));
        shutdown_upkeep(-1);
    }
}

int main (int argc, char** argv)
{
    if( argc == 2 && (strcmp(argv[1], "-v") == 0) ) {
        printf("Version: [%f].  And it's funny that you think this is versioned in any meaningful way.\n", VERSION);
        return 0;
    }

    log_info("upkeep version: %f", VERSION);

    register_interrupt_handlers();

    init_database();

    listen_for_connections();
    
    init_webserver();

    return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
