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
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include "measure.h"
#include "tasksched.h"

/* Accepted command-line arguments (normal) */
#define OPTSTRING "fc:p:hv"

/* Accepted command-line arguments (long) */
struct option long_opts[] = {
	{ "help",    no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'v' },
	{ 0, 0, 0, 0 }
};

void version(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n", VERSION);
	printf("Copyright (c) 2014 Roland van Rijswijk-Deij\n\n");
	printf("Use, modification and redistribution of this software is subject to the terms\n");
	printf("of the license agreement. This software is licensed under a 2-clause BSD-style\n");
	printf("license a copy of which is included as the file LICENSE in the distribution.\n");
}

void usage(void)
{
	printf("Smart Meter Monitoring Daemon (meterd) version %s\n", VERSION);
	printf("\n");
	printf("Usage:\n");
	printf("\tmeterd [-c <config>] [-f] [-p <pidfile>]\n");
	printf("\n");
	printf("Options:\n");
	printf("\t-c <config>   Use <config> as configuration file (default: %s)\n", DEFAULT_METERD_CONF);
	printf("\t-f            Run in the foreground rather than forking as a daemon\n");
	printf("\t-p <pidfile>  Specify the PID file to write the daemon process ID to (default: %s)\n", DEFAULT_METERD_PIDFILE);
	printf("\t-h            Print this help message\n");
	printf("\t-v            Print the version number\n");
	printf("\n");
}

void write_pid(const char* pid_path, pid_t pid)
{
	FILE* pid_file = fopen(pid_path, "w");

	if (pid_file == NULL)
	{
		ERROR_MSG("Failed to write the pid file %s", pid_path);

		return;
	}

	fprintf(pid_file, "%d\n", pid);
	fclose(pid_file);
}

/* Signal handler for certain exit codes */
void signal_handler(int signum)
{
	switch(signum)
	{
	case SIGABRT:
		ERROR_MSG("Caught SIGABRT");
		break;
	case SIGBUS:
		ERROR_MSG("Caught SIGBUS");
		break;
	case SIGFPE:
		ERROR_MSG("Caught SIGFPE");
		break;
	case SIGILL:
		ERROR_MSG("Caught SIGILL");
		break;
	case SIGPIPE:
		ERROR_MSG("Caught SIGPIPE");
		break;
	case SIGQUIT:
		INFO_MSG("Caught SIGQUIT, exiting");

		/* Stop running measurement */
		meterd_measure_interrupt();

		break;
	case SIGTERM:
		INFO_MSG("Caught SIGTERM, exiting");

		/* Stop running measurement */
		meterd_measure_interrupt();

		break;
	case SIGINT:
		INFO_MSG("Caught SIGINT, exiting");

		/* Stop running measurement */
		meterd_measure_interrupt();

		break;
	case SIGSEGV:
		ERROR_MSG("Caught SIGSEGV");
		exit(-1);
		break;
	case SIGSYS:
		ERROR_MSG("Caught SIGSYS");
		break;
	case SIGXCPU:
		ERROR_MSG("Caught SIGXCPU");
		break;
	case SIGXFSZ:
		ERROR_MSG("Caught SIGXFSZ");
		break;
	default:
		ERROR_MSG("Caught unknown signal 0x%X", signum);
		break;
	}
}

