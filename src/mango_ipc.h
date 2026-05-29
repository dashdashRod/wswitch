/* src/mango_ipc.h - Mango tag awareness via mmsg IPC
 *
 * wswitch's foreign-toplevel data source carries no tag information.
 * This module shells out to `mmsg -C` (the v4 client-list IPC) to learn
 * which mango tag(s) each window lives on, so the switcher can scope its
 * MRU cycle to the current tag.
 *
 * Output format expected from `mmsg -C` (one client per line, tab-separated):
 *
 *     <id>\t<tagmask>\t<appid>\t<title>
 *
 * tagmask is a bitmask: bit 0 = tag 1, bit 1 = tag 2, ... It is parsed with
 * base 0, so both decimal (1, 2, 4) and hex (0x1) are accepted.
 *
 * Everything degrades gracefully: if mmsg is missing, errors, or returns
 * nothing (e.g. running under Sway), mango_refresh() yields an empty table
 * and every lookup returns 0, which callers treat as "tag unknown -> don't
 * filter". So tag scoping is purely additive and never breaks other setups.
 */
#ifndef MANGO_IPC_H
#define MANGO_IPC_H

/* Re-read the mango client list. Cheap; call once per switcher show.
 * Returns the number of clients parsed (0 on any failure). */
int mango_refresh(void);

/* Tagmask for the window identified by (appid, title).
 * Returns 0 if no match (or if no mango data is available). When several
 * clients share the same (appid, title) the OR of their tagmasks is returned,
 * so a duplicate-titled window still passes a same-tag test if any twin is on
 * that tag. */
unsigned mango_tagmask_for(const char *appid, const char *title);

/* Free the internal table (optional; called at daemon shutdown). */
void mango_ipc_cleanup(void);

#endif /* MANGO_IPC_H */
