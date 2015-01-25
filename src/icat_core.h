/* icat_core.h - icat core, mainly links together icat_serv and icat_user
 * icat - IRC netcat on top of libsrsirc - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_ICAT_CORE_H
#define LIBSRSIRC_ICAT_CORE_H 1


void core_init(void);
void core_destroy(void);

int core_run(void);


#endif /* LIBSRSIRC_ICAT_CORE_H */