int main(int argc, char* argv[])
{
	char* config_path = NULL;
	char* pid_path = NULL;
	int daemon = 1;
	int c = 0;
	int pid_path_set = 0;
	int daemon_set = 0;
	pid_t pid = 0;
	
	while ((c = getopt_long(argc, argv, OPTSTRING, long_opts, NULL)) != -1)
	{
		switch(c)
		{
		case 'f':
			daemon = 0;
			daemon_set = 1;
			break;
		case 'c':
			config_path = strdup(optarg);

			if (config_path == NULL)
			{
				fprintf(stderr, "Error allocating memory, exiting\n");
				return MRV_MEMORY;
			}

			break;
		case 'p':
			pid_path = strdup(optarg);

			if (pid_path == NULL)
			{
				fprintf(stderr, "Error allocating memory, exiting\n");
				return MRV_MEMORY;
			}
			
			pid_path_set = 1;
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

		if (config_path == NULL)
		{
			fprintf(stderr, "Error allocating memory, exiting\n");
			return MRV_MEMORY;
		}
	}

	if (pid_path == NULL)
	{
		pid_path = strdup(DEFAULT_METERD_PIDFILE);

		if (pid_path == NULL)
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

	/* Determine configuration settings that were not specified on the command line */
	if (!pid_path_set)
	{
		char* conf_pid_path = NULL;

		if (meterd_conf_get_string("daemon", "pidfile", &conf_pid_path, NULL) != MRV_OK)
		{
			ERROR_MSG("Failed to retrieve pidfile information from the configuration");
		}
		else
		{
			if (conf_pid_path != NULL)
			{
				free(pid_path);
				pid_path = conf_pid_path;
			}
		}
	}

	if (!daemon_set)
	{
		if (meterd_conf_get_bool("daemon", "fork", &daemon, 1) != MRV_OK)
		{
			ERROR_MSG("Failed to retrieve daemon information from the configuration");
		}
	}

	/* Now fork if that was requested */
	if (daemon)
	{
		pid = fork();

		if (pid != 0)
		{
			/* This is the parent process; write the PID file and exit */
			write_pid(pid_path, pid);

			/* Unload the configuration */
			if (meterd_uninit_config_handling() != MRV_OK)
			{
				ERROR_MSG("Failed to uninitialise configuration handling");
			}
		
			/* Uninitialise logging */
			if (meterd_uninit_log() != MRV_OK)
			{
				fprintf(stderr, "Failed to uninitialise logging\n");
			}
		
			free(pid_path);
			free(config_path);
		
			return MRV_OK;
		}
	}

	/* If we forked, this is the child */
	INFO_MSG("Starting the Smart Meter Monitoring Daemon (meterd) version %s", VERSION);
	INFO_MSG("meterd %sprocess ID is %d", daemon ? "daemon " : "", getpid());

	/* Install signal handlers */
	signal(SIGABRT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGPIPE, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGSYS, signal_handler);
	signal(SIGXCPU, signal_handler);
	signal(SIGXFSZ, signal_handler);

	/* Initialise the measurement loop */
	if (meterd_measure_init() != MRV_OK)
	{
		ERROR_MSG("Failed to initialise the measurement subsystem, giving up");
	}
	else
	{
		if (meterd_tasksched_init() != MRV_OK)
		{
			ERROR_MSG("Failed to intialise task scheduling, giving up");
		}
		else
		{
			/* Start task scheduler */
			meterd_tasksched_start();

			/* Run measurement loop */
			meterd_measure_loop();

			/* Stop task scheduler */
			meterd_tasksched_stop();

			/* Uninitialise task scheduling */
			if (meterd_tasksched_finalize() != MRV_OK)
			{
				WARNING_MSG("Failed to properly uninitialise the task scheduler");
			}

			/* Uninitialise the measurement loop */
			if (meterd_measure_finalize() != MRV_OK)
			{
				WARNING_MSG("Failed to properly uninitialise the measurement subsystem");
			}
		}
	}

	INFO_MSG("Stopping the Smart Meter Monitoring Daemon (meterd) version %s", VERSION);

	/* Unload the configuration */
	if (meterd_uninit_config_handling() != MRV_OK)
	{
		ERROR_MSG("Failed to uninitialise configuration handling");
	}

	/* Remove signal handlers */
	signal(SIGABRT, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGSYS, SIG_DFL);
	signal(SIGXCPU, SIG_DFL);
	signal(SIGXFSZ, SIG_DFL);

	/* Uninitialise logging */
	if (meterd_uninit_log() != MRV_OK)
	{
		fprintf(stderr, "Failed to uninitialise logging\n");
	}

	free(pid_path);
	free(config_path);

	return MRV_OK;
}

