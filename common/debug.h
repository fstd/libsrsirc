/* debug.h - macros for logging/verbosity
 * libsrsirc - a lightweight serious IRC lib - (C) 2013, Timo Buhrmester
 * See README for contact-, COPYING for license information.  */

#ifndef SRS_IRC_DEBUG_H
#define SRS_IRC_DEBUG_H 1

#include <stdlib.h>
#include <err.h>

#define W_(FNC, THR, FMT, A...) do {                                  \
		const char *v = getenv("LIBSRSIRC_DEBUG");                    \
		if (!v || v[0] < THR) break;                                  \
		FNC("%s:%d:%s() - " FMT, __FILE__, __LINE__, __func__, ##A);  \
		} while(0)

#define W(FMT, A...) W_(warn, 1, FMT, ##A)
#define WV(FMT, A...) W_(warn, 2, FMT, ##A)

#define WX(FMT, A...) W_(warnx, 1, FMT, ##A)
#define WVX(FMT, A...) W_(warnx, 2, FMT, ##A)

#endif /* SRS_IRC_DEBUG_H */
