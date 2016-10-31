#include <iostream>

struct uptime_report_t {
    char* mac_address;
    char* description;
    uint32_t uptime;
};

uptime_report_t* deserialize_report (const char* buffer, int len);
uint8_t* serialize_report (uptime_report_t* unit);
void free_uptime_report_t(uptime_report_t* report);