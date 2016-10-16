static char* zlog_category = "upkeep_log";
static char* zlog_config_filepath = "./zlog.conf";
static char* zlog_error_filepath = "/tmp/zlog_error.txt";

typedef enum {DEBUG, INFO, WARN, ERROR, FATAL} log_type;

void log_synchronous (log_type type, const char* msg, ...);
void log_info (const char* msg, ...);
void log_warn (const char* msg, ...);
void log_error (const char* msg, ...);
