/* util.h - Interface to misc. useful functions related to IRC
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-2024, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_UTIL_H
#define LIBSRSIRC_IRC_UTIL_H 1


#include <stdbool.h>
#include <stddef.h>

#include <libsrsirc/defs.h>


/* Overview:
 *
 * void lsi_ut_ident2nick(char *dest, size_t dest_sz, const char *ident);
 * void lsi_ut_ident2uname(char *dest, size_t dest_sz, const char *ident);
 * void lsi_ut_ident2host(char *dest, size_t dest_sz, const char *ident);
 *
 * int lsi_ut_istrcmp(const char *n1, const char *n2, int casemap);
 * int lsi_ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap);
 * char lsi_ut_tolower(char c, int casemap);
 * void lsi_ut_strtolower(char *dest, size_t destsz, const char *str, int cmap);
 *
 * void lsi_ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
 *     bool *ssl, const char *hostspec);
 * bool lsi_ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
 *     uint16_t *port, const char *pxspec);
 *
 * char *lsi_ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);
 * void lsi_ut_dumpmsg(void *tag, tokarr *msg);
 *
 * bool lsi_ut_conread(tokarr *msg, void *tag);
 * void lsi_ut_mut_nick(char *nick, size_t nick_sz);
 *
 * char **ut_parse_005_cmodes(const char *const *arr, size_t argcnt,
 *     size_t *num, const char *modepfx005chr, const char *const *chmodes);
 *
 * tokarr *lsi_ut_clonearr(tokarr *arr);
 * void lsi_ut_freearr(tokarr *arr);
 */

/** @file
 * \defgroup utilif Utility function interface provided by util.h
 * \addtogroup utilif
 *  @{
 */

/** \brief Extract the nick from a `nick[!user][@host]-style` identity
 * \param dest   Buffer where the extracted nickname will be put in
 * \param dest_sz   Size of `dest` buffer (\0-termination is ensured)
 * \param ident   The `nick[!user][@host]-style` identity to deal with. */
void lsi_ut_ident2nick(char *dest, size_t dest_sz, const char *ident);

/** \brief does the same as lsi_ut_ident2nick, just for the `user` part
 *
 * If there is no user part, `dest` will point to an empty string */
void lsi_ut_ident2uname(char *dest, size_t dest_sz, const char *ident);

/** \brief does the same as lsi_ut_ident2nick, just for the `host` part
 *
 * If there is no host part, `dest` will point to an empty string */
void lsi_ut_ident2host(char *dest, size_t dest_sz, const char *ident);

/** \brief case-insensitively compare strings, taking case mapping into account.
 * \param n1, n2   The strings to be compared
 * \param casemap   CMAP_* constant (usually what irc_casemap() returns)
 * \return 0 if equal; -1 if `n2` comes after `n1` (lex.), 1 otherwise
 *
 * \sa irc_casemap() */
int lsi_ut_istrcmp(const char *n1, const char *n2, int casemap);

/** \brief like lsi_ut_istrcmp, but compare only a maximum of `len` chars */
int lsi_ut_istrncmp(const char *n1, const char *n2, size_t len, int casemap);

/** \brief casemap-aware translate a char to lowercase, if uppercase */
char lsi_ut_tolower(char c, int casemap);

/** \brief casemap-aware translate a string to lowercase
 * \param dest   buffer where the resulting lowercase string will be put in
 * \param destsz   size of `dest` (\0-termination is ensured)
 * \param str   input string to have its uppercase characters lowered
 * \param casemap   CMAP_* constant (usually what irc_casemap() returns) */
void lsi_ut_strtolower(char *dest, size_t destsz, const char *str, int casemap);

/** \brief parse a "host spec" (i.e. something "host:port"-ish)
 * \param hoststr   buffer where the host part will be put in
 * \param hoststr_sz   size of said buffer (\0-termination is ensured)
 * \param port   points to where the port will be put in (0 if none)
 * \param ssl   points to a bool which tells if SSL was wished for
 * \param hostspec   the host specifier to parse. format: host[:port]['/SSL']
 *             where `host` can be a hostname, an IPv4- or an IPv6-Address,
 *             port is a 16 bit unsigned integer and the '/SSL' is literal */
void lsi_ut_parse_hostspec(char *hoststr, size_t hoststr_sz, uint16_t *port,
    bool *ssl, const char *hostspec);

