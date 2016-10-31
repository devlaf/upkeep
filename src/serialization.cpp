#include <iostream>
#include <stdio.h>
#include <string.h>
#include "logger.h"
#include "serialization.h"
#include "uptime_report_msg.pb-c.h"


uptime_report_t* deserialize_report (const char* buffer, int len)
{    
    UptimeReportMsg* msg = uptime_report_msg__unpack(NULL, len, (uint8_t*)buffer);

    if (NULL == msg) {
        log_error("Failed to deserialize an incoming packet into an uptime report.");
        return NULL;
    }

    uptime_report_t* retval = (uptime_report_t*) malloc(sizeof(uptime_report_t));
    retval->mac_address = strdup(msg->mac_address);
    retval->description = strdup(msg->description);
    retval->uptime = msg->uptime;

    uptime_report_msg__free_unpacked(msg, NULL);
    return retval;
}

uint8_t* serialize_report (uptime_report_t* unit)
{
    UptimeReportMsg msg = UPTIME_REPORT_MSG__INIT;
    msg.mac_address = strdup(unit->mac_address);
    msg.description = strdup(unit->description);
    msg.uptime = unit->uptime;

    unsigned len = uptime_report_msg__get_packed_size(&msg);
    uint8_t* buf = (uint8_t*)malloc(len);
    uptime_report_msg__pack(&msg, buf);

    return buf;
}

void free_uptime_report_t(uptime_report_t* report)
{
    if (NULL == report)
        return;
    
    free(report->mac_address);
    free(report->description);
    free(report);
}