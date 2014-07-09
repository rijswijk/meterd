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
 * P1 Telegram Parser
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <regex.h>
#include "utlist.h"
#include "meterd_log.h"
#include "meterd_error.h"
#include "p1_parser.h"

meterd_rv meterd_parse_p1_telegram(telegram_ll* telegram, const char* gas_id, smart_counter** counters)
{
	assert(telegram != NULL);
	assert(counters != NULL);

	const char*	re_simple	= "[0-9]-[0-9]:([0-9]+\\.[0-9]+\\.[0-9]+)[(](.*)[)]";
	const char*	re_gas		= "[(](.*)[)]";
	const char*	re_counterval	= "([0-9.]*)[*]([A-Za-z0-9]*)";
	regmatch_t	simple_m[3];
	regmatch_t	gas_m[2];
	regmatch_t	counterval_m[3];
	int		next_is_gas	= 0;
	regex_t		re_simple_c	= { 0 };
	regex_t		re_gas_c	= { 0 };
	regex_t		re_counterval_c	= { 0 };
	int		rv		= 0;
	telegram_ll*	telegram_it	= NULL;

	/* Set up regular expressions */
	if ((rv = regcomp(&re_simple_c, re_simple, REG_EXTENDED)) != 0)
	{
		DEBUG_MSG("Regex failed to compile: %d\n", rv);

		return MRV_GENERAL_ERROR;
	}

	if ((rv = regcomp(&re_gas_c, re_gas, REG_EXTENDED)) != 0)
	{
		DEBUG_MSG("Regex failed to compile: %d\n", rv);

		regfree(&re_simple_c);

		return MRV_GENERAL_ERROR;
	}

	if ((rv = regcomp(&re_counterval_c, re_counterval, REG_EXTENDED)) != 0)
	{
		DEBUG_MSG("Regex failed to compile: %d\n", rv);

		regfree(&re_simple_c);
		regfree(&re_gas_c);

		return MRV_GENERAL_ERROR;
	}

	/* Parse the telegram */
	LL_FOREACH(telegram, telegram_it)
	{
		if (next_is_gas)
		{
			next_is_gas = 0;

			if ((rv = regexec(&re_gas_c, telegram_it->t_line, 2, gas_m, 0)) == 0)
			{
				size_t		val_len		= gas_m[1].rm_eo - gas_m[1].rm_so;
				char		val_buf[256]	= { 0 };
				smart_counter*	new_counter	= NULL;

				if (val_len >= 256)
				{
					ERROR_MSG("Invalid gas counter data of length %zd", val_len);
				}
				else
				{
					/* Copy value */
					strncpy(val_buf, &telegram_it->t_line[gas_m[1].rm_so], val_len);

					/* Add new counter */
					new_counter = (smart_counter*) malloc(sizeof(smart_counter));

					if (new_counter == NULL)
					{
						regfree(&re_simple_c);
						regfree(&re_gas_c);
						regfree(&re_counterval_c);

						/* Return memory error */
						return MRV_MEMORY;
					}

					new_counter->id = strdup(gas_id);
					new_counter->unit = strdup(UNIT_M3);
					new_counter->value = strtold(val_buf, NULL);

					LL_APPEND((*counters), new_counter);
				}
			}
		}
		else if ((rv = regexec(&re_simple_c, telegram_it->t_line, 3, simple_m, 0)) == 0)
		{
			size_t 		id_len 		= simple_m[1].rm_eo - simple_m[1].rm_so;
			size_t 		val_len		= simple_m[2].rm_eo - simple_m[2].rm_so;
			smart_counter*	new_counter	= NULL;
			char*		new_id		= (char*) malloc((id_len + 1) * sizeof(char));
			char*		new_val		= NULL;

			if (new_id == NULL)
			{
				/* Return memory error */
				regfree(&re_simple_c);
				regfree(&re_gas_c);
				regfree(&re_counterval_c);

				return MRV_MEMORY;
			}

			/* First, copy the ID of the counter so we can check if this is a gas meter */
			memset(new_id, 0, id_len + 1);
			strncpy(new_id, &telegram_it->t_line[simple_m[1].rm_so], id_len);

			if ((gas_id != NULL) && (strlen(gas_id) == id_len) && (strcmp(gas_id, new_id) == 0))
			{
				/* This is a gas meter counter, the actual value is on the next line */
				next_is_gas = 1;

				free(new_id);
			}
			else
			{
				/* This is a regular counter */
				new_val = (char*) malloc((val_len + 1) * sizeof(char));

				if (new_val == NULL)
				{
					/* Return memory error */
					free(new_id);
					regfree(&re_simple_c);
					regfree(&re_gas_c);
					regfree(&re_counterval_c);

					return MRV_MEMORY;
				}

				memset(new_val, 0, val_len + 1);
				strncpy(new_val, &telegram_it->t_line[simple_m[2].rm_so], val_len);

				/* Match the value */
				if ((rv = regexec(&re_counterval_c, new_val, 3, counterval_m, 0)) == 0)
				{
					size_t	ctr_len		= counterval_m[1].rm_eo - counterval_m[1].rm_so;
					size_t	unit_len	= counterval_m[2].rm_eo - counterval_m[2].rm_so;
					char	ctr_buf[256]	= { 0 };
					char	unit_buf[256]	= { 0 };

					if ((ctr_len >= 256) || (unit_len >= 256))
					{
						ERROR_MSG("Invalid counter ID (%zd bytes) or unit (%zd bytes) length", ctr_len, unit_len);
					}
					else
					{
						/* Copy counter value and unit information */
						strncpy(ctr_buf, &new_val[counterval_m[1].rm_so], ctr_len);
						strncpy(unit_buf, &new_val[counterval_m[2].rm_so], unit_len);

						/* Add this new counter to the list */
						new_counter = (smart_counter*) malloc(sizeof(smart_counter));
	
						if (new_counter == NULL)
						{
							/* Return memory error! */
							regfree(&re_simple_c);
							regfree(&re_gas_c);
							regfree(&re_counterval_c);
							free(new_id);
							free(new_val);
	
							return MRV_MEMORY;
						}

						new_counter->unit 	= strdup(unit_buf);
						new_counter->id 	= new_id;
						new_counter->value 	= strtold(&ctr_buf[0], NULL);

						LL_APPEND((*counters), new_counter);
					}
				}
				else
				{
					free(new_id);
				}

				free(new_val);
			}
		}
	}

	/* Clean up regular expression parser */
	regfree(&re_simple_c);
	regfree(&re_gas_c);
	regfree(&re_counterval_c);

	return 0;
}

/* Free a linked list of counters*/
void meterd_p1_counters_free(smart_counter* counters)
{
	smart_counter*	ctr_it	= NULL;
	smart_counter*	ctr_tmp	= NULL;

	LL_FOREACH_SAFE(counters, ctr_it, ctr_tmp)
	{
		free(ctr_it->id);
		free(ctr_it->unit);
		free(ctr_it);
	}
}