/** \brief parse a "proxy specifier".
 *
 * A proxy specifier is a (no-ssl) "host spec" (see lsi_ut_parse_hostspec()),
 * prefixed by either 'HTTP:', 'SOCKS4:' or 'SOCKS5:'.
 * \param hoststr   See lsi_ut_parse_hostspec()
 * \param hoststr_sz   See lsi_ut_parse_hostspec()
 * \param port   See lsi_ut_parse_hostspec()
 * \param ptype   The proxy type (as in one of the IRCPX_* constants
 * \param pxspec   The "proxy specifier" format: proxytype:hostspec
 *             where proxytype is one of 'HTTP', 'SOCKS5', 'SOCKS5', and
 *             hostspec is a host specifier as shown in lsi_ut_parse_hostspec()
 *             (with the constraint that /SSL isn't allowed (yet?))
 *         (The other parameters work as in lsi_ut_parse_hostspec())
 * \return true on success; false on invalid format or invalid proxy type */
bool lsi_ut_parse_pxspec(int *ptype, char *hoststr, size_t hoststr_sz,
    uint16_t *port, const char *pxspec);

/** \brief glue a tokenized message back together into a single line
 *
 * This is mostly useful for debugging; it does /not/ generate valid protocol
 *     lines.
 * \param dest   buffer where the resulting line string will be put in
 * \param dest_sz   Size of `dest` buffer (\0-termination is ensured)
 * \param tag   XXX why is this here? Provide NULL for this parameter...
 * \param msg   pointer to a tokarr containing the message fields that are to
 *              be glued back together.
 * \return `dest`, where the result is put in. */
char *lsi_ut_sndumpmsg(char *dest, size_t dest_sz, void *tag, tokarr *msg);

/** \brief Like lsi_ut_sndumpmsg(), but print the result to stderr */
void lsi_ut_dumpmsg(void *tag, tokarr *msg);

/** \brief dump-to-stderr conread handler (see irc_regcb_conread())
 * \return true
 * \sa fp_con_read
 */
bool lsi_ut_conread(tokarr *msg, void *tag);

/** \brief Default alternative nickname generator.
 *
 * \sa irc_regcb_mutnick(), fp_mut_nick */
void lsi_ut_mut_nick(char *nick, size_t nick_sz);

/** \brief Dissect a MODE message, correlating mode chars with their arguments.
 *
 * A typical MODE message looks like this:
 * \code
 * :irc.example.org MODE #channel +oln-kt NewOp 42 OldKey
 * \endcode
 * As one can see, there are 5 mode letters involved, but only 3 arguments
 * provided -- whether or not a mode takes an argument is determined by the
 * channel mode classes the IRC server advertised in its 005 message and hence
 * must be figured out at runtime.  To make life easier to the user, this
 * function is provided to take care of how the twists of dealing with MODE
 * by turning a MODE message into an array of strings, where each element
 * has only one mode char followed by its argument, if it had one.
 *
 * The MDOE message in the above example would be turned into an array like:
 * \code
 * { [0] = "+o NewOp",
 *   [1] = "+l 42"
 *   [2] = "+n",
 *   [3] = "-k OldKey",
 *   [4] = "-t" }
 * \endcode
 * (Assuming typical IRC servers that implement the conventional modes with
 * their conventional semantics.  Weird servers may do this differently (which
 * is the reason this function is provided in the first place))
 *
 * \param msg   tokarr (as populated by irc_read(), containing the MODE message
 *              in question.
 * \param num   The number of elements in the resulting array will be stored in the
 *        variable pointed to by this parameter.
 * \param is324   This function can also be used to dissect the 324 message, which has
 *          pretty much identical semantics.  Pass true here if your `msg`
 *          contains a 324 rather than a MODE
 *
 * \return   A pointer into a newly allocated array of char *, where each
 *           element points to one specific mode change as described above
 *
 * *NOTE*: The caller is (currently) responsible for free()ing all elements
 *         of the returned array, as well as the (pointer into) the array
 *         itself.
 *
 * *NOTE2*: There is probably little reason to use this function as of the
 *          addition of channel tracking, since that can be used to keep
 *          track of the mode changes for us.
 */
char **lsi_ut_parse_MODE(irc *ctx, tokarr *msg, size_t *num, bool is324);

