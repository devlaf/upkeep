#include <iostream>
#include <stdio.h>
#include "time.h"

time_t get_current_time()
{
    time_t rawtime;
    time (&rawtime);
    return rawtime;   
}

char* print_time_local(time_t rawtime)
{
    char* retval = (char*)malloc(sizeof(char) * 20);
    struct tm* timeinfo = localtime(&rawtime);
    strftime(retval, 20, "%Y-%m-%d %H:%M:%S", timeinfo);
    return retval;
}