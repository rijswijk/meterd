/*
 * Copyright (c) 2015-2019 Roland van Rijswijk-Deij
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
#include <string.h>
#include "p1_parser.h"
#include "meterd_error.h"
#include "utlist.h"

int main(int argc, char* argv[])
{
	telegram_ll*	telegram	= NULL;
	telegram_ll*	tel_it		= NULL;
	telegram_ll*	tel_tmp		= NULL;
	smart_counter*	counters	= NULL;
	smart_counter*	ctr_it		= NULL;
	smart_counter*	ctr_tmp		= NULL;
	FILE*		in		= NULL;
	char		buf[4096]	= { 0 };

	if (argc != 2)
	{
		fprintf(stderr, "Please specify the filename of the test telegram on the command line\n");
		
		return -1;
	}

	in = fopen(argv[1], "r");

	if (in == NULL)
	{
		fprintf(stderr, "Failed to open '%s' for reading\n", argv[1]);

		return -1;
	}

	while (!feof(in))
	{
		if (fgets(buf, 4096, in) != NULL)
		{
			telegram_ll*	new_line	= (telegram_ll*) malloc(sizeof(telegram_ll));

			/* Strip CR/LF */
			while (strrchr(buf, '\r') != NULL) *strrchr(buf, '\r') = '\0';
			while (strrchr(buf, '\n') != NULL) *strrchr(buf, '\n') = '\0';

			new_line->t_line = strdup(buf);

			LL_APPEND(telegram, new_line);
		}
	}

	/* Parse the test telegram */
	if (meterd_parse_p1_telegram(telegram, "24.3.0", &counters) != MRV_OK)
	{
		fprintf(stderr, "Parsing of P1 telegram returned and error\n");
	}

	LL_FOREACH_SAFE(counters, ctr_it, ctr_tmp)
	{
		printf("id = %s, value = %0.5Lf, unit = %s\n", ctr_it->id, ctr_it->value, ctr_it->unit);

		free(ctr_it->id);
		free(ctr_it->unit);
		free(ctr_it);
	}

	LL_FOREACH_SAFE(telegram, tel_it, tel_tmp)
	{
		free(tel_it->t_line);
		free(tel_it);
	}

	return 0;
}

