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
 * Configuration handling
 */

#ifndef _METERD_CONFIG_H
#define _METERD_CONFIG_H

#include "config.h"
#include "meterd_types.h"
#include <libconfig.h>

#define COUNTER_TYPE_RAW	0
#define COUNTER_TYPE_CONSUMED	1
#define COUNTER_TYPE_PRODUCED	2

#define TABLE_PREFIX_RAW	"RAW_"
#define TABLE_PREFIX_PRODUCED	"PRODUCED_"
#define TABLE_PREFIX_CONSUMED	"CONSUMED_"

/* Counter specifications */
typedef struct counter_spec
{
	char*			description;	/* Short text description of the counter */
	char*			id;		/* Identifier of the counter */
	char*			table_name;	/* The database table name for this counter */
	int			type;		/* Counter type */
	struct counter_spec*	next;
}
counter_spec;

/* Initialise the configuration handler */
meterd_rv meterd_init_config_handling(const char* config_path);

/* Get an integer value */
meterd_rv meterd_conf_get_int(const char* base_path, const char* sub_path, int* value, int def_val);

/* Get a boolean value */
meterd_rv meterd_conf_get_bool(const char* base_path, const char* sub_path, int* value, int def_val);

/* Get a string value; note: caller must free string returned in value! */
meterd_rv meterd_conf_get_string(const char* base_path, const char* sub_path, char** value, char* def_val);

/* Get an array of string values; note: caller must free the array by calling the function below */
meterd_rv meterd_conf_get_string_array(const char* base_path, const char* sub_path, char*** value, int* count);

/* Free an array of string values */
meterd_rv meterd_conf_free_string_array(char** array, int count);

/* Retrieve a list of counter specifications */
meterd_rv meterd_conf_get_counter_specs(const char* base_path, const char* sub_path, int type, counter_spec** counter_specs);

/* Convert a counter ID to a table name */
char* meterd_conf_create_table_name(const char* id, int type);

/* Clean up counter specifications */
void meterd_conf_free_counter_specs(counter_spec* counter_specs);

/* Release the configuration handler */
meterd_rv meterd_uninit_config_handling(void);

/* Get a pointer to the internal configuration structure */
const config_t* meterd_conf_get_config_t(void);

#endif /* !_METERD_CONFIG_H */

