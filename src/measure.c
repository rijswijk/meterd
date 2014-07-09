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
#include "utlist.h"

/* Module variables */
static void*		raw_db_h	= NULL;
static void*		fivemin_db_h	= NULL;
static void*		hourly_db_h	= NULL;
static void*		cumul_db_h	= NULL;
static counter_spec*	counters	= NULL;
static int		run_measurement	= 1;

/* Initialise measuring */
meterd_rv meterd_measure_init(void)
{
	char* 		raw_db_name	= NULL;
	char* 		fivemin_db_name	= NULL;
	char*		hourly_db_name	= NULL;
	char*		cumul_db_name	= NULL;
	meterd_rv	rv		= MRV_OK;

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

	return MRV_OK;
}

/* Run the main measurement loop until interrupted */
void meterd_measure_loop(void)
{
	meterd_rv	rv	= MRV_OK;
	telegram_ll*	p1	= NULL;
	telegram_ll*	tel_it	= NULL;

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

		/* FIXME: this is test code */
		LL_FOREACH(p1, tel_it)
		{
			DEBUG_MSG("%s", tel_it->t_line);
		}

		meterd_comm_telegram_free(p1);
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

