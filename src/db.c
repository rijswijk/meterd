/*
 * Copyright (c) 2014 Roland van Rijswijk-Deij
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Smart Meter Monitoring Daemon (meterd)
 * Database interaction
 */

#include "config.h"
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_log.h"
#include "db.h"
#include "utlist.h"
#include <pthread.h>
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static pthread_mutex_t meterd_db_mutex;

/* Initialise database handling */
meterd_rv meterd_db_init(void)
{
	if (pthread_mutex_init(&meterd_db_mutex, NULL) != 0)
	{
		return MRV_GENERAL_ERROR;
	}

	return MRV_OK;
}

/* Uninitialise database handling */
void meterd_db_finalize(void)
{
	pthread_mutex_destroy(&meterd_db_mutex);
}

/* Check if the specified database exists */
meterd_rv meterd_db_exists(const char* db_name)
{
	assert(db_name != NULL);

	FILE* exists = fopen(db_name, "r");

	if (exists)
	{
		fclose(exists);

		return MRV_OK;
	}
	else
	{
		return MRV_FILE_NOT_FOUND;
	}
}

/* Create and open the specified database */
meterd_rv meterd_db_create(const char* db_name, int force_create, void** db_handle)
{
	assert(db_name != NULL);
	assert(db_handle != NULL);

	sqlite3*	internal_handle	= NULL;
	char*		sql		= NULL;
	char*		errmsg		= NULL;

	/* First, make sure we're not unintentionally overwriting an existing DB */
	if (!force_create && (meterd_db_exists(db_name) == MRV_OK))
	{
		WARNING_MSG("Trying to create a database that already exists (%s)", db_name);

		return MRV_FILE_EXISTS;
	}
	else
	{
		/* Ensure that if an old database exists it is deleted first */
		unlink(db_name);
	}

	/* Create the database */
	if (sqlite3_open_v2(db_name, &internal_handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK)
	{
		ERROR_MSG("Failed to create database %s (%s)", db_name, sqlite3_errmsg(internal_handle));

		return MRV_DB_ERROR;
	}

	*db_handle = (void*) internal_handle;

	/* Turn on direct disk synchronisation (data immediately available) */
	sql = "PRAGMA synchronous=ON;";

	if (sqlite3_exec(internal_handle, sql, NULL, 0, &errmsg) != SQLITE_OK)
	{
		WARNING_MSG("Failed to turn off direct disk synchronisation (%s)", errmsg);

		sqlite3_free(errmsg);
	}

	/* Switch to write-ahead logging for concurrent DB access */
	sql = "PRAGMA journal_mode=WAL;";

	if (sqlite3_exec(internal_handle, sql, NULL, 0, &errmsg) != SQLITE_OK)
	{
		WARNING_MSG("Failed to switch new database to write-ahead logging mode (%s)", errmsg);

		sqlite3_free(errmsg);
	}

	return MRV_OK;
}

/* Create tables based on the supplied prefix and counter specifications */
meterd_rv meterd_db_create_tables(void* db_handle, counter_spec* counters)
{
	assert(db_handle != NULL);

	char*		sql		= NULL;
	char		sql_buf[4096]	= { 0 };
	char*		errmsg		= NULL;
	counter_spec*	ctr_it		= NULL;

	/* Create the configuration table */
	sql = 	"CREATE TABLE CONFIGURATION " \
		"(" \
			"id		VARCHAR(16) PRIMARY KEY," \
			"description	VARCHAR(255)," \
			"type		INTEGER," \
			"table_name	VARCHAR(255)" \
		");";

	if (sqlite3_exec((sqlite3*) db_handle, sql, NULL, 0, &errmsg) != SQLITE_OK)
	{
		ERROR_MSG("Failed to create configuration table (%s)", errmsg);

		sqlite3_free(errmsg);

		return MRV_DB_ERROR;
	}

	/* Populate the configuration table and create the data table */
	sql =	"INSERT INTO CONFIGURATION (id,description,type,table_name) VALUES ('%s','%s',%d,'%s');" \
		"CREATE TABLE %s " \
		"(" \
			"timestamp	INTEGER," \
			"value		DOUBLE," \
			"unit		VARCHAR(16)"
		");";

	LL_FOREACH(counters, ctr_it)
	{
		snprintf(sql_buf, 4096, sql, ctr_it->id, ctr_it->description, ctr_it->type, ctr_it->table_name, ctr_it->table_name);

		if (sqlite3_exec((sqlite3*) db_handle, sql_buf, NULL, 0, &errmsg) != SQLITE_OK)
		{
			ERROR_MSG("Failed to insert counter %s into CONFIGURATION table or create table %s (%s)", ctr_it->id, ctr_it->table_name, errmsg);

			sqlite3_free(errmsg);

			return MRV_DB_ERROR;
		}
	}

	return MRV_OK;
}

/* Open the specified database */
meterd_rv meterd_db_open(const char* db_name, int read_only, void** db_handle)
{
	assert(db_name != NULL);
	assert(db_handle != NULL);

	sqlite3*	internal_handle	= NULL;
	char*		sql		= NULL;
	char*		errmsg		= NULL;

	if (sqlite3_open_v2(db_name, &internal_handle, (read_only ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE) | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK)
	{
		ERROR_MSG("Failed to open database %s (%s)", db_name, sqlite3_errmsg(internal_handle));

		return MRV_DB_ERROR;
	}

	*db_handle = (void*) internal_handle;

	
	/* Turn off direct disk synchronisation (data immediately available) */
	sql = "PRAGMA synchronous=ON;";

	if (sqlite3_exec(internal_handle, sql, NULL, 0, &errmsg) != SQLITE_OK)
	{
		WARNING_MSG("Failed to turn off direct disk synchronisation (%s)", errmsg);

		sqlite3_free(errmsg);
	}

	return MRV_OK;
}

/* Record a measurement in the specified table of the specified database */
meterd_rv meterd_db_record(void* db_handle, const char* table_name, long double value, const char* unit, int timestamp)
{
	assert(db_handle != NULL);
	assert(table_name != NULL);
	assert(unit != NULL);

	char*	sql		= NULL;
	char*	errmsg		= NULL;
	char	sql_buf[4096]	= { 0 };

	sql = "INSERT INTO %s (timestamp, value, unit) VALUES (%d,%Lf,'%s');";

	snprintf(sql_buf, 4096, sql, table_name, timestamp, value, unit);

	if (sqlite3_exec((sqlite3*) db_handle, sql_buf, NULL, 0, &errmsg) != SQLITE_OK)
	{
		WARNING_MSG("Failed to record new measurement in the database (%s)", errmsg);

		sqlite3_free(errmsg);
	}

	return MRV_OK;
}

static int meterd_db_get_config_cb(void* data, int argc, char* argv[], char* colname[])
{
	assert(data != NULL);

	char**	table_name = (char**) data;

	if (argc != 1)
	{
		ERROR_MSG("Invalid database format detected");

		return -1;
	}

	*table_name = strdup(argv[0]);

	return 0;
}

static int meterd_db_get_results_cb(void* data, int argc, char* argv[], char* colname[])
{
	assert(data != NULL);

	db_res_ctr** results = (db_res_ctr**) data;
	db_res_ctr* new_result = NULL;

	if (argc != 3)
	{
		ERROR_MSG("Invalid database format detected");
	}

	new_result = (db_res_ctr*) malloc(sizeof(db_res_ctr));

	new_result->timestamp = atoi(argv[0]);
	new_result->value = strtold(argv[1], NULL);
	new_result->unit = strdup(argv[2]);

	LL_APPEND(*results, new_result);

	return 0;
}

/* Retrieve results from the database */
meterd_rv meterd_db_get_results(void* db_handle, const char* id, long double invert, db_res_ctr** results, int select_from)
{
	assert(id != NULL);
	assert(db_handle != NULL);
	assert(results != NULL);

	char* 	sql		= NULL;
	char* 	errmsg		= NULL;
	char	sql_buf[4096]	= { 0 };
	char*	table_name	= NULL;

	DEBUG_MSG("Retrieving data for ID %s", id);

	/* First, find the counter in the configuration table of the database */
	sql = "SELECT table_name FROM CONFIGURATION WHERE id='%s';";

	snprintf(sql_buf, 4096, sql, id);

	if (sqlite3_exec((sqlite3*) db_handle, sql_buf, meterd_db_get_config_cb, (void*) &table_name, &errmsg) != SQLITE_OK)
	{
		ERROR_MSG("Failed to retrieve table name for ID %s from the database (%s)", id, errmsg);

		sqlite3_free(errmsg);

		return MRV_DB_ERROR;
	}

	DEBUG_MSG("Data for ID %s is in table %s", id, table_name);

	/* Now, select the data for the specified interval */
	sql = "SELECT * FROM %s WHERE timestamp >= %d;";

	snprintf(sql_buf, 4096, sql, table_name, select_from);

	if (sqlite3_exec((sqlite3*) db_handle, sql_buf, meterd_db_get_results_cb, (void*) results, &errmsg) != SQLITE_OK)
	{
		ERROR_MSG("Failed to retrieve results from table %s (%s)", table_name, errmsg);

		sqlite3_free(errmsg);

		free(table_name);

		return MRV_DB_ERROR;
	}

	/* Apply inversion */
	if (invert < 0.0f)
	{
		db_res_ctr* res_it = NULL;

		LL_FOREACH(*results, res_it)
		{
			res_it->value *= invert;
		}
	}

	free(table_name);

	return MRV_OK;
}

/* Close the specified database */
void meterd_db_close(void* db_handle)
{
	if (db_handle != NULL)
	{
		sqlite3_close((sqlite3*) db_handle);
	}
}

