static char* zlog_category = "upkeep_log";
static char* zlog_config_filepath = "./zlog.conf";
static char* zlog_error_filepath = "/tmp/zlog_error.txt";

void log_synchronous (const char* msg, ...);
