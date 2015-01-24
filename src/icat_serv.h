#ifndef SRC_ICAT_SERV_H
#define SRC_ICAT_SERV_H 1

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include <libsrsirc/defs.h>

void serv_init(void);
bool serv_canread(void);
int serv_read(tokarr *t);
int serv_printf(const char *fmt, ...);
bool serv_operate(void);
bool serv_online(void);
int serv_casemap(void);
uint64_t serv_sentquit(void);
void serv_destroy(void);

#endif /* SRC_ICAT_SERV_H */
