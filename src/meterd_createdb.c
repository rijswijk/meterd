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
 * GOODS OR SMRVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include "db.h"
#include "utlist.h"

void version(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n", VERSION);
	printf("Database initialisation utility\n");
	printf("Copyright (c) 2014 Roland van Rijswijk-Deij\n\n");
	printf("Use, modification and redistribution of this software is subject to the terms\n");
	printf("of the license agreement. This software is licensed under a 2-clause BSD-style\n");
	printf("license a copy of which is included as the file LICENSE in the distribution.\n");
}

void usage(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n\n", VERSION);
	printf("Database initialisation utility\n");
	printf("Usage:\n");
	printf("\tmeterd-createdb [-c <config>] [-f]\n");
	printf("\tmeterd-createdb -h\n");
	printf("\tmeterd-createdb -v\n");
	printf("\n");
	printf("\t-c <config>   Use <config> as configuration file\n");
	printf("\t              Defaults to %s\n", DEFAULT_METERD_CONF);
	printf("\t-f            Force overwriting of existing databases\n");
	printf("\n");
	printf("\t-h            Print this help message\n");
	printf("\n");
	printf("\t-v            Print the version number\n");
}

meterd_rv meterd_createdb_raw(const char* type, int force_overwrite)
{
	assert(type != NULL);

	char*		db_name		= NULL;
	char*		id_cur_consume	= NULL;
	char*		id_cur_produce	= NULL;
	void*		db_handle	= NULL;
	meterd_rv	rv		= MRV_OK;
	counter_spec*	ctr_specs	= NULL;

	/* Check if the database type is configured */
	if ((rv = meterd_conf_get_string("database", type, &db_name, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve configuration option database.%s", type);

		return rv;
	}

	if (db_name == NULL)
	{
		INFO_MSG("No database of type %s configured", type);

		return MRV_OK;
	}

	/* Retrieve counters for current consumption and production */
	if ((rv = meterd_conf_get_string("database", "current_consumption_id", &id_cur_consume, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve configuration option database.current_consumption");

		free(db_name);

		return rv;
	}

	if (id_cur_consume != NULL)
	{
		counter_spec* new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		if (new_counter == NULL)
		{
			free(db_name);
			free(id_cur_consume);

			return MRV_MEMORY;
		}

		new_counter->description 	= strdup("Current consumption");
		new_counter->id			= strdup(id_cur_consume);
		new_counter->table_name		= meterd_conf_create_table_name(id_cur_consume, COUNTER_TYPE_RAW);
		new_counter->type		= COUNTER_TYPE_RAW;

		LL_APPEND(ctr_specs, new_counter);

		free(id_cur_consume);
	}

	if ((rv = meterd_conf_get_string("database", "current_production_id", &id_cur_produce, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve configuration option database.current_production");

		free(db_name);
		meterd_conf_free_counter_specs(ctr_specs);

		return rv;
	}

	if (id_cur_produce != NULL)
	{
		counter_spec* new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		if (new_counter == NULL)
		{
			free(db_name);
			free(id_cur_consume);
			meterd_conf_free_counter_specs(ctr_specs);

			return MRV_MEMORY;
		}

		new_counter->description 	= strdup("Current production");
		new_counter->id 		= strdup(id_cur_produce);
		new_counter->table_name		= meterd_conf_create_table_name(id_cur_produce, COUNTER_TYPE_RAW);
		new_counter->type		= COUNTER_TYPE_RAW;

		LL_APPEND(ctr_specs, new_counter);

		free(id_cur_produce);
	}

	if (ctr_specs == NULL)
	{
		INFO_MSG("No raw consumption or production counters specified, skipping creation of database %s of type %s", db_name, type);

		free(db_name);

		return MRV_OK;
	}

	/* Create and open the database */
	if ((rv = meterd_db_create(db_name, force_overwrite, &db_handle)) != MRV_OK)
	{
		ERROR_MSG("Failed to create database %s of type %s", db_name, type);

		free(db_name);
		meterd_conf_free_counter_specs(ctr_specs);

		return rv;
	}

	INFO_MSG("Created database %s of type %s", db_name, type);

	/* Create data tables */
	if ((rv = meterd_db_create_tables(db_handle, ctr_specs)) != MRV_OK)
	{
		ERROR_MSG("Error during table creation");

		unlink(db_name);
	}

	free(db_name);
	meterd_conf_free_counter_specs(ctr_specs);

	/* Close the database */
	meterd_db_close(db_handle);

	return rv;
}

meterd_rv meterd_createdb_counters(int force_overwrite)
{
	counter_spec*	counters	= NULL;
	char*		db_name		= NULL;
	char*		gas_id		= NULL;
	char*		gas_description	= NULL;
	meterd_rv	rv		= MRV_OK;
	void*		db_handle	= NULL;

	/* Check if the database type is configured */
	if ((rv = meterd_conf_get_string("database", "counters", &db_name, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve configuration option database.counters");

		return rv;
	}

	if (db_name == NULL)
	{
		INFO_MSG("No database for consumption and production counters specified, skipping");

		return MRV_OK;
	}

	/* Retrieve the consumption counters */
	if ((rv = meterd_conf_get_counter_specs("database", "consumption", COUNTER_TYPE_CONSUMED, &counters)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve consumption counter configuration");

		return rv;
	}

	/* Retrieve the production counters */
	if ((rv = meterd_conf_get_counter_specs("database", "production", COUNTER_TYPE_PRODUCED, &counters)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve production counter configuration");

		return rv;
	}

	/* Check if there is a gas counter configured */
	if (((rv = meterd_conf_get_string("database.gascounter", "id", &gas_id, NULL)) != MRV_OK) ||
	    ((rv = meterd_conf_get_string("database.gascounter", "description", &gas_description, NULL)) != MRV_OK))
	{
		ERROR_MSG("Failed to retrieve gas counter configuration");

		free(db_name);
		free(gas_id);
		free(gas_description);
		meterd_conf_free_counter_specs(counters);

		return rv;
	}

	/* Add the gas counter if specified */
	if (gas_id != NULL)
	{
		counter_spec* new_counter = NULL;

		if (gas_description == NULL)
		{
			gas_description = strdup("Gas");
		}

		new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		if (new_counter == NULL)
		{
			free(db_name);
			free(gas_id);
			free(gas_description);
			meterd_conf_free_counter_specs(counters);

			return MRV_MEMORY;
		}

		new_counter->id 		= gas_id;
		new_counter->description	= gas_description;
		new_counter->table_name		= meterd_conf_create_table_name(gas_id, COUNTER_TYPE_CONSUMED);
		new_counter->type		= COUNTER_TYPE_CONSUMED;

		LL_APPEND(counters, new_counter);
	}

	/* Create and open the database */
	if ((rv = meterd_db_create(db_name, force_overwrite, &db_handle)) != MRV_OK)
	{
		ERROR_MSG("Failed to create database %s for counters", db_name);

		free(db_name);
		meterd_conf_free_counter_specs(counters);

		return rv;
	}

	INFO_MSG("Created database %s for counters", db_name);

	/* Create data tables */
	if ((rv = meterd_db_create_tables(db_handle, counters)) != MRV_OK)
	{
		ERROR_MSG("Error during table creation");

		unlink(db_name);
	}

	free(db_name);
	meterd_conf_free_counter_specs(counters);

	/* Close the database */
	meterd_db_close(db_handle);

	return rv;
}

int main(int argc, char* argv[])
{
	char* 	config_path 	= NULL;
	int	force_overwrite	= 0;
	int 	c 		= 0;
	
	while ((c = getopt(argc, argv, "c:fhv")) != -1)
	{
		switch(c)
		{
		case 'c':
			config_path = strdup(optarg);

			if (config_path == NULL)
			{
				fprintf(stderr, "Error allocating memory, exiting\n");
				return MRV_MEMORY;
			}

			break;
		case 'f':
			force_overwrite = 1;
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			version();
			return 0;
		}
	}

	if (config_path == NULL)
	{
		config_path = strdup(DEFAULT_METERD_CONF);

		if (config_path == NULL)
		{
			fprintf(stderr, "Error allocating memory, exiting\n");
			return MRV_MEMORY;
		}
	}

	/* Load the configuration */
	if (meterd_init_config_handling(config_path) != MRV_OK)
	{
		fprintf(stderr, "Failed to load the configuration, exiting\n");

		return MRV_CONFIG_ERROR;
	}

	/* Initialise logging */
	if (meterd_init_log() != MRV_OK)
	{
		fprintf(stderr, "Failed to initialise logging, exiting\n");

		return MRV_LOG_INIT_FAIL;
	}

	INFO_MSG("Smart Meter Monitoring Daemon (meterd) version %s", VERSION);
	INFO_MSG("Starting database creation");

	/* Initialise database handling */
	if (meterd_db_init() != MRV_OK)
	{
		ERROR_MSG("Failed to initialise database handling, giving up");

		return MRV_DB_ERROR;
	}

	if (force_overwrite)
	{
		INFO_MSG("Will overwrite existing databases");
	}

	/* Create raw databases */
	meterd_createdb_raw("raw_db", force_overwrite);
	meterd_createdb_raw("fivemin_avg", force_overwrite);
	meterd_createdb_raw("hourly_avg", force_overwrite);

	/* Create counters database */
	meterd_createdb_counters(force_overwrite);

	INFO_MSG("Finished database creation");

	/* Uninitialise database handling */
	meterd_db_finalize();

	/* Uninitialise logging */
	if (meterd_uninit_log() != MRV_OK)
	{
		fprintf(stderr, "Failed to uninitialise logging\n");
	}

	free(config_path);

	return MRV_OK;
}

