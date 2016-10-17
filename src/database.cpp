#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "sqlite3.h"
#include "logger.h"
#include "database.h"

bool create_directory(const char* fullPath)
{
	char cmd [256];
	strcpy(cmd,"mkdir -p ");
	strcat(cmd,fullPath); 
	
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

	auto open = sqlite3_open(SQLite_db_filepath, &db);
	if (open != SQLITE_OK) {
		log_synchronous(ERROR, "init_database: Failed to open the database at [%s]. "
			"SQLite Error: %d", SQLite_db_filepath, open);
		_exit(SIGTERM);
	}

	auto create_table = sqlite3_exec(db, create_table_script, NULL, NULL, NULL);
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
	
	auto open = sqlite3_open(SQLite_db_filepath, &db);
	if (open != SQLITE_OK) {
		log_synchronous(ERROR, "insert_uptime_entry: Failed to open the database at [%s]. "
			"SQLite Error: %d", SQLite_db_filepath, open);
		_exit(SIGTERM);
	}

	sqlite3_busy_timeout(db, 1000*60);  // Wait 60 seconds for the lock

	auto begin_transaction = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if(begin_transaction != SQLITE_OK) {
		log_synchronous(ERROR, "insert_uptime_entry: Failed to begin transaction."
			" SQLite Error: %d", begin_transaction);
		_exit(SIGTERM);
	}

	static const char* query = "INSERT INTO uptime VALUES (@mac_address, @description, @uptime, @last_update)";
	
	auto insert = sqlite3_prepare_v2(db, query, 256, &stmt, NULL);
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

	auto end_transaction = sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
	if(end_transaction != SQLITE_OK) {
		log_synchronous(ERROR, "insert_uptime_entry: Failed to end transaction."
			" SQLite Error: %d", end_transaction);
		_exit(SIGTERM);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
}

uptime_record* get_uptime_record()
{
	sqlite3* db;
	sqlite3_stmt* stmt;

	auto open = sqlite3_open(SQLite_db_filepath, &db);
	if (open != SQLITE_OK) {
		log_synchronous(ERROR, "get_uptime_record: Failed to open the database at [%s]."
			"  SQLite Error: %d", SQLite_db_filepath, open);
		return NULL;
	}

	sqlite3_busy_timeout(db, 1000*60);  // Wait 60 seconds for the lock

	auto begin_transaction = sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if(begin_transaction != SQLITE_OK) {
		log_synchronous(ERROR, "get_uptime_record: Failed to begin transaction. "
			"SQLite Error: %d", begin_transaction);
		return NULL;
	}

	const char* sql_query = "select * from uptime";
	auto record_query = sqlite3_prepare_v2(db, sql_query, 256, &stmt, NULL);
	if (record_query != SQLITE_OK) {
		log_synchronous(ERROR, "get_uptime_record: Failed to execute the select query [%s]."
			"  SQLite Error: %d", sql_query, record_query);
		return NULL;
	}

	auto retval = new uptime_record();

	while(sqlite3_step(stmt) == SQLITE_ROW) {
		uptime_entry_t* record = (uptime_entry_t*)calloc(1, sizeof(uptime_entry_t));

		char* mac_address   = (char*)sqlite3_column_text(stmt, 0);
		char* description   = (char*)sqlite3_column_text(stmt, 1);
		uint32_t uptime     = sqlite3_column_int(stmt, 2);					// unchecked
		time_t last_update  = (time_t)sqlite3_column_int64(stmt, 3);

		record->mac_address = strdup(mac_address);
		record->description = strdup(description);	
		record->uptime = uptime;
		record->last_update = last_update;

		retval->push_back(record);
	}

	auto end_transaction = sqlite3_exec(db, "END TRANSACTION", NULL, NULL, NULL);
	if(end_transaction != SQLITE_OK) {
		log_synchronous(ERROR, "get_uptime_record: Failed to end transaction. "
			"SQLite Error: %d", end_transaction);
		return NULL;
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return retval;
}