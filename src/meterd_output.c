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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include "db.h"
#include "utlist.h"

#define FORMAT_GNUPLOT		1
#define FORMAT_CSV		2

/* Accepted command-line arguments (normal) */
#define OPTSTRING "c:qapCs:S:d:o:i:r:xy:j:hv"

/* Accepted command-line arguments (long) */
struct option long_opts[] = {
	{ "help",    no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 }
};

void version(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n", VERSION);
	printf("Data series output tool\n");
	printf("Copyright (c) 2014 Roland van Rijswijk-Deij\n\n");
	printf("Use, modification and redistribution of this software is subject to the terms\n");
	printf("of the license agreement. This software is licensed under a 2-clause BSD-style\n");
	printf("license a copy of which is included as the file LICENSE in the distribution.\n");
}

void usage(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n", VERSION);
	printf("Data series output tool\n");
	printf("\n");
	printf("Usage:\n");
	printf("\tmeterd-output [-c <config>] [-q] [-a] [-p] [-C] [-s <id>] [-S <id>]\n");
	printf("\t              -d <database> [-o <file>] -i <interval> [-y <offset]\n");
	printf("\t              [-x] [-r <file>]\n");
	printf("\n");
	printf("Options:\n");
	printf("\t-c <config>   Use <config> as configuration file (default: %s)\n", DEFAULT_METERD_CONF);
	printf("\t-q            Be quiet; only logs errors (default: off)\n");
	printf("\t-a            Add data of selected counters and merge to a single column (default: off)\n");
	printf("\t-p            Output in GNUPlot compatible format\n");
	printf("\t-C            Output as CSV file\n");
	printf("\t-s <id>       Select counter with <id> (can occur multiple times)\n");
	printf("\t-S <id>       Select counter with <id> and invert (negate) its value\n");
	printf("\t              (can occur multiple times)\n");
	printf("\t-d <database> Read data from <database>\n");
	printf("\t-o <file>     Write output to <file> (default: stdout)\n");
	printf("\t-i <interval> Interval in seconds to output data for (relative to the\n");
	printf("\t              current time)\n");
	printf("\t-y <offset>   Output GNUPlot y-range statement based on counter values\n");
	printf("\t              in the output data (requires -r)\n");
	printf("\t-x            Output GNUPlot x-range statement based on timestamps\n");
	printf("\t              (requires -r)\n");
	printf("\t-r <file>     File to write GNUPlot range statements to\n");
	printf("\t-j <seconds>  Skip <seconds> between each query results\n");
	printf("\t-h            Print this help message\n");
	printf("\t-v            Print the version number\n");
	printf("\n");
}

