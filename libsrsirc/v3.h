/* v3.h - v3 protocol support internal interface
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-20, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_V3_H
#define LIBSRSIRC_V3_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>
#include "intdefs.h"

void lsi_v3_init_caps(irc *ctx);
void lsi_v3_reset_caps(irc *ctx);
bool lsi_v3_want_caps(irc *ctx);
bool lsi_v3_want_cap(irc *ctx, const char *cap, bool musthave);
void lsi_v3_clear_cap(irc *ctx, const char *cap);
void lsi_v3_update_caps(irc *ctx, const char *capsline, bool offered);
bool lsi_v3_check_caps(irc *ctx, bool offered);
bool lsi_v3_mk_capreq(irc *ctx, char *dest, size_t destsz);
void lsi_v3_update_cap(irc *ctx, const char *cap, const char *adddata,
    int offered, int enabled); //-1: don't upd

bool lsi_v3_regall(irc *ctx, bool dumb);
void lsi_v3_unregall(irc *ctx);

#endif /* LIBSRSIRC_V3_H */
