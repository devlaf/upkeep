#include <iostream>
#include <stdio.h>
#include <time.h>
#include "uv.h"
#include "logger.h"
#include "database.h"

using namespace std;

int main()
{
    init_database();

    uptime_entry_t* entry = (uptime_entry_t*)calloc(1, sizeof(uptime_entry_t));
    entry->mac_address = "a";
    entry->description = "b";
    entry->uptime = 0;
    entry->last_update = time(0);

    insert_uptime_entry(entry);

    auto entries = get_uptime_record();

    for(uptime_record::iterator i = entries->begin();  i != entries->end(); i++) {
    	uptime_entry_t* entry_details = *i;

    	printf("mac: %s \n", entry_details->mac_address);
    	printf("description: %s \n", entry_details->description);
    	printf("uptime: %d \n", entry_details->uptime);
    	printf("last_report: %d \n", entry_details->last_update);
    }
}