/** \brief Determine class of a channel mode
 * \param c   The channel mode letter (b, n, etc) to classify
 * \return If `c` is a channel mode supported by the IRC server we're talking
              to, one of the CHANMODE_CLASS_A, CHANMODE_CLASS_B,
	      CHANMODE_CLASS_C, CHANMODE_CLASS_D constants is returned.
	      Otherwise, we return 0.
 */
int lsi_ut_classify_chanmode(irc *ctx, char c);

/** \brief deep-copy a tokarr (usually containing a tokenized irc msg)
 * \param arr   pointer to the tokarr to clone
 * \return A pointer to the cloned result */
tokarr *lsi_ut_clonearr(tokarr *arr);

/** \brief free a tokarr (as obtained by lsi_ut_clonearr())
 * \param arr   Pointer to a tokarr obtained from lsi_ut_clonearr() */
void lsi_ut_freearr(tokarr *arr);

/** \brief Reconstruct a valid protocol message from a tokarr
 *
 * There is probably little reason to use this.  It might come handy for
 * implementing bouncer-like software, though.
 *
 * \param dest   Buffer where the resulting line will be put in
 * \param destsz   Size of `dest` (\0-termination is ensured)
 * \param msg   Pointer to a tokarr (as obtained from irc_read() or
 *              lsi_ut_clonearr()
 * \param coltr   If the last field in `msg` does not contain spaces,
 *                this flag determines whether or not to prefix it with
 *                a colon anyway (this is normally done as a syntactic
 *                trick to allow space in the last argument)
 * \return `dest` */
char *lsi_ut_snrcmsg(char *dest, size_t destsz, tokarr *msg, bool coltr);

/** \brief In-place field-split an IRC protocol message and populate a tokarr
 *
 * This is the routine that field-splits the protocol messages we read from
 * the IRC server.  It is provided as a utility function because it might
 * be useful in its own right.
 *
 * \param buf   Pointer to a buffer that contains an IRC protocol message.
 *              The contents of this buffer are changed by this function
 *              (specifically, \\0 characters are inserted)
 * \param tok   Pointer to a tokarr that is to be populated with pointers to
 *              the identified fields in `buf`
 *
 * \sa tokarr
 */
bool lsi_ut_tokenize(char *buf, tokarr *tok);

/** \brief Determine the name of a given case mapping
 *
 * This is useful pretty much only for debugging.
 * \param cm   Casemapping constant (CMAP_*)
 * \return Name of the casemapping
 * \sa CMAP_ASCII, CMAP_RFC1459, CMAP_STRICT_RFC1459
 */
const char *lsi_ut_casemap_nam(int cm);

/** \brief Determine whether a given string is a channel
 *
 * This is a convenience function that tries to find the first character of the
 * given string in the 005 CHANTYPES entry, in order to guess whether it is a
 * channel name.
 *
 * \param str An arbitrary string that may or may not be a channel name
 * \return true if `str` starts with one of the channel types that were
 * advertised in the 005 ISUPPORT message, or with either '#' or '&' if no 005
 * message was seen.
 *
 * Note that this function returning true does NOT mean the given string is
 * necessarily a *valid* channel name (i.e. it might contain blanks or TABs,
 * both of which cannot appear in valid channel names). All we check is *just*
 * whether it begins with a character that is a known channel type.
 */
bool lsi_ut_ischan(irc *ctx, const char *str);

/** \brief Produce a SASL authentication (non-)string suitable for use with the
 * PLAIN mechanism
 *
 * Given a username and a password, a blob of the following format is written
 * into `dest`: "username\0username\0password"
 *
 * \param dest Buffer to hold the resulting auth blob
 * \param destsz Input-output parameter that should contain the size of
 *               `dest` when this function is called, and will upon returning
 *               reflect the length of the produced auth blob.
 * \param name Username to go into the result
 * \param pass Password to go into the result
 * \param base64 If true, base64 the result (as some IRC servers seem to prefer)
 *
 * \return true if everything went fine, false if we didn't have enough buffer
 *              space to store the result.
 */
bool lsi_ut_sasl_mkplauth(void *dest, size_t *destsz,
    const char *name, const char *pass, bool base64);

/** @} */

/* XXX */
char *lsi_ut_extract_tags(char *line, char **dest, size_t *ndest);

#endif /* LIBSRSIRC_IRC_UTIL_H */
