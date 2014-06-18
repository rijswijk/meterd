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
#include <pthread.h>
#include <sqlite3.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

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
		WARNING_MSG("Trying to create a database that already exists (%d)", db_name);

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

	/* Turn off direct disk synchronisation (speed improvement) */
	sql = "PRAGMA synchronous=OFF;";

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

	
	/* Turn off direct disk synchronisation (speed improvement) */
	sql = "PRAGMA synchronous=OFF;";

	if (sqlite3_exec(internal_handle, sql, NULL, 0, &errmsg) != SQLITE_OK)
	{
		WARNING_MSG("Failed to turn off direct disk synchronisation (%s)", errmsg);

		sqlite3_free(errmsg);
	}

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

