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

/*
 * Smart Meter Monitoring Daemon (meterd)
 * Configuration handling
 */

#include "config.h"
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "utlist.h"
#include "meterd_log.h"
#include <libconfig.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

/* The configuration */
config_t configuration;

static const char* prefixes[3] =
{
	TABLE_PREFIX_RAW,
	TABLE_PREFIX_CONSUMED,
	TABLE_PREFIX_PRODUCED
};

/* Initialise the configuration handler */
meterd_rv meterd_init_config_handling(const char* config_path)
{
	if ((config_path == NULL) || (strlen(config_path) == 0))
	{
		return MRV_NO_CONFIG;
	}

	/* Initialise the configuration */
	config_init(&configuration);

	/* Load the configuration from the specified file */
	if (config_read_file(&configuration, config_path) != CONFIG_TRUE)
	{
		fprintf(stderr, "Failed to read the configuration: %s (%s:%d)\n",
			config_error_text(&configuration),
			config_path,
			config_error_line(&configuration));

		config_destroy(&configuration);

		return MRV_CONFIG_ERROR;
	}

	return MRV_OK;
}

/* Release the configuration handler */
meterd_rv meterd_uninit_config_handling(void)
{
	/* Uninitialise the configuration */
	config_destroy(&configuration);

	return MRV_OK;
}

/* Get an integer value */
meterd_rv meterd_conf_get_int(const char* base_path, const char* sub_path, int* value, int def_val)
{
	/* Unfortunately, the kludge below is necessary since the interface for config_lookup_int changed between
	 * libconfig version 1.3 and 1.4 */
#ifndef LIBCONFIG_VER_MAJOR /* this means it is a pre 1.4 version */
	long conf_val = 0;
#else
	int conf_val = 0;
#endif /* libconfig API kludge */
	static char path_buf[8192];

	if ((base_path == NULL) || (sub_path == NULL) || (value == NULL))
	{
		return MRV_PARAM_INVALID;
	}

	snprintf(path_buf, 8192, "%s.%s", base_path, sub_path);

	if (config_lookup_int(&configuration, path_buf, &conf_val) != CONFIG_TRUE)
	{
		*value = def_val;
	}
	else
	{
		*value = conf_val;
	}

	return MRV_OK;
}

/* Get a boolean value */
meterd_rv meterd_conf_get_bool(const char* base_path, const char* sub_path, int* value, int def_val)
{
	int conf_val = 0;
	static char path_buf[8192];

	if ((base_path == NULL) || (sub_path == NULL) || (value == NULL))
	{
		return MRV_PARAM_INVALID;
	}

	snprintf(path_buf, 8192, "%s.%s", base_path, sub_path);

	if (config_lookup_bool(&configuration, path_buf, &conf_val) != CONFIG_TRUE)
	{
		*value = def_val;
	}
	else
	{
		*value = (conf_val == CONFIG_TRUE) ? 1 : 0;
	}

	return MRV_OK;
}

/* Get a string value; note: caller must free string returned in value! */
meterd_rv meterd_conf_get_string(const char* base_path, const char* sub_path, char** value, char* def_val)
{
	const char* conf_val = NULL;
	static char path_buf[8192];

	if ((base_path == NULL) || (sub_path == NULL) || (value == NULL))
	{
		return MRV_PARAM_INVALID;
	}

	snprintf(path_buf, 8192, "%s.%s", base_path, sub_path);

	if (config_lookup_string(&configuration, path_buf, &conf_val) != CONFIG_TRUE)
	{
		if (def_val == NULL)
		{
			*value = NULL;
		}
		else
		{
			*value = strdup(def_val);
		}
	}
	else
	{
		*value = strdup(conf_val);
	}

	return MRV_OK;
}

