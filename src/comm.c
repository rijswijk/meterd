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
 * Serial communication
 */

#include "config.h"
#include "meterd_types.h"
#include "meterd_error.h"
#include "meterd_config.h"
#include "meterd_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include "utlist.h"

/* Module variables*/
static int	comm_fd	= 0;

/* Initialise communication */
meterd_rv meterd_comm_init(void)
{
	char*		tty		= NULL;
	int		line_speed_cfg	= 0;
	int		bits_cfg	= 0;
	char*		parity_cfg	= NULL;
	int		rts_cts_cfg	= 0;
	int		xon_xoff_cfg	= 0;
	struct termios	tsettings;

	memset(&tsettings, 0, sizeof(struct termios));

	/* Retrieve configuration */
	if ((meterd_conf_get_string("meter", "port", &tty, NULL) != MRV_OK) || (tty == NULL))
	{
		ERROR_MSG("No serial port specified, please fix the configuration");

		return MRV_CONFIG_ERROR;
	}

	if (meterd_conf_get_int("meter", "speed", &line_speed_cfg, 9600) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve line speed from the configuration");
	}

	switch(line_speed_cfg)
	{
	case 50:	tsettings.c_cflag |= B50;	break;
	case 75:	tsettings.c_cflag |= B75;	break;
	case 110:	tsettings.c_cflag |= B110;	break;
	case 134:	tsettings.c_cflag |= B134;	break;
	case 150:	tsettings.c_cflag |= B150;	break;
	case 200:	tsettings.c_cflag |= B200;	break;
	case 300:	tsettings.c_cflag |= B300;	break;
	case 600:	tsettings.c_cflag |= B600;	break;
	case 1200:	tsettings.c_cflag |= B1200;	break;
	case 2400:	tsettings.c_cflag |= B2400;	break;
	case 4800:	tsettings.c_cflag |= B4800;	break;
	case 9600:	tsettings.c_cflag |= B9600;	break;
	case 19200:	tsettings.c_cflag |= B19200;	break;
	case 38400:	tsettings.c_cflag |= B38400;	break;
	case 57600:	tsettings.c_cflag |= B57600;	break;
	case 115200:	tsettings.c_cflag |= B115200;	break;
	case 230400:	tsettings.c_cflag |= B230400;	break;
	default:
		ERROR_MSG("Unsupported line speed %d baud", line_speed_cfg);
		free(tty);
		return MRV_CONFIG_ERROR;
	}

	if (meterd_conf_get_int("meter", "bits", &bits_cfg, 7) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve serial line #bits from the configuration");
	}

	switch(bits_cfg)
	{
	case 5:		tsettings.c_cflag |= CS5;	break;
	case 6:		tsettings.c_cflag |= CS6;	break;
	case 7:		tsettings.c_cflag |= CS7;	break;
	case 8:		tsettings.c_cflag |= CS8;	break;
	default:
		ERROR_MSG("Unsupported #serial bits %d", bits_cfg);
		free(tty);
		return MRV_CONFIG_ERROR;
	}

	if (meterd_conf_get_string("meter", "parity", &parity_cfg, NULL) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve parity setting from the configuration");
	}

	if ((parity_cfg == NULL) || !strcasecmp(parity_cfg, "none"))
	{
		/* Do nothing */
	}
	else if (!strcasecmp(parity_cfg, "even"))
	{
		tsettings.c_cflag |= PARENB;
	}
	else if (!strcasecmp(parity_cfg, "odd"))
	{
		tsettings.c_cflag |= PARENB | PARODD;
	}
	else
	{
		ERROR_MSG("Invalid parity setting %s, valid values are: none, even, odd", parity_cfg);

		free(tty);
		free(parity_cfg);

		return MRV_CONFIG_ERROR;
	}

	free(parity_cfg);

	if (meterd_conf_get_bool("meter", "rts_cts", &rts_cts_cfg, 0) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve RTS/CTS (hardware flow control) configuration");
	}

	if (rts_cts_cfg)
	{
		tsettings.c_cflag |= CRTSCTS;
	}

	if (meterd_conf_get_bool("meter", "xon_xoff", &xon_xoff_cfg, 0) != MRV_OK)
	{
		WARNING_MSG("Failed to retrieve XON/XOFF (software flow control) configuration");
	}

	if (xon_xoff_cfg)
	{
		tsettings.c_iflag |= IXON | IXOFF;
	}

	tsettings.c_cflag |= CLOCAL | CREAD;
	tsettings.c_lflag = ICANON;

	/* Open the serial terminal */
	comm_fd = open(tty, O_RDWR | O_NOCTTY);

	if (comm_fd < 0)
	{
		ERROR_MSG("Failed to open serial terminal %s: %s", tty, strerror(errno));

		free(tty);

		return MRV_COMM_ERROR;
	}

	INFO_MSG("Connected to serial terminal %s", tty);
	free(tty);

	/* Set the terminal configuration */
	tcflush(comm_fd, TCIFLUSH);

	if (tcsetattr(comm_fd, TCSANOW, &tsettings) != 0)
	{
		ERROR_MSG("Failed to change terminal settings");

		close(comm_fd);

		return MRV_COMM_ERROR;
	}

	return MRV_OK;
}

/* Free space held by the telegram */
void meterd_comm_telegram_free(telegram_ll* telegram)
{
	telegram_ll*	tel_it	= NULL;
	telegram_ll*	tel_tmp	= NULL;

	LL_FOREACH_SAFE(telegram, tel_it, tel_tmp)
	{
		free(tel_it->t_line);
		free(tel_it);
	}
}

/* Wait for a new P1 telegram */
meterd_rv meterd_comm_recv_p1(telegram_ll** telegram)
{
	assert(telegram != NULL);

	char 	buf[4096] 	= { 0 };	/* Note: we assume telegram lines are smaller than 6Kbytes */
	int	res		= 0;

	/* Read data until we encounter a '/' character, that starts a telegram */
	do
	{
		res = read(comm_fd, buf, 4095);

		if (res <= 0)
		{
			if (errno == EINTR)
			{
				return MRV_COMM_INTR;
			}

			return MRV_COMM_ERROR;
		}

		buf[res] = '\0';
	}
	while(buf[0] != '/');

	/* Now, read the telegram, which ends with a '!' character */
	do
	{
		telegram_ll* new_tel_line	= (telegram_ll*) malloc(sizeof(telegram_ll));

		/* Remove \r and \n */
		if (strchr(buf, '\r') != NULL) *strchr(buf, '\r') = '\0';
		if (strchr(buf, '\n') != NULL) *strchr(buf, '\n') = '\0';

		new_tel_line->t_line = strdup(buf);

		LL_APPEND(*telegram, new_tel_line);

		res = read(comm_fd, buf, 4095);

		if (res <= 0)
		{
			meterd_comm_telegram_free(*telegram);

			if (errno == EINTR)
			{
				return MRV_COMM_INTR;
			}

			return MRV_COMM_ERROR;
		}

		buf[res] = '\0';
	}
	while(buf[0] != '!');

	return MRV_OK;
}

/* Uninitialise communication */
meterd_rv meterd_comm_finalize(void)
{
	close(comm_fd);

	INFO_MSG("Disconnected from serial terminal");

	return MRV_OK;
}

