#ifndef SRC_ICAT_MISC_H
#define SRC_ICAT_MISC_H 1

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define STRACAT(DSTARR, STR) strNcat((DSTARR), (STR), sizeof (DSTARR))
#define STRACPY(DSTARR, STR) strNcpy((DSTARR), (STR), sizeof (DSTARR))

void strNcat(char *dest, const char *src, size_t destsz);
void strNcpy(char *dest, const char *src, size_t destsz);
uint8_t strtou8(const char *nptr, char **endptr, int base);
uint64_t strtou64(const char *nptr, char **endptr, int base);

bool ismychan(const char *chan);
bool isdigitstr(const char *str);

void sleep_us(uint64_t us);
uint64_t timestamp_us(void);
#endif /* SRC_ICAT_MISC_H */

