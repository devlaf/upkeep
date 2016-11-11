#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include "uv.h"
#include "zlog.h"
#include "logger.h"
#include "list.h"

typedef struct log_data_t {
    log_type type;
    char* msg;
} log_data_t;

static list* queued_logs;
static bool flush_ongoing = false;
static uv_rwlock_t queued_logs_lock;

static void set_zlog_error_file()
{
    if(NULL == getenv("ZLOG_PROFILE_ERROR"))
       setenv("ZLOG_PROFILE_ERROR", zlog_error_filepath, 0);
}

static int get_total_length_with_args(const char* msg, va_list args)
{
    va_list argclone;
    va_copy(argclone, args);
    int len = vsnprintf(0, 0, msg, argclone);
    va_end(argclone);
    return (len + 1);
}

static char* generate_str_from_args(const char* msg, va_list args)
{
    int len = get_total_length_with_args(msg, args);
    char* full_message = (char*)malloc(sizeof(char)*(len + 1));
    vsnprintf(full_message, len, msg, args);
    full_message[len] = '\0';

    return full_message;
}

static void log_single_entry_synchronous(void* single_entry, void* category)
{
    log_data_t* log_details = (log_data_t*)single_entry;
    zlog_category_t* c = (zlog_category_t*)category;

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

static void log_collection_synchronous (list* logs)
{
    set_zlog_error_file();
    
    int rc = zlog_init(zlog_config_filepath);
    if (rc) {
        printf("zlog init (synchronous) failed.  See zlog error file for details at [%s].\n", zlog_error_filepath);
        return;
    }

    zlog_category_t* category = zlog_get_category(zlog_category);
    if (!category) {
        printf("Could not find configuration for zlog category [%s] in %s.\n", zlog_category, zlog_config_filepath);
        zlog_fini();
        return;
    }

    list_foreach(logs, log_single_entry_synchronous, category);

    zlog_fini();
}

static void free_log_details(void* data, void* args)
{
    if (data) {
        log_data_t* log_details = (log_data_t*)data;
        free(log_details->msg);
        free(log_details);
    }
}

static void flush_log_data (list* data)
{
    log_collection_synchronous(data);

    list_foreach(data, free_log_details, NULL);
    list_free(data);
}

static void on_flushing_thread (uv_work_t* req)
{
    flush_log_data((list*)req->data);
}

static void on_flushing_thread_done (uv_work_t* req, int status)
{
    if (0 != status)
        log_synchronous(ERROR, "Failed to flush log to file from thread.  Error Code: %d", status);

    flush_ongoing = false;

    free(req);
}

static void schedule_flush()
{
    flush_ongoing = true;

    uv_work_t* req = (uv_work_t*)malloc(sizeof(uv_work_t));
    uv_rwlock_wrlock(&queued_logs_lock);
    req->data = queued_logs;
    queued_logs = NULL;

    uv_rwlock_wrunlock(&queued_logs_lock);

    uv_queue_work(uv_default_loop(), req, on_flushing_thread, on_flushing_thread_done);
}

static list* get_queued_logs()
{
    if (NULL == queued_logs)
        queued_logs = list_init();
    return queued_logs;
}

static void queue_log_generic(list* log_list, log_type type, const char* msg, va_list args)
{
    if (NULL == msg || NULL == log_list)
        return;

    char* full_message = generate_str_from_args(msg, args);
    
    log_data_t* data = (log_data_t*)malloc(sizeof(log_data_t));
    data ->type = type;
    data ->msg = full_message;

    list_append(log_list, data);
}

static void queue_log(log_type type, const char* msg, va_list args)
{
    uv_rwlock_wrlock(&queued_logs_lock);
    queue_log_generic(get_queued_logs(), type, msg, args);
    uv_rwlock_wrunlock(&queued_logs_lock);

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
    data->type = type;
    data->msg = generate_str_from_args(msg, args);

    list* log_list = list_init();
    list_append(log_list, data);

    log_collection_synchronous(log_list);

    free(data->msg);
    free(data);
    list_free(log_list);

    va_end(args);
}

void force_log_flush()
{
    uv_rwlock_wrlock(&queued_logs_lock);
    if (NULL != queued_logs)
        flush_log_data(queued_logs);
    uv_rwlock_wrunlock(&queued_logs_lock);
}