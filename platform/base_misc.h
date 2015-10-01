/* base_misc.h -
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_BASE_MISC_H
#define LIBSRSIRC_BASE_MISC_H 1


#include <stdint.h>


void lsi_b_usleep(uint64_t us);

int lsi_b_getopt(int argc, char * const argv[], const char *optstring);
const char *lsi_b_optarg(void);
int lsi_b_optind(void);
void lsi_b_regsig(int sig, void (*sigfn)(int));


#endif /* LIBSRSIRC_BASE_MISC_H */
