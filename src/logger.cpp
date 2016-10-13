#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "zlog.h"
#include "logger.h"

using namespace std;

int get_total_length_with_args(const char* msg, va_list args)
{
	va_list argclone;
	va_copy(argclone, args);
	int len = vsnprintf(0, 0, msg, argclone);
	va_end(argclone);
	return (len + 1);
}

void set_zlog_error_file()
{
    if(NULL == getenv("ZLOG_PROFILE_ERROR"))
	   setenv("ZLOG_PROFILE_ERROR", zlog_error_filepath, 0);
}

void log_synchronous (const char* msg, ...)
{
    va_list args;
	va_start(args, msg);

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

    int len = get_total_length_with_args(msg, args);
	char* full_message = (char*)malloc(sizeof(char)*(len + 1));
	vsnprintf(full_message, len, msg, args);
	full_message[len] = '\0';

    zlog_error(c, full_message);
    zlog_fini();
    printf("%s\n", full_message);

    free(full_message);
    va_end(args);
}
