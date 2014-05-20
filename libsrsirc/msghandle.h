/* msghandle.h - protocl message handler prototypes
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-14, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef MSGHANDLE_H
#define MSGHANDLE_H 1

#include <stdbool.h>
#include <stddef.h>

bool handle_001(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_002(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_003(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_004(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_PING(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_432(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_433(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_436(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_437(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_XXX(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_464(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_383(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_484(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_465(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_466(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_ERROR(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_NICK(ibhnd_t hnd, char **msg, size_t nargs);
bool handle_005(ibhnd_t hnd, char **msg, size_t nargs);

bool handle_005_CASEMAPPING(ibhnd_t hnd, const char *val);
bool handle_005_PREFIX(ibhnd_t hnd, const char *val);
bool handle_005_CHANMODES(ibhnd_t hnd, const char *val);

#endif /* MSGHANDLE_H */