void meterd_output(sel_counter* sel_counters, const char* dbname, const char* outfile, const int format, const int additive, const int interval, const char* range_file, const int give_y_range, const long double y_offset, const int give_x_range, int skip_time)
{
	db_res_ctr**	results		= NULL;
	db_res_ctr**	result_it	= NULL;
	db_res_ctr**	result_tmp	= NULL;
	int		ctr_count	= 0;
	int		i		= 0;
	int		iterating	= 1;
	sel_counter*	ctr_it		= 0;
	void*		db_handle	= NULL;
	int		select_from	= ((int) time(NULL)) - interval;
	FILE*		out		= stdout;
	long double	added		= 0.0f;
	long double	max_y		= -100000000.0f;
	long double	min_y		= 100000000.0f;
	int		min_x		= 0x7fffffff;
	int		max_x		= 0;

	if (outfile != NULL)
	{
		out = fopen(outfile, "w");

		if (out == NULL)
		{
			ERROR_MSG("Failed to open %s for writng", outfile);

			return;
		}
	}

	/* Initialise database handling */
	if (meterd_db_init() != MRV_OK)
	{
		ERROR_MSG("Failed to initialise database handling, giving up");

		if (outfile)
		{
			fclose(out);
			unlink(outfile);
		}

		return;
	}

	/* Open the database */
	if (meterd_db_open(dbname, 1, &db_handle) != MRV_OK)
	{
		ERROR_MSG("Failed to open database file %s", dbname);

		meterd_db_finalize();

		if (outfile)
		{
			fclose(out);
			unlink(outfile);
		}

		return;
	}

	/* Allocate space for results from the database */
	LL_COUNT(sel_counters, ctr_it, ctr_count);

	results 	= (db_res_ctr**) malloc(ctr_count * sizeof(db_res_ctr*));
	result_it	= (db_res_ctr**) malloc(ctr_count * sizeof(db_res_ctr*));
	result_tmp	= (db_res_ctr**) malloc(ctr_count * sizeof(db_res_ctr*));

	memset(results, 0, ctr_count * sizeof(db_res_ctr*));
	memset(result_it, 0, ctr_count * sizeof(db_res_ctr*));
	memset(result_tmp, 0, ctr_count * sizeof(db_res_ctr*));

	/* Retrieve results from the database */
	i = 0;

	LL_FOREACH(sel_counters, ctr_it)
	{
		if (meterd_db_get_results(db_handle, ctr_it->id, ctr_it->invert, &results[i++], select_from, skip_time) != MRV_OK)
		{
			ERROR_MSG("Failed to retrieve results for %s from database %s", ctr_it->id, dbname);

			if (outfile)
			{
				fclose(out);
				unlink(outfile);
			}
	
			return;
		}
	}

	/* Output the data */
	if (format == FORMAT_CSV)
	{
		/* Output the heading first */
		fprintf(out, "timestamp");

		if (additive)
		{
			fprintf(out, ",");
		}

		i = 0;

		LL_FOREACH(sel_counters, ctr_it)
		{
			if (additive)
			{
				fprintf(out, "%s%s", ctr_it->id, (i > 0) ? "+" : "");
			}
			else
			{
				fprintf(out, ",%s", ctr_it->id);
			}

			i++;
		}

		fprintf(out, "\n");

		/* Set iterators to start of list */
		for (i = 0; i < ctr_count; i++)
		{
			result_it[i] = results[i];
		}

		while(iterating)
		{
			added = 0.0f;

			/* Have we reached the end of one of the result lists? */
			for (i = 0; i < ctr_count; i++)
			{
				if (result_it[i] == NULL)
				{
					iterating = 0;
					break;
				}
			}

			if (!iterating) break;

			/* Print one result for each result list */
			for (i = 0; i < ctr_count; i++)
			{
				if (i == 0)
				{
					/* 
					 * Output the timestamp of the first result list;
					 * because of the way data is written to the database,
					 * timestamps are the same in all tables per row
					 */
					fprintf(out, "%d", result_it[i]->timestamp);

					if (result_it[i]->timestamp < min_x)
					{
						min_x = result_it[i]->timestamp;
					}
					else if (result_it[i]->timestamp > max_x)
					{
						max_x = result_it[i]->timestamp;
					}
				}

				if (!additive)
				{
					fprintf(out, ",%0.3Lf", result_it[i]->value);

					if (result_it[i]->value < min_y)
					{
						min_y = result_it[i]->value;
					}
					else if (result_it[i]->value > max_y)
					{
						max_y = result_it[i]->value;
					}
				}
				else
				{
					added += result_it[i]->value;
				}
			}

			if (additive)
			{
				fprintf(out, ",%0.3Lf", added);

				if (added < min_y)
				{
					min_y = added;
				}
				else if (added > max_y)
				{
					max_y = added;
				}
			}

			fprintf(out, "\n");

			/* Advance all lists one step and discard processed results */
			for (i = 0; i < ctr_count; i++)
			{
				result_tmp[i] = result_it[i];
				result_it[i] = result_it[i]->next;

				free(result_tmp[i]->unit);
				free(result_tmp[i]);
			}
		}
	}
	else if (format == FORMAT_GNUPLOT)
	{
		/* Set iterators to start of list */
		for (i = 0; i < ctr_count; i++)
		{
			result_it[i] = results[i];
		}

		while(iterating)
		{
			added = 0.0f;

			/* Have we reached the end of one of the result lists? */
			for (i = 0; i < ctr_count; i++)
			{
				if (result_it[i] == NULL)
				{
					iterating = 0;
					break;
				}
			}

			if (!iterating) break;

			/* Print one result for each result list */
			for (i = 0; i < ctr_count; i++)
			{
				if (i == 0)
				{
					/* 
					 * Output the timestamp of the first result list;
					 * because of the way data is written to the database,
					 * timestamps are the same in all tables per row
					 */
					fprintf(out, "%10d", result_it[i]->timestamp);

					if (result_it[i]->timestamp < min_x)
					{
						min_x = result_it[i]->timestamp;
					}
					else if (result_it[i]->timestamp > max_x)
					{
						max_x = result_it[i]->timestamp;
					}
				}

				if (!additive)
				{
					fprintf(out, "  %3.3Lf", result_it[i]->value);

					if (result_it[i]->value < min_y)
					{
						min_y = result_it[i]->value;
					}
					else if (result_it[i]->value > max_y)
					{
						max_y = result_it[i]->value;
					}
				}
				else
				{
					added += result_it[i]->value;
				}
			}

			if (additive)
			{
				fprintf(out, "  %3.3Lf", added);

				if (added < min_y)
				{
					min_y = added;
				}
				else if (added > max_y)
				{
					max_y = added;
				}
			}

			fprintf(out, "\n");

			/* Advance all lists one step and discard processed results */
			for (i = 0; i < ctr_count; i++)
			{
				result_tmp[i] = result_it[i];
				result_it[i] = result_it[i]->next;

				free(result_tmp[i]->unit);
				free(result_tmp[i]);
			}
		}
	}

	/* Clean up spurious results */
	for (i = 0; i < ctr_count; i++)
	{
		while(result_it[i] != NULL)
		{
			result_tmp[i] = result_it[i];
			result_it[i] = result_it[i]->next;

			free(result_tmp[i]->unit);
			free(result_tmp[i]);
		}
	}

	free(results);
	free(result_it);
	free(result_tmp);

	/* Close the database connection */
	meterd_db_close(db_handle);

	/* Uninitialise database handling */
	meterd_db_finalize();

	if (outfile)
	{
		fclose(out);
	}

	/* Write min/max values to file if requested */
	if (range_file != NULL)
	{
		FILE* range_fd = fopen(range_file, "w");

		if (range_fd == NULL)
		{
			ERROR_MSG("Failed to open %s for writing", range_file);

			return;
		}

		if (give_x_range) fprintf(range_fd, "set xrange [\"%d\":\"%d\"]\n", min_x, max_x);
		if (give_y_range) fprintf(range_fd, "set yrange [%3.3Lf:%3.3Lf]\n", min_y - y_offset, max_y + y_offset);
		fclose(range_fd);
	}
}

