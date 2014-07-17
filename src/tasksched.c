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
 * Task scheduler
 */

#include "config.h"
#include "tasksched.h"
#include "meterd_types.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include "meterd_error.h"
#include "utlist.h"
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Modules variables */
static pthread_t	tasksched_thread;
static int		tasksched_run		= 1;
static scheduled_task*	tasks			= NULL;

/* Initialise task scheduling */
meterd_rv meterd_tasksched_init(void)
{
	/* Load scheduled tasks */
	return meterd_conf_get_scheduled_tasks(&tasks);
}

/* Main thread procedure */
void* meterd_tasksched_threadproc(void* param)
{
	scheduled_task*	task_it	= NULL;
	time_t		now	= 0;
	size_t		i	= 0;

	INFO_MSG("Entering task scheduler thread");

	while(tasksched_run)
	{
		/* Iterate over all tasks and check if they need to be run */
		now = time(NULL);

		LL_FOREACH(tasks, task_it)
		{
			if ((now - task_it->last_executed) > task_it->interval)
			{
				DEBUG_MSG("Executing task '%s'", task_it->description);

				for (i = 0; i < task_it->num_cmds; i++)
				{
					DEBUG_MSG("Running '%s'", task_it->cmds[i]);

					if (system(task_it->cmds[i]) != 0)
					{
						ERROR_MSG("Execution of command '%s' return non-zero exit status", task_it->cmds[i]);
						break;
					}
				}

				task_it->last_executed = now;
			}
		}

		sleep(1);
	}

	INFO_MSG("Leaving task scheduler thread");

	return NULL;
}

/* Start the task scheduler thread */
void meterd_tasksched_start(void)
{
	scheduled_task*	task_it		= NULL;
	int		task_count	= 0;
	pthread_attr_t	task_t_attr;

	LL_COUNT(tasks, task_it, task_count);

	/* Only start the task scheduling thread if there are tasks configured */
	if (task_count > 0)
	{
		INFO_MSG("There are %d tasks scheduled, launching task scheduler thread", task_count);

		pthread_attr_init(&task_t_attr);
		pthread_attr_setdetachstate(&task_t_attr, PTHREAD_CREATE_JOINABLE);

		if (pthread_create(&tasksched_thread, &task_t_attr, meterd_tasksched_threadproc, NULL) != 0)
		{
			ERROR_MSG("Failed to start task scheduler thread");
		}
	}
	else
	{
		INFO_MSG("No tasks scheduled, skipping start of task scheduler thread");
		tasksched_run = 0;
	}
}

/* Stop the task scheduler thread */
void meterd_tasksched_stop(void)
{
	if (tasksched_run)
	{
		tasksched_run = 0;

		pthread_join(tasksched_thread, NULL);
	}
}

/* Uninitialise task scheduling */
meterd_rv meterd_tasksched_finalize(void)
{
	/* Clean up tasks */
	meterd_conf_free_scheduled_tasks(tasks);

	return MRV_OK;
}

