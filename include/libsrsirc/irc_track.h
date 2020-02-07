/* irc_track.h - (optionally) track users and channels
 * libsrsirc - a lightweight serious IRC lib - (C) 2012-20, Timo Buhrmester
 * See README for contact-, COPYING for license information. */

#ifndef LIBSRSIRC_IRC_TRACK_H
#define LIBSRSIRC_IRC_TRACK_H 1


#include <libsrsirc/defs.h>

/** @file
 * \defgroup trackif Tracking interface provided by irc_track.h
 * \addtogroup trackif
 *  @{
 */

/** \brief Object representation of an IRC channel
 *
 * This struct represents a channel and the information we know about it
 */
struct chanrep {
	const char *name; /**< \brief Name of the channel */
	const char *topic; /**< \brief Topic of the channel, or NULL */
	const char *topicnick; /**< \brief Nick of who set the topic, or NULL */
	uint64_t tscreate; /**< \brief Channel creation time (ms since Epoch) */
	uint64_t tstopic; /**< \brief Topic timestamp (ms since Epoch) */
	void *tag; /**< \brief Opaque user data */
};

/** \brief Object representation of an IRC user
 *
 * This struct represents a user on IRC and the information we know about them
 */
struct userrep {
	/** \brief Mode prefix - only used when dealing with the user in the
	 * context of a channel membership, NULL otherwise.
	 *
	 * When this struct represents a user in the context of a channel
	 * membership, e.g. via irc_member(), modepfx will point to the user's
	 * mode prefix in the respective channel.
	 *
	 * When this struct represents a user outside of a channel context,
	 * modepfx will be NULL.
	 */
	const char *modepfx;
	const char *nick; /**< \brief Nickname of the user */
	const char *uname; /**< \brief Username, or NULL if unknown */ //XXX rly?
	const char *host; /**< \brief Hostname, or NULL if unknown */
	const char *fname; /**< \brief Full name, or NULL if unknown */
	size_t nchans; /**< \brief Number of channels we know the user is in */
	void *tag; /**< \brief Opaque user (as in, libsrsirc user) data */
};

/** \brief Convenience typedef for struct chanrep. Probably a bad idea. */
typedef struct chanrep chanrep;

/** \brief Convenience typedef for struct userrep. Probably a bad idea. */
typedef struct userrep userrep;


/* the results of these functions (i.e. the strings pointed to by the various
 * members of the structs above) are only valid until the next time irc_read
 * is called.  if you need persistence, copy the data. */

/** \brief Count the channels we are currently in
 * \return The number of channels we are currently in.  */
size_t irc_num_chans(irc *ctx);

/** \brief Retrieve representations of all channels we're currently in
 *
 * \param chanarr   Pointer into an array of at least `chanarr_cnt` elements.
 *                  The array is populated with the retrieved channel
 *                  representations.
 * \param chanarr_cnt   Maximum number of channels to retrieve
 * \return The number of channels that were put into `chanarr`
 *
 * *NOTE:* The information contained in the retrieved chanrep structures is
 *         only valid until the next call to irc_read() */
size_t irc_all_chans(irc *ctx, chanrep *chanarr, size_t chanarr_cnt);

/** \brief Retrieve one channel representation by channel name
 * \param dest   Pointer to a chanrep where we put the result
 * \param name   Name of the channel to retrieve
 * \return A representation of channel `name`, or NULL if we don't know it.
 *
 * *NOTE:* The information contained in the retrieved chanrep structure is
 *         only valid until the next call to irc_read() */
chanrep *irc_chan(irc *ctx, chanrep *dest, const char *name);

/** \brief Associate opaque user data with a channel
 *
 * This is a general-purpose mechanism to associate a piece of user-defined
 * information with a channel, called a "tag".  The tag is persistent and can
 * later be accessed using the `tag` member of struct chanrep.
 *
 * \param chnam   Name of the channel to tag
 * \param tag   The tag.  libsrsirc does not care what exactly this is.
 * \param autofree   If true, call free(3) on the tag when the channel is
 *                   deallocated (e.g. by PARTing it)
 *
 * \return True if the channel was tagged (false if we don't know that channel)
 */
