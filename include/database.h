#include <list>
#include <string.h>

static char* SQLite_db_directory = "/opt/upkeep/db/";    // yeah, whatever
static char* SQLite_db_filepath = "/opt/upkeep/db/upkeep.sqlite";

struct uptime_entry_t {
    char* mac_address;
    char* description;
    uint32_t uptime;
    time_t last_update;
};

typedef std::list<uptime_entry_t*> uptime_record;

void init_database();
uptime_record* get_uptime_record();
void insert_uptime_entry(uptime_entry_t* entry);