int main(int argc, char* argv[])
{
	char* 		config_path 	= NULL;
	int		quiet		= 0;
	int		additive	= 0;
	int		format_gnuplot	= 0;
	int		format_CSV	= 0;
	sel_counter*	sel_counters	= NULL;
	sel_counter*	new_ctr		= NULL;
	sel_counter*	sel_ctr_it	= NULL;
	sel_counter*	sel_ctr_tmp	= NULL;
	char*		dbname		= NULL;
	char*		outfile		= NULL;
	int		interval	= 0;
	int		give_y_range	= 0;
	long double	y_offset	= 0.0f;
	int		give_x_range	= 0;
	char*		range_file	= NULL;
	int		skip_time	= 0;
	int 		c 		= 0;
	
	while ((c = getopt_long(argc, argv, OPTSTRING, long_opts, NULL)) != -1)
	{
		switch(c)
		{
		case 'c':
			config_path = strdup(optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'a':
			additive = 1;
			break;
		case 'p':
			format_gnuplot = 1;
			break;
		case 'C':
			format_CSV = 1;
			break;
		case 's':
			new_ctr = (sel_counter*) malloc(sizeof(sel_counter));
			new_ctr->id = strdup(optarg);
			new_ctr->invert = 0.0f;
			LL_APPEND(sel_counters, new_ctr);
			break;
		case 'S':
			new_ctr = (sel_counter*) malloc(sizeof(sel_counter));
			new_ctr->id = strdup(optarg);
			new_ctr->invert = -1.0f;
			LL_APPEND(sel_counters, new_ctr);
			break;
		case 'd':
			dbname = strdup(optarg);
			break;
		case 'o':
			outfile = strdup(optarg);
			break;
		case 'i':
			interval = atoi(optarg);
			break;
		case 'y':
			give_y_range = 1;
			y_offset = strtold(optarg, NULL);
			break;
		case 'x':
			give_x_range = 1;
			break;
		case 'r':
			range_file = strdup(optarg);
			break;
		case 'j':
			skip_time = atoi(optarg);
			break;
		case 'h':
			usage();
			return 0;
		case 'v':
			version();
			return 0;
		default:
			usage();
			return 1;
		}
	}

	if (config_path == NULL)
	{
		config_path = strdup(DEFAULT_METERD_CONF);
	}

	/* Load the configuration */
	if (meterd_init_config_handling(config_path) != MRV_OK)
	{
		fprintf(stderr, "Failed to load the configuration, exiting\n");

		return MRV_CONFIG_ERROR;
	}

	/* Initialise logging */
	if (quiet)
	{
		if (meterd_init_log_at_level(METERD_LOG_ERROR) != MRV_OK)
		{
			fprintf(stderr, "Failed to initialise logging, exiting\n");

			return MRV_LOG_INIT_FAIL;
		}
	}
	else
	{
		if (meterd_init_log() != MRV_OK)
		{
			fprintf(stderr, "Failed to initialise logging, exiting\n");

			return MRV_LOG_INIT_FAIL;
		}
	}

	if (format_gnuplot && format_CSV)
	{
		ERROR_MSG("Cannot output in both GNUPlot as well as CSV format, bailing out");

		return MRV_PARAM_INVALID;
	}

	if (!format_gnuplot && !format_CSV)
	{
		ERROR_MSG("No output format selected, bailing out");

		return MRV_PARAM_INVALID;
	}

	if (interval <= 0)
	{
		ERROR_MSG("Invalid or no interval specified, bailing out");

		return MRV_PARAM_INVALID;
	}

	if (dbname == NULL)
	{
		ERROR_MSG("No database specified, bailing out");

		return MRV_PARAM_INVALID;
	}

	if ((give_x_range || give_y_range) && (range_file == NULL))
	{
		ERROR_MSG("Must specify -r in combination with -x and/or -y");

		return MRV_PARAM_INVALID;
	}

	INFO_MSG("Smart Meter Monitoring Daemon (meterd) version %s", VERSION);
	INFO_MSG("Processing data output request");

	/* Generate the requested output */
	meterd_output(sel_counters, dbname, outfile, format_gnuplot ? FORMAT_GNUPLOT : FORMAT_CSV, additive, interval, range_file, give_y_range, y_offset, give_x_range, skip_time);

	/* Uninitialise logging */
	if (meterd_uninit_log() != MRV_OK)
	{
		fprintf(stderr, "Failed to uninitialise logging\n");
	}

	free(config_path);
	free(dbname);
	free(outfile);
	free(range_file);

	LL_FOREACH_SAFE(sel_counters, sel_ctr_it, sel_ctr_tmp)
	{
		free(sel_ctr_it->id);
		free(sel_ctr_it);
	}

	INFO_MSG("Finished processing data output request");

	return MRV_OK;
}