/* Get an array of string values; note: caller must free the array by calling the function below */
meterd_rv meterd_conf_get_string_array(const char* base_path, const char* sub_path, char*** value, int* count)
{
	config_setting_t* array = NULL;
	static char path_buf[8192];

	if ((base_path == NULL) || (sub_path == NULL) || (value == NULL) || (count == NULL))
	{
		return MRV_PARAM_INVALID;
	}

	snprintf(path_buf, 8192, "%s.%s", base_path, sub_path);

	*count = 0;

	array = config_lookup(&configuration, path_buf);

	if (array != NULL)
	{
		int elem_count = 0;
		int i = 0;

		/* Check if it is an array */
		if (config_setting_is_array(array) == CONFIG_FALSE)
		{
			return MRV_CONFIG_NO_ARRAY;
		}

		/* The array was found, retrieve the strings */
		elem_count = config_setting_length(array);

		/* Now allocate memory for the string array */
		*value = (char**) malloc(elem_count * sizeof(char**));

		if (*value == NULL)
		{
			return MRV_MEMORY;
		}

		for (i = 0; i < elem_count; i++)
		{
			/* Retrieve the individual element */
			const char* string_value = config_setting_get_string_elem(array, i);

			if (string_value != NULL)
			{
				(*value)[i] = strdup(string_value);
			}
			else
			{
				(*value)[i] = strdup("");
			}
		}

		*count = elem_count;
	}

	return MRV_OK;
}

/* Free an array of string values */
meterd_rv meterd_conf_free_string_array(char** array, int count)
{
	int i = 0;

	for (i = 0; i < count; i++)
	{
		free(array[i]);
	}

	free(array);

	return MRV_OK;
}

/* Retrieve a list of counter specifications */
meterd_rv meterd_conf_get_counter_specs(const char* base_path, const char* sub_path, int type, counter_spec** counter_specs)
{
	assert(base_path != NULL);
	assert(sub_path != NULL);
	assert(counter_specs != NULL);

	char			path_buf[8192]	= { 0 };
	unsigned int		counter_count	= 0;
	unsigned int		i		= 0;
	config_setting_t*	counters_conf	= NULL;

	snprintf(path_buf, 8192, "%s.%s", base_path, sub_path);

	counters_conf = config_lookup(&configuration, path_buf);

	if (counters_conf == NULL)
	{
		ERROR_MSG("No counters specified under %s in the configuration file", path_buf);

		return MRV_CONF_NO_COUNTERS;
	}

	counter_count = config_setting_length(counters_conf);

	for (i = 0; i < counter_count; i++)
	{
		config_setting_t*	counter_conf		= NULL;
		counter_spec*		new_counter		= NULL;
		const char*		description		= NULL;
		const char*		id			= NULL;

		/* Retrieve the next configured counter */
		counter_conf = config_setting_get_elem(counters_conf, i);

		if (counter_conf == NULL)
		{
			ERROR_MSG("Failed to enumerate next counter specification");

			continue;
		}
		
		/* Retrieve the description */
		if ((config_setting_lookup_string(counter_conf, "description", &description) != CONFIG_TRUE) || (description == NULL))
		{
			ERROR_MSG("No description for counter %s", config_setting_name(counter_conf));

			continue;
		}
		
		/* Retrieve the ID */
		if ((config_setting_lookup_string(counter_conf, "id", &id) != CONFIG_TRUE) || (id == NULL))
		{
			ERROR_MSG("No ID for counter %s", config_setting_name(counter_conf));

			continue;
		}

		new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		if (new_counter == NULL)
		{
			return MRV_MEMORY;
		}
		
		new_counter->description 	= strdup(description);
		new_counter->id			= strdup(id);
		new_counter->table_name		= meterd_conf_create_table_name(id, type);
		new_counter->type		= type;

		LL_APPEND((*counter_specs), new_counter);
	}

	return MRV_OK;
}

/* Convert a counter ID to a table name */
char* meterd_conf_create_table_name(const char* id, int type)
{
	char*	table_name_id		= NULL;
	char	table_name_buf[256]	= { 0 };

	/* Create the table name */
	table_name_id = strdup(id);

	if (table_name_id == NULL)
	{
		return NULL;
	}

	while (strchr(table_name_id, '.') != NULL)
	{
		(*strchr(table_name_id, '.')) = '_';
	}

	snprintf(table_name_buf, 256, "%s%s", prefixes[type], table_name_id);

	free(table_name_id);

	return strdup(table_name_buf);
}

/* Clean up counter specifications */
void meterd_conf_free_counter_specs(counter_spec* counter_specs)
{
	counter_spec*	ctr_it	= NULL;
	counter_spec*	ctr_tmp	= NULL;

	LL_FOREACH_SAFE(counter_specs, ctr_it, ctr_tmp)
	{
		LL_DELETE(counter_specs, ctr_it);

		free(ctr_it->description);
		free(ctr_it->id);
		free(ctr_it->table_name);
		free(ctr_it);
	}
}

