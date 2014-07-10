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
 * Main measurement loop
 */

#include "config.h"
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include "measure.h"
#include "db.h"
#include "comm.h"
#include <stdlib.h>
#include <string.h>
#include "utlist.h"
#include "p1_parser.h"

/* Module variables */
static void*		raw_db_h	= NULL;
static void*		fivemin_db_h	= NULL;
static void*		hourly_db_h	= NULL;
static void*		cumul_db_h	= NULL;
static counter_spec*	counters	= NULL;
static int		run_measurement	= 1;
static char*		gas_id		= NULL;
static int		total_interval	= 0;

/* Initialise measuring */
meterd_rv meterd_measure_init(void)
{
	char* 		raw_db_name	= NULL;
	char* 		fivemin_db_name	= NULL;
	char*		hourly_db_name	= NULL;
	char*		cumul_db_name	= NULL;
	meterd_rv	rv		= MRV_OK;
	char*		id_cur_consume	= NULL;
	char*		id_cur_produce	= NULL;
	counter_spec*	new_counter	= NULL;
	counter_spec*	counter_it	= NULL;

	INFO_MSG("Initialising measurement subsystem");

	/* Get database names */
	if (meterd_conf_get_string("database", "raw_db", &raw_db_name, NULL) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve raw database name from the configuration");
	}

	if (meterd_conf_get_string("database", "fivemin_avg", &fivemin_db_name, NULL) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve 5 minute average database name from the configuration");
	}

	if (meterd_conf_get_string("database", "hourly_avg", &hourly_db_name, NULL) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve hourly average database name from the configuration");
	}

	if (meterd_conf_get_string("database", "total_consumed", &cumul_db_name, NULL) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve total consumption/production database name from the configuration");
	}

	if ((raw_db_name == NULL) &&
	    (fivemin_db_name == NULL) &&
	    (hourly_db_name == NULL) &&
	    (cumul_db_name == NULL))
	{
		ERROR_MSG("No databases configured, please fix the configuration");

		return MRV_DB_ERROR;
	}

	/* Open databases */
	if ((raw_db_name != NULL) && (meterd_db_open(raw_db_name, 0, &raw_db_h) != MRV_OK))
	{
		ERROR_MSG("Failed to open %s as raw database", raw_db_name);
	}
	else
	{
		INFO_MSG("Writing raw measurement data to %s", raw_db_name);
	}

	if ((fivemin_db_name != NULL) && (meterd_db_open(fivemin_db_name, 0, &fivemin_db_h) != MRV_OK))
	{
		ERROR_MSG("Failed to open %s as 5 minute average database", fivemin_db_name);
	}
	else
	{
		INFO_MSG("Writing 5 minute average values to %s", fivemin_db_name);
	}

	if ((hourly_db_name != NULL) && (meterd_db_open(hourly_db_name, 0, &hourly_db_h) != MRV_OK))
	{
		ERROR_MSG("Failed to open %s as hourly average database", hourly_db_name);
	}
	else
	{
		INFO_MSG("Writing hourly average values to %s", hourly_db_name);
	}

	if ((cumul_db_name != NULL) && (meterd_db_open(cumul_db_name, 0, &cumul_db_h) != MRV_OK))
	{
		ERROR_MSG("Failed to open %s as cumulative consumption/production database", cumul_db_name);
	}
	else
	{
		INFO_MSG("Writing cumulative consumption/production data to %s", cumul_db_name);
	}

	free(raw_db_name);
	free(fivemin_db_name);
	free(hourly_db_name);
	free(cumul_db_name);

	if ((raw_db_h == NULL) &&
	    (fivemin_db_h == NULL) &&
	    (hourly_db_h == NULL) &&
	    (cumul_db_h == NULL))
	{
		ERROR_MSG("Failed to open any database, please fix the configuration");

		return MRV_DB_ERROR;
	}

	/* Initialise communications */
	if ((rv = meterd_comm_init()) != MRV_OK)
	{
		meterd_measure_finalize();

		return rv;
	}

	/* Read counter specifications from the configuration */
	
	/* Add raw data counters */
	if ((rv = meterd_conf_get_string("database", "current_consumption_id", &id_cur_consume, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to get identifier for current consumption counter from the configuration");
	}

	if (id_cur_consume != NULL)
	{
		new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		new_counter->description 	= strdup("Current consumption");
		new_counter->id			= id_cur_consume;
		new_counter->table_name		= meterd_conf_create_table_name(id_cur_consume, COUNTER_TYPE_RAW);
		new_counter->type		= COUNTER_TYPE_RAW;
		new_counter->last_val		= 0.0f;
		new_counter->last_ts		= 0;
		new_counter->fivemin_cumul	= 0.0f;
		new_counter->fivemin_ctr	= 0;
		new_counter->fivemin_ts		= 0;
		new_counter->hourly_cumul	= 0.0f;
		new_counter->hourly_ctr		= 0;
		new_counter->hourly_ts		= 0;
		new_counter->raw_db_h		= raw_db_h;
		new_counter->fivemin_db_h	= fivemin_db_h;
		new_counter->hourly_db_h	= hourly_db_h;

		LL_APPEND(counters, new_counter);
	}

	if ((rv = meterd_conf_get_string("database", "current_production_id", &id_cur_produce, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to get identifier for current production counter from the configuration");
	}

	if (id_cur_produce != NULL)
	{
		new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		new_counter->description 	= strdup("Current production");
		new_counter->id			= id_cur_produce;
		new_counter->table_name		= meterd_conf_create_table_name(id_cur_produce, COUNTER_TYPE_RAW);
		new_counter->type		= COUNTER_TYPE_RAW;
		new_counter->last_val		= 0.0f;
		new_counter->last_ts		= 0;
		new_counter->fivemin_cumul	= 0.0f;
		new_counter->fivemin_ctr	= 0;
		new_counter->fivemin_ts		= 0;
		new_counter->hourly_cumul	= 0.0f;
		new_counter->hourly_ctr		= 0;
		new_counter->hourly_ts		= 0;
		new_counter->raw_db_h		= raw_db_h;
		new_counter->fivemin_db_h	= fivemin_db_h;
		new_counter->hourly_db_h	= hourly_db_h;

		LL_APPEND(counters, new_counter);
	}

	/* Add consumption counters */
	new_counter 	= NULL;
	counter_it 	= NULL;

	if ((rv = meterd_conf_get_counter_specs("database", "consumption", COUNTER_TYPE_CONSUMED, &new_counter)) != MRV_OK)
	{
		ERROR_MSG("Failed to get consumption counter specifications from the configuration");
	}

	LL_FOREACH(new_counter, counter_it)
	{
		counter_it->last_val		= 0.0f;
		counter_it->last_ts		= 0;
		counter_it->cumul_rec_ts	= 0;
		counter_it->cumul_db_h		= cumul_db_h;
	}

	LL_CONCAT(counters, new_counter);

	/* Add production counters */
	new_counter	= NULL;
	counter_it	= NULL;

	if ((rv = meterd_conf_get_counter_specs("database", "production", COUNTER_TYPE_PRODUCED, &new_counter)) != MRV_OK)
	{
		ERROR_MSG("Failed to get production counter specifications from the configuration");
	}

	LL_FOREACH(new_counter, counter_it)
	{
		counter_it->last_val		= 0.0f;
		counter_it->last_ts		= 0;
		counter_it->cumul_rec_ts	= 0;
		counter_it->cumul_db_h		= cumul_db_h;
	}

	LL_CONCAT(counters, new_counter);

	/* Get gas identifier */
	if ((rv = meterd_conf_get_string("database", "gascounter.id", &gas_id, NULL)) != MRV_OK)
	{
		ERROR_MSG("Failed to retrieve ID for gas counter from the configuration");
	}

	if (gas_id != NULL)
	{
		new_counter = (counter_spec*) malloc(sizeof(counter_spec));

		new_counter->id			= strdup(gas_id);
		new_counter->description	= strdup("Gas");
		new_counter->table_name		= meterd_conf_create_table_name(gas_id, COUNTER_TYPE_CONSUMED);
		new_counter->type		= COUNTER_TYPE_CONSUMED;
		new_counter->last_val		= 0.0f;
		new_counter->last_ts		= 0;
		new_counter->cumul_rec_ts	= 0;
		new_counter->cumul_db_h		= cumul_db_h;

		LL_APPEND(counters, new_counter);
	}

	/* Get interval for recording total consumed/produced values */
	if ((rv = meterd_conf_get_int("database", "total_interval", &total_interval, 300)) != MRV_OK)
	{
		ERROR_MSG("Failed to get interval between recording total consumed/produced values from the configuration");
	}

	return MRV_OK;
}

/* Run the main measurement loop until interrupted */
void meterd_measure_loop(void)
{
	meterd_rv	rv		= MRV_OK;
	telegram_ll*	p1		= NULL;
	smart_counter*	p1_counters	= NULL;
	smart_counter*	p1_ctr_it	= NULL;
	counter_spec*	ctr_it		= NULL;

	while(run_measurement)
	{
		if ((rv = meterd_comm_recv_p1(&p1)) != MRV_OK)
		{
			if (rv == MRV_COMM_INTR)
			{
				WARNING_MSG("Interrupted by signal, continuing");

				continue;
			}

			ERROR_MSG("Communication error, giving up");

			break;
		}

		/* Parse the telegram */
		if (meterd_parse_p1_telegram(p1, gas_id, &p1_counters) == MRV_OK)
		{
			time_t 	now 	= time(NULL);
			int 	db_ts	= (int) now;

			/* Record values of the counters where appropriate */
			LL_FOREACH(p1_counters, p1_ctr_it)
			{
				LL_FOREACH(counters, ctr_it)
				{
					if (!strcmp(ctr_it->id, p1_ctr_it->id))
					{
						ctr_it->last_val 	= 	p1_ctr_it->value;
						ctr_it->last_ts		= 	now;

						if (ctr_it->type == COUNTER_TYPE_RAW)
						{
							ctr_it->fivemin_cumul	+= 	p1_ctr_it->value;
							ctr_it->fivemin_ctr++;
							ctr_it->hourly_cumul	+=	p1_ctr_it->value;
							ctr_it->hourly_ctr++;

							if (ctr_it->raw_db_h != NULL)
							{
								meterd_db_record(ctr_it->raw_db_h, ctr_it->table_name, p1_ctr_it->value, p1_ctr_it->unit, db_ts);
								DEBUG_MSG("Recorded %Lf %s for %s as raw value", p1_ctr_it->value, p1_ctr_it->unit, ctr_it->id);
							}

							if ((ctr_it->fivemin_db_h != NULL) && ((now - ctr_it->fivemin_ts) >= 300))
							{
								ctr_it->fivemin_cumul /= (long double) ctr_it->fivemin_ctr;

								meterd_db_record(ctr_it->fivemin_db_h, ctr_it->table_name, ctr_it->fivemin_cumul, p1_ctr_it->unit, db_ts);
								DEBUG_MSG("Recorded %Lf %s for %s as 5 minute average", ctr_it->fivemin_cumul, p1_ctr_it->unit, ctr_it->id);

								ctr_it->fivemin_cumul 	= 0.0f;
								ctr_it->fivemin_ctr 	= 0;
								ctr_it->fivemin_ts	= now;
							}

							if ((ctr_it->hourly_db_h != NULL) && ((now - ctr_it->hourly_ts) >= 3600))
							{
								ctr_it->hourly_cumul /= (long double) ctr_it->hourly_ctr;

								meterd_db_record(ctr_it->hourly_db_h, ctr_it->table_name, ctr_it->hourly_cumul, p1_ctr_it->unit, db_ts);
								DEBUG_MSG("Recorded %Lf %s for %s as hourly average", ctr_it->hourly_cumul, p1_ctr_it->unit, ctr_it->id);

								ctr_it->hourly_cumul 	= 0.0f;
								ctr_it->hourly_ctr 	= 0;
								ctr_it->hourly_ts	= now;
							}
						}
						else
						{
							if ((ctr_it->cumul_db_h != NULL) && ((now - ctr_it->cumul_rec_ts) >= total_interval))
							{
								meterd_db_record(ctr_it->cumul_db_h, ctr_it->table_name, p1_ctr_it->value, p1_ctr_it->unit, db_ts);
								ctr_it->cumul_rec_ts = now;
								DEBUG_MSG("Recorded %Lf %s for %s as cumulative value", p1_ctr_it->value, p1_ctr_it->unit, ctr_it->id);
							}
						}
					}
				}
			}
		}

		meterd_comm_telegram_free(p1);
		p1 = NULL;

		meterd_p1_counters_free(p1_counters);
		p1_counters = NULL;
		p1_ctr_it = NULL;
	}
}

/* Stop measuring */
void meterd_measure_interrupt(void)
{
	INFO_MSG("Cancelling measurement");

	run_measurement = 0;
}

/* Uninitialise measuring */
meterd_rv meterd_measure_finalize(void)
{
	INFO_MSG("Finalizing measurements");

	/* Uninitialise communications */
	meterd_comm_finalize();

	/* Free counter specifications */
	meterd_conf_free_counter_specs(counters);
	counters = NULL;

	/* Close database connections */
	meterd_db_close(raw_db_h);
	meterd_db_close(fivemin_db_h);
	meterd_db_close(hourly_db_h);
	meterd_db_close(cumul_db_h);

	return MRV_OK;
}