bool irc_tag_chan(irc *ctx, const char *chnam, void *tag, bool autofree);

/** \brief Count the users we are currently seeing
 * \return The number of users we are currently seeing.  */
size_t irc_num_users(irc *ctx);

/** \brief Retrieve representations of all users we're currently seeing
 *
 * \param userarr   Pointer into an array of at least `userarr_cnt` elements.
 *                  The array is populated with the retrieved user
 *                  representations.
 * \param userarr_cnt   Maximum number of users to retrieve
 * \return The number of users that were put into `userarr`
 *
 * *NOTE:* The information contained in the retrieved userrep structures is
 *         only valid until the next call to irc_read() */
size_t irc_all_users(irc *ctx, userrep *userarr, size_t userarr_cnt);

/** \brief Retrieve one user representation by nickname
 * \param dest   Pointer to a userrep where we put the result
 * \param ident   Name of the user.  This can be just a nickname, or a
 *                nick!user\@host-style identity.
 * \return A representation of user `ident`, or NULL if we don't know them.
 *
 * *NOTE:* The information contained in the retrieved userrep structure is
 *         only valid until the next call to irc_read() */
userrep *irc_user(irc *ctx, userrep *dest, const char *ident);

/** \brief Associate opaque user data with a user
 *
 * This is a general-purpose mechanism to associate a piece of user-defined
 * information with a user, called a "tag".  The tag is persistent and can
 * later be accessed using the `tag` member of struct userrep.
 *
 * \param ident   Name of the user to tag.  This can be just a nickname, or a
 *                nick!user\@host-style identity.
 * \param tag   The tag.  libsrsirc does not care what exactly this is.
 * \param autofree   If true, call free(3) on the tag when the user is
 *                   deallocated (e.g. by them leaving our last common channel)
 *
 * \return True if the user was tagged (false if we don't know that user)
 */
bool irc_tag_user(irc *ctx, const char *ident, void *tag, bool autofree);


/** \brief Count number of users in a given channel
 * \param chnam   Name of the channel
 * \return The number of users in channel `chnam` */
size_t irc_num_members(irc *ctx, const char *chnam);

/** \brief Retrieve representations of all members of a channel
 *
 * \param chnam   Name of the channel
 * \param userarr   Pointer into an array of at least `userarr_cnt` elements.
 *                  The array is populated with the retrieved user
 *                  representations.
 * \param userarr_cnt   Maximum number of users to retrieve
 * \return The number of users that were put into `userarr`
 *
 * *NOTE:* The information contained in the retrieved userrep structures is
 *         only valid until the next call to irc_read() */
size_t irc_all_members(irc *ctx, const char *chnam, userrep *userarr,
    size_t userarr_cnt);


/** \brief Retrieve one member representation by nickname and channel
 * \param dest   Pointer to a userrep where we put the result
 * \param chnam   Name of the channel
 * \param ident   Name of the user.  This can be just a nickname, or a
 *                nick!user\@host-style identity.
 * \return A representation of user `ident` in channel `chnam`, or NULL if we
 * don't know the user, the channel, or the user isn't in that channel.
 *
 * *NOTE:* The information contained in the retrieved userrep structure is
 *         only valid until the next call to irc_read() */
userrep *irc_member(irc *ctx, userrep *dest, const char *chnam, const char *ident);

/* for debugging */

/** \brief Dump tracking state for debugging purposes
 *
 * This function is intended for debugging and will puke out a dump of all
 * tracking-related state associated with the given context
 *
 * \param full   Do a full dump
 */
void lsi_trk_dump(irc *ctx, bool full);

/** @} */

#endif /* LIBSRSIRC_IRC_TRACK_H */
