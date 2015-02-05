/* base_misc.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_MISC_H
#define LIBSRSIRC_BASE_MISC_H 1


#include <stdint.h>


void b_usleep(uint64_t us);

int b_getopt(int argc, char * const argv[], const char *optstring);
const char *b_optarg(void);
int b_optind(void);


#endif /* LIBSRSIRC_BASE_MISC_H */
