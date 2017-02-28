#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "sqlite3.h"
#include "logger.h"
#include "database.h"

bool create_directory(const char* fullPath)
{
    const char* mkdir = "mkdir -p ";
    char* cmd = (char*)malloc(sizeof(char)*(strlen(fullPath) + strlen(mkdir) + 1));
    strcpy(cmd, mkdir);
    strcat(cmd, fullPath); 
    
    if (0 == system(cmd))
        return true;

    log_synchronous(ERROR, "Failed to create a directory at [%s]. \n", fullPath);
    return false;
}

void init_database()
{
    sqlite3* db;
    static const char* create_table_script = "CREATE TABLE IF NOT EXISTS uptime ("
            "mac_address TEXT PRIMARY KEY ON CONFLICT REPLACE, "
            "description TEXT, uptime TEXT, last_update INTEGER)";

    if (!create_directory(SQLite_db_directory))
        _exit(SIGTERM);

    int open = sqlite3_open(SQLite_db_filepath, &db);
    if (open != SQLITE_OK) {
        log_synchronous(ERROR, "init_database: Failed to open the database at [%s]. "
            "SQLite Error: %d", SQLite_db_filepath, open);
        _exit(SIGTERM);
    }

    int create_table = sqlite3_exec(db, create_table_script, NULL, NULL, NULL);
    if (create_table != SQLITE_OK) {
        log_synchronous(ERROR, "init_database: Failed to setup tables in the database. "
            "SQLite Error: %d", create_table);
        _exit(SIGTERM);
    }

    sqlite3_close(db);

    log_info("SQLite database initialized");
}

void insert_uptime_entry(uptime_entry_t* entry)
{
    sqlite3* db = NULL;
    sqlite3_stmt* stmt = NULL;
    
    int open = sqlite3_open(SQLite_db_filepath, &db);
    if (open != SQLITE_OK) {
        log_synchronous(ERROR, "insert_uptime_entry: Failed to open the database at [%s]. "
            "SQLite Error: %d", SQLite_db_filepath, open);
        _exit(SIGTERM);
    }

    sqlite3_busy_timeout(db, 100);  // Wait 100 ms for the lock

    int begin_transaction = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
    if(begin_transaction != SQLITE_OK) {
        log_synchronous(ERROR, "insert_uptime_entry: Failed to begin transaction."
            " SQLite Error: %d", begin_transaction);
        _exit(SIGTERM);
    }

    static const char* query = "INSERT INTO uptime VALUES (@mac_address, @description, @uptime, @last_update)";
    
    int insert = sqlite3_prepare_v2(db, query, 256, &stmt, NULL);
    if(insert != SQLITE_OK) {
        log_synchronous(ERROR, "insert_uptime_entry: Failed to insert into db. "
            "SQLite Error: %d", insert);
        _exit(SIGTERM);
    }

    if (NULL != entry) {
        sqlite3_bind_text (stmt, 1, entry->mac_address, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text (stmt, 2, entry->description, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int  (stmt, 3, entry->uptime);
        sqlite3_bind_int64(stmt, 4, entry->last_update);

        sqlite3_step(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_reset(stmt);
    }

    int end_transaction = sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    if(end_transaction != SQLITE_OK) {
        log_synchronous(ERROR, "insert_uptime_entry: Failed to end transaction."
            " SQLite Error: %d", end_transaction);
        _exit(SIGTERM);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

uptime_record* get_uptime_record_with_modifiers(const char* sql_query_modifiers)
{
    sqlite3* db;
    sqlite3_stmt* stmt;

    int open = sqlite3_open(SQLite_db_filepath, &db);
    if (open != SQLITE_OK) {
        log_synchronous(ERROR, "get_uptime_record: Failed to open the database at [%s]."
            "  SQLite Error: %d", SQLite_db_filepath, open);
        return NULL;
    }

    sqlite3_busy_timeout(db, 100);  // Wait 100 ms for the lock

    int begin_transaction = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
    if(begin_transaction != SQLITE_OK) {
        log_synchronous(ERROR, "get_uptime_record: Failed to begin transaction. "
            "SQLite Error: %d", begin_transaction);
        return NULL;
    }

    const char* sql_query_base = "select * from uptime ";

    if (NULL == sql_query_modifiers)
        sql_query_modifiers = "\0";
    char* sql_query = (char*)malloc(sizeof(char)*(strlen(sql_query_modifiers)) + 22);
    sprintf(sql_query, "%s %s", sql_query_base, sql_query_modifiers);

    int record_query = sqlite3_prepare_v2(db, sql_query, 256, &stmt, NULL);
    free(sql_query);
    if (record_query != SQLITE_OK) {
        log_synchronous(ERROR, "get_uptime_record: Failed to execute the select query [%s]."
            "  SQLite Error: %d", sql_query, record_query);
        return NULL;
    }

    uptime_record* retval = list_init();

    while(sqlite3_step(stmt) == SQLITE_ROW) {
        uptime_entry_t* record = (uptime_entry_t*)calloc(1, sizeof(uptime_entry_t));

        char* mac_address   = (char*)sqlite3_column_text(stmt, 0);
        char* description   = (char*)sqlite3_column_text(stmt, 1);
        uint32_t uptime     = sqlite3_column_int(stmt, 2);                    // unchecked
        time_t last_update  = (time_t)sqlite3_column_int64(stmt, 3);

        record->mac_address = strdup(mac_address);
        record->description = strdup(description);    
        record->uptime = uptime;
        record->last_update = last_update;

        list_append(retval, record);
    }

    int end_transaction = sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
    if(end_transaction != SQLITE_OK) {
        log_synchronous(ERROR, "get_uptime_record: Failed to end transaction. "
            "SQLite Error: %d", end_transaction);
        return NULL;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return retval;
}

uptime_record* get_uptime_record()
{
    return get_uptime_record_with_modifiers(NULL);
}

uint32_t get_last_known_uptime(const char* mac_address)
{
    if (NULL == mac_address)
        return 0;

    const char* where_clause_base = "WHERE mac_address='%s'";
    char* where_clause = (char*)malloc(sizeof(char)*(strlen(mac_address)) + 21);
    sprintf(where_clause, where_clause_base, mac_address);

    uptime_record* record = get_uptime_record_with_modifiers(where_clause);

    if (NULL == record->head)
    	return 0;

    uint32_t retval = ((uptime_entry_t*)(record->head->data))->uptime;
    free_uptime_record(record);
    return retval;
}

void uptime_record_foreach(uptime_record* collection, void (*fptr)(uptime_entry_t*, void*), void* args)
{
    list_foreach(collection, (void(*)(void*, void*))&fptr, args);
}

void free_uptime_entry_t(uptime_entry_t* entry)
{
    if(NULL == entry)
        return;

    free(entry->mac_address);
    free(entry->description);
    free(entry);
}

void free_uptime_entry_t_with_args(uptime_entry_t* entry, void* args)
{
    free_uptime_entry_t(entry);
}

void free_uptime_record(uptime_record* records)
{
    if(NULL == records)
        return;

    uptime_record_foreach(records, free_uptime_entry_t_with_args, NULL);
    
    list_free(records);
}
