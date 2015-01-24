#ifndef SRC_ICAT_USER_H
#define SRC_ICAT_USER_H 1

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include <sys/types.h>

bool user_canread(void);
size_t user_readline(char *dest, size_t destsz);
int user_printf(const char *fmt, ...);
bool user_eof(void);

#endif /* SRC_ICAT_USER_H */
