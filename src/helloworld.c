/* helloworld.c - trivial libsrsirc example. Join a chan, write a message
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-19, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include <libsrsirc/irc.h>


#define IRC_SERVER "irc.freenode.org"
#define IRC_PORT 6667
#define IRC_CHANNEL "#fstd"
#define IRC_NICK "libsrsirc"
#define IRC_MESSAGE "Hello world!"


int
main(void)
{
	/* Allocate and initialize new IRC context */
	irc *ihnd = irc_init();

	/* Set connection parameters */
	irc_set_server(ihnd, IRC_SERVER, IRC_PORT);
	irc_set_nick(ihnd, IRC_NICK);

	/* Connect and logon to IRC */
	if (!irc_connect(ihnd)) {
		fprintf(stderr, "could not connect/logon\n");
		exit(EXIT_FAILURE);
	}

	/* Join channel, send message */
	irc_printf(ihnd, "JOIN %s", IRC_CHANNEL);
	irc_printf(ihnd, "PRIVMSG %s :%s", IRC_CHANNEL, IRC_MESSAGE);

	for (;;) {
		tokarr msg; /* This is an array */

		/* Read a protocol message into `msg`, 500ms timeout */
		int r = irc_read(ihnd, &msg, 500000);

		/* Read failure, connection closed? */
		if (r < 0)
			break;

		/* Read timeout */
		if (r == 0)
			continue;

		/* r is >0, so we have read something.
		 * msg[0] points to the prefix, if any
		 * msg[1] points to the command
		 * msg[2+] point to the arguments, if any */

		/* This COULD also be done by registering dedicated
		 * handler functions for PING and PRIVMSG */
		if (strcmp(msg[1], "PING") == 0)
			irc_printf(ihnd, "PONG %s", msg[2]);
		else if (strcmp(msg[1], "PRIVMSG") == 0) {
			printf("Message from %s to %s: %s\n", msg[0], msg[2], msg[3]);
			/* If someone says "die", we die. */
			if (strcmp(msg[3], "die") == 0)
				break;
		}
	}

	/* Clean up */
	irc_dispose(ihnd);

	return EXIT_SUCCESS;
}
