#include <string.h>
#include <stdint.h>
#include "list.h"

static char* SQLite_db_directory = "/opt/upkeep/db/";    // yeah, whatever
static char* SQLite_db_filepath = "/opt/upkeep/db/upkeep.sqlite";

typedef struct uptime_entry_t {
    char* mac_address;
    char* description;
    uint32_t uptime;
    time_t last_update;
}  uptime_entry_t;

typedef list uptime_record;
void uptime_record_foreach(uptime_record* collection, void (*fptr)(uptime_entry_t*, void*), void* args);

void init_database();
uptime_record* get_uptime_record();
uint32_t get_last_known_uptime(const char* mac_address);
void insert_uptime_entry(uptime_entry_t* entry);
void free_uptime_entry_t(uptime_entry_t* entry);
void free_uptime_record(uptime_record* records);