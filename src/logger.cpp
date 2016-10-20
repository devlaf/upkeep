#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <list>
#include <mutex>
#include "uv.h"
#include "zlog.h"
#include "logger.h"

using namespace std;

std::mutex mtx;

struct log_data_t {
    log_type type;
    char* msg;
};

typedef std::list<log_data_t*> log_data_list;
static log_data_list* queued_logs = NULL;
static bool flush_ongoing = false;

void set_zlog_error_file()
{
    if(NULL == getenv("ZLOG_PROFILE_ERROR"))
       setenv("ZLOG_PROFILE_ERROR", zlog_error_filepath, 0);
}

int get_total_length_with_args(const char* msg, va_list args)
{
    va_list argclone;
    va_copy(argclone, args);
    int len = vsnprintf(0, 0, msg, argclone);
    va_end(argclone);
    return (len + 1);
}

char* generate_str_from_args(const char* msg, va_list args)
{
    int len = get_total_length_with_args(msg, args);
    char* full_message = (char*)malloc(sizeof(char)*(len + 1));
    vsnprintf(full_message, len, msg, args);
    full_message[len] = '\0';

    return full_message;
}

void log_synchronous (log_data_list* logs)
{
    set_zlog_error_file();

    int rc = zlog_init(zlog_config_filepath);
    if (rc) {
        printf("zlog init (synchronous) failed.  See zlog error file for details at [%s].\n", zlog_error_filepath);
        return;
    }

    zlog_category_t* c = zlog_get_category(zlog_category);
    if (!c) {
        printf("Could not find configuration for zlog category [%s] in %s.\n", zlog_category, zlog_config_filepath);
        zlog_fini();
        return;
    }

    for(log_data_list::iterator i = logs->begin();  i != logs->end(); i++) {

        log_data_t* log_details = *i;
        
        switch(log_details->type) {
            case FATAL:
                zlog_fatal(c, log_details->msg);
                printf("FATAL: %s\n", log_details->msg);
                break;
            case ERROR:
                zlog_error(c, log_details->msg);
                printf("ERROR: %s\n", log_details->msg);
                break;
            case WARN:
                zlog_warn(c, log_details->msg);
                printf("WARNING: %s\n", log_details->msg);
                break;
            case INFO:
                zlog_info(c, log_details->msg);
                printf("INFO: %s\n", log_details->msg);
                break;
            case DEBUG:
            default:
                zlog_notice(c, log_details->msg);
                printf("DEBUG: %s\n", log_details->msg);
                break;
        }
    }

    zlog_fini();
}

void flush_log_data (log_data_list* data)
{
    log_synchronous(data);

    for(log_data_list::iterator i = data->begin();  i != data->end(); i++) {

        log_data_t* log_details = *i;
        free(log_details->msg);
        free(log_details);
    }

    delete data;
}

static void on_flushing_thread (uv_work_t* req)
{
    flush_log_data((log_data_list*)req->data);
}

static void on_flushing_thread_done (uv_work_t* req, int status)
{
    if (0 != status)
        log_synchronous(ERROR, "Failed to flush log to file from thread.  Error Code: %d", status);

    flush_ongoing = false;   
}

void schedule_flush()
{
    flush_ongoing = true;

    mtx.lock();
    static uv_work_t req;
    req.data = queued_logs;
    queued_logs = NULL;
    mtx.unlock();

    uv_queue_work(uv_default_loop(), &req, on_flushing_thread, on_flushing_thread_done);
}

static log_data_list* get_queued_logs()
{
    if (NULL == queued_logs)
        queued_logs = new log_data_list();
    return queued_logs;
}

void queue_log_generic(log_data_list* log_list, log_type type, const char* msg, va_list args)
{
    if (NULL == msg || NULL == log_list)
        return;

    char* full_message = generate_str_from_args(msg, args);
    
    log_data_t* data = (log_data_t*)malloc(sizeof(log_data_t));
    data ->type = type;
    data ->msg = full_message;

    log_list->push_back(data);
}

void queue_log(log_type type, const char* msg, va_list args)
{
    mtx.lock();
    queue_log_generic(get_queued_logs(), type, msg, args);
    mtx.unlock();

    if (!flush_ongoing)
        schedule_flush();
}

void log_info (const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    queue_log(INFO, msg, args);
    va_end(args);
}

void log_warn (const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    queue_log(WARN, msg, args);
    va_end(args);
}

void log_error (const char* msg, ...)
{
    va_list args;
    va_start(args, msg);
    queue_log(ERROR, msg, args);
    va_end(args);
}

void log_synchronous (log_type type, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);

    log_data_t* data = (log_data_t*)malloc(sizeof(log_data_t));
    data ->type = type;
    data ->msg = generate_str_from_args(msg, args);

    auto log_list = new log_data_list();
    log_list->push_back(data);

    log_synchronous(log_list);

    free(data->msg);
    free(data);
    delete log_list;

    va_end(args);
}