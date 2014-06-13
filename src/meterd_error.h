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
 * Types
 */

#ifndef _METERD_ERROR_H
#define _METERD_ERROR_H

#include "config.h"

/* Function return values */

/* Success */
#define MRV_OK			0x00000000

/* Error messages */
#define MRV_GENERAL_ERROR	0x80000000	/* An undefined error occurred */
#define MRV_MEMORY		0x80000001	/* An error occurred while allocating memory */
#define MRV_PARAM_INVALID	0x80000002	/* Invalid parameter(s) provided for function call */
#define MRV_NO_CONFIG		0x80000003	/* No configuration file was specified */
#define MRV_CONFIG_ERROR	0x80000004	/* An error occurred while reading the configuration file */
#define MRV_LOG_INIT_FAIL	0x80000005	/* Failed to initialise logging */
#define MRV_CONFIG_NO_ARRAY	0x80000006	/* The requested configuration item is not an array */
#define MRV_CONFIG_NO_STRING	0x80000007	/* The requested configuration item is not a string */

#endif /* !_METERD_ERROR_H */

