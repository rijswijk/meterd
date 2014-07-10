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
 * Types
 */

#ifndef _METERD_TYPES_H
#define _METERD_TYPES_H

#include "config.h"
#include <time.h>

#define FLAG_SET(flags, flag) ((flags & flag) == flag)

#define COUNTER_TYPE_RAW	0
#define COUNTER_TYPE_CONSUMED	1
#define COUNTER_TYPE_PRODUCED	2

#define TABLE_PREFIX_RAW	"RAW_"
#define TABLE_PREFIX_PRODUCED	"PRODUCED_"
#define TABLE_PREFIX_CONSUMED	"CONSUMED_"

/* Type for function return values */
typedef unsigned long meterd_rv;

/* Counter specifications */
typedef struct counter_spec
{
	char*			description;	/* Short text description of the counter */
	char*			id;		/* Identifier of the counter */
	char*			table_name;	/* The database table name for this counter */
	int			type;		/* Counter type */
	long double		last_val;	/* Last recorded value */
	time_t			last_ts;	/* Timestamp of last recorded value */

	/* The fields below are only used for raw counters */
	long double		fivemin_cumul;	/* Five minute average cumulative value */
	size_t			fivemin_ctr;	/* Number of values accumulated in the 5-min avg. cumulative value */
	time_t			fivemin_ts;	/* Timestamp of last 5-min average calculation */
	long double		hourly_cumul;	/* Hourly average cumulative value */
	size_t			hourly_ctr;	/* Number of values accumulated in the hourly avg. cumulative value */
	time_t			hourly_ts;	/* Timestamp of last hourly average calculation */

	/* The fields below are only used for cumulative consumption/production counters */
	time_t			cumul_rec_ts;	/* Timestamp of last recorded cumulative value */

	/* Database handles associated with this counter */
	void*			raw_db_h;	/* Database handle for raw counter values */
	void*			fivemin_db_h;	/* Database handle for 5-min average values */
	void*			hourly_db_h;	/* Database handle for hourly average values */
	void*			cumul_db_h;	/* Database handle for cumulative values */

	struct counter_spec*	next;
}
counter_spec;

/* Telegram linked list */
typedef struct telegram_ll
{
	char*			t_line;
	struct telegram_ll*	next;
}
telegram_ll;

/* Smart counter data from a telegram */
typedef struct smart_counter
{
	char*			id;
	long double		value;
	char*			unit;
	struct smart_counter*	next;
}
smart_counter;

#define UNIT_KW			"kW"
#define UNIT_KWH		"kWh"
#define UNIT_M3			"m3"

/* Counter selection for output */
typedef struct sel_counter
{
	char*			id;
	long double		invert;
	struct sel_counter*	next;
}
sel_counter;

/* Counter values from the database */
typedef struct db_res_ctr
{
	int			timestamp;
	long double		value;
	char*			unit;
	struct db_res_ctr*	next;
}
db_res_ctr;

#endif /* !_METERD_TYPES_H */