/* Retrieve the configured recurring tasks */
meterd_rv meterd_conf_get_scheduled_tasks(scheduled_task** tasks)
{
	assert(tasks != NULL);

	unsigned int 		tasks_count	= 0;
	unsigned int		i		= 0;
	unsigned int		j		= 0;
	config_setting_t*	tasks_conf	= NULL;

	tasks_conf = config_lookup(&configuration, "tasks");

	if (tasks_conf == NULL)
	{
		/* Configuring tasks is optional, but inform the user anyway */
		INFO_MSG("No tasks configured");

		return MRV_OK;
	}

	tasks_count = config_setting_length(tasks_conf);

	for (i = 0; i < tasks_count; i++)
	{
		config_setting_t*	task		= NULL;
		scheduled_task*		new_task	= NULL;

		task = config_setting_get_elem(tasks_conf, i);

		if (task != NULL)
		{
			unsigned int 	task_elem_count	= config_setting_length(task);
			const char*	description	= NULL;

			/* Unfortunately, the kludge below is necessary since the interface for config_lookup_int changed between
	 		 * libconfig version 1.3 and 1.4 */
#ifndef LIBCONFIG_VER_MAJOR /* this means it is a pre 1.4 version */
			long int 	interval 	= 0;
#else
			int 		interval 	= 0;
#endif /* libconfig API kludge */

			/* First, get the known elements of the task */
			if ((config_setting_lookup_string(task, "description", &description) != CONFIG_TRUE) || (description == NULL))
			{
				ERROR_MSG("No description for task %s", config_setting_name(task));

				continue;
			}

			if (config_setting_lookup_int(task, "interval", &interval) != CONFIG_TRUE)
			{
				ERROR_MSG("No interval specified for task %s", config_setting_name(task));

				continue;
			}

			if (interval <= 0)
			{
				ERROR_MSG("Invalid interval %d specified for task %s (must be > 0)", interval, config_setting_name(task));

				continue;
			}

			new_task = (scheduled_task*) calloc(1, sizeof(scheduled_task));

			for (j = 0; j < task_elem_count; j++)
			{
				config_setting_t*	cmd	= NULL;
				const char*		cmd_val	= NULL;

				cmd = config_setting_get_elem(task, j);

				if (cmd != NULL)
				{
					/* Check if this is a command */
					if ((strlen(config_setting_name(cmd)) >= 3) && !strncasecmp(config_setting_name(cmd), "cmd", 3))
					{
						cmd_val = config_setting_get_string(cmd);

						if (cmd_val == NULL)
						{
							WARNING_MSG("Empty command %s in task %s", config_setting_name(cmd), config_setting_name(task));

							continue;
						}
						
						new_task->num_cmds++;

						new_task->cmds = (char**) realloc(new_task->cmds, new_task->num_cmds * sizeof(char*));
						new_task->cmds[new_task->num_cmds - 1] = strdup(cmd_val);
					}
				}
			}

			if (new_task->num_cmds == 0)
			{
				ERROR_MSG("Task %s has no commands", config_setting_name(task));

				free(new_task);

				continue;
			}

			/* Copy in remaining configuration */
			new_task->description = strdup(description);
			new_task->interval = interval;

			LL_APPEND(*tasks, new_task);
		}
	}

	return MRV_OK;
}

/* Clean up scheduled tasks */
void meterd_conf_free_scheduled_tasks(scheduled_task* tasks)
{
	scheduled_task*	task_it		= NULL;
	scheduled_task*	task_tmp	= NULL;
	size_t		i		= 0;

	LL_FOREACH_SAFE(tasks, task_it, task_tmp)
	{
		LL_DELETE(tasks, task_it);

		free(task_it->description);

		for (i = 0; i < task_it->num_cmds; i++)
		{
			free(task_it->cmds[i]);
		}

		free(task_it->cmds);
		free(task_it);
	}
}

/* Get a pointer to the internal configuration structure */
const config_t* meterd_conf_get_config_t(void)
{
	return &configuration;
}

