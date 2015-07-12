/* util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-15, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_UTIL_H
#define LIBSRSIRC_IRC_UTIL_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>


/* Overview:
 *
 * void ut_pfx2nick(char *dest, size_t dest_sz, const char *pfx);
 * void ut_pfx2uname(char *dest, size_t dest_sz, const char *pfx);
 * void ut_pfx2host(char *dest, size_t dest_sz, const char *pfx);
 *
 * int ut_istrcmp(const char *n1, const char *n2, int casemap);
 * int ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap);
 * char ut_tolower(char c, int casemap);
 * void ut_strtolower(char *dest, size_t destsz, const char *str, int casemap);
 *
 * void ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
 *     bool *ssl, const char *hostspec);
 * bool ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
 *     uint16_t *port, const char *pxspec);
 *
 * char* ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);
 * void ut_dumpmsg(void *tag, tokarr *msg);
 *
 * bool ut_conread(tokarr *msg, void *tag);
 * void ut_mut_nick(char *nick, size_t nick_sz);
 *
 * char** ut_parse_005_cmodes(const char *const *arr, size_t argcnt,
 *     size_t *num, const char *modepfx005chr, const char *const *chmodes);
 *
 * tokarr *ut_clonearr(tokarr *arr);
 * void ut_freearr(tokarr *arr);
 */


/* ut_pfx2nick - Extracts the nick part from a `nick[!user][@host]-style' prefix
 * Params: `dest': Buffer where the extracted nickname will be put in
 *         `dest_sz': Size of `dest' buffer (\0-termination is ensured)
 *         `pfx': The `nick[!user][@host]-style' prefix to deal with. */
void ut_pfx2nick(char *dest, size_t dest_sz, const char *pfx);

/* ut_pfx2uname - does the same as ut_pfx2nick, just for the `user' part
 * If there is no user part, `dest' will point to an empty string */
void ut_pfx2uname(char *dest, size_t dest_sz, const char *pfx);

/* ut_pfx2host - does the same as ut_pfx2nick, just for the `host' part
 * If there is no host part, `dest' will point to an empty string */
void ut_pfx2host(char *dest, size_t dest_sz, const char *pfx);

/* ut_istrcmp - case-insensitively compare strings while taking IRC
 *     case mapping into account (e.g. "foo\[]bar" might be the lowercase
 *     equivalent of "FOO|{}BAR", depending on case mapping.
 * Params: `n1', `n2': The strings to be compared
 *         `casemap': CMAP_* constant (usually what irc_casemap() returns)
 * Returns 0 if equal; -1 if `n2' comes after `n1' (lex.), 1 otherwise */
int ut_istrcmp(const char *n1, const char *n2, int casemap);

/* ut_istrncmp - like ut_istrcmp, but compare a maximum of `len' chars */
int ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap);

/* ut_tolower - casemap-aware translate a char to lowercase, if uppercase */
char ut_tolower(char c, int casemap);

/* ut_strtolower - casemap-aware translate a string to lowercase
 * Params: `dest': buffer where the resulting lowercase string will be put in
 *         `destsz': size of `dest' (\0-termination is ensured)
 *         `str': input string to have its uppercase characters lowered
 *         `casemap': CMAP_* constant (usually what irc_casemap() returns) */
void ut_strtolower(char *dest, size_t destsz, const char *str, int casemap);

/* ut_strtolower_ip - like ut_strtolower, but do it in-place */
void ut_strtolower_ip(char *str, int casemap);

/* ut_parse_hostspec - parse a "host specifier" (i.e. something "host:port"-ish)
 * Params: `hoststr': buffer where the host part will be put in
 *         `hoststr_sz' : size of said buffer (\0-termination is ensured)
 *         `port': points to where the port will be put in (0 if none)
 *         `ssl': points to a bool which tells if SSL was wished for
 *         `hostspec': the host specifier to parse. format: host[:port]['/SSL']
 *             where `host' can be a hostname, an IPv4- or an IPv6-Address,
 *             port is a 16 bit unsigned integer and the '/SSL' is literal */
void ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *hostspec);

/* ut_parse_pxspec - parse a "proxy specifier", which is a (no-ssl) "host spec"
 *     (see above), prefixed by either 'HTTP:', 'SOCKS4:' or 'SOCKS5'.
 * Params: `ptype': The proxy type (as in one of the IRCPX_* constants
 *         `pxspec': The "proxy specifier" format: proxytype:hostspec
 *             where proxytype is one of 'HTTP', 'SOCKS5', 'SOCKS5', and
 *             hostspec is a host specifier as described in ut_parse_hostspec
 *             (with the constraint that /SSL isn't allowed (yet?))
 *         (The other parameters work as in ut_parse_hostspec)
 * Returns true on success; false on invalid format or invalid proxy type */
bool ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
    uint16_t *port, const char *pxspec);

/* ut_sndumpmsg - glue a tokenized message back together into a single line
 * This is mostly useful for debugging; it does /not/ generate valid protocol
 *     lines.  Returns `dest', where the result is put in.  Nevermind `tag'. */
char* ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);

/* ut_dumpmsg - glue a tokenized message back together and dump to stderr */
void ut_dumpmsg(void *tag, tokarr *msg);

/* ut_conread - dump-to-stderr conread handler (see irc_regcb_conread()) */
bool ut_conread(tokarr *msg, void *tag);

/* ut_mut_nick - Default "nick mutilation" function to generate new nicknames
 *     if, at logon time, the nickname we want is unavailable
 * Params: `nick':  when called, holds the nickname which was unavailable.
 *                  when returning, it should hold the new nickname to try
 *         `nick_sz': size of the nick buffer (honor it!) */
void ut_mut_nick(char *nick, size_t nick_sz);

/* ut_parse_MODE - oh well. if you need this documented, email me. */
char** ut_parse_MODE(irc h, tokarr *msg, size_t *num, bool is324);

/* ut_classify_chanmode - tell class (see 005 ISUPPORT spec) of a chanmode */
int ut_classify_chanmode(irc h, char c);

/* ut_clonearr - deep-copy a tokarr (usually containing a tokenized irc message)
 * Params: `arr': pointer to the tokarr to clone
 * Returns pointer to the cloned result */
tokarr *ut_clonearr(tokarr *arr);

/* ut_freearr - free a tokarr (which presumably was obtained by ut_clonearr) */
void ut_freearr(tokarr *arr);

char* ut_snrcmsg(char *dest, size_t destsz, tokarr *msg, bool coltr);

bool ut_tokenize(char *buf, tokarr *tok);

const char * ut_casemap_nam(int cm);


#endif /* LIBSRSIRC_IRC_UTIL_H */
