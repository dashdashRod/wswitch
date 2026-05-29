/* src/mango_ipc.c - Mango tag awareness via mmsg IPC */
#define _POSIX_C_SOURCE 200809L

#include "mango_ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG(fmt, ...) fprintf(stderr, "[Mango] " fmt "\n", ##__VA_ARGS__)

/* Command used to enumerate clients. Kept as a macro so it is trivial to
 * point at a wrapper or an absolute path if mmsg is not on PATH. */
/* `mmsg -C` prints tag/monitor status lines AND, for each managed window, a
 * client line of the form:
 *
 *     <id>\t<appid>\t<title>\t<tagbuf>
 *
 * where tagbuf is a binary string with one digit per tag, e.g. "000000001"
 * = tag 1, "000000010" = tag 2, "000001001" = tags 1 and 4. We keep only the
 * client lines (those whose first tab-separated field is all digits) and
 * parse tagbuf as a base-2 bitmask. Status lines are space-separated and have
 * no tabs, so they're skipped automatically.
 *
 * The mmsg binary is found on PATH by default. Set the WSWITCH_MMSG
 * environment variable to override it with an absolute path (useful when mmsg
 * lives outside PATH, e.g. a local dev build). */
#ifndef MANGO_MMSG_DEFAULT
#define MANGO_MMSG_DEFAULT "mmsg"
#endif

typedef struct {
  char *appid;
  char *title;
  unsigned tagmask;
} MangoClient;

static MangoClient *clients = NULL;
static int client_count = 0;
static int client_cap = 0;

static void clear_table(void) {
  for (int i = 0; i < client_count; i++) {
    free(clients[i].appid);
    free(clients[i].title);
  }
  client_count = 0;
}

static int push_client(unsigned tagmask, const char *appid, const char *title) {
  if (client_count >= client_cap) {
    int newcap = client_cap == 0 ? 16 : client_cap * 2;
    MangoClient *p = realloc(clients, newcap * sizeof(MangoClient));
    if (!p)
      return -1;
    clients = p;
    client_cap = newcap;
  }
  clients[client_count].tagmask = tagmask;
  clients[client_count].appid = strdup(appid ? appid : "");
  clients[client_count].title = strdup(title ? title : "");
  client_count++;
  return 0;
}

int mango_refresh(void) {
  clear_table();

  /* Resolve the mmsg binary: $WSWITCH_MMSG wins, otherwise rely on PATH.
   * The value is the user's own configuration, not untrusted input. */
  const char *mmsg = getenv("WSWITCH_MMSG");
  if (!mmsg || !*mmsg)
    mmsg = MANGO_MMSG_DEFAULT;

  char cmd[1024];
  snprintf(cmd, sizeof(cmd), "%s -C 2>/dev/null", mmsg);

  FILE *fp = popen(cmd, "r");
  if (!fp) {
    /* No mmsg / not mango: silently behave as "no tag data". */
    return 0;
  }

  char *line = NULL;
  size_t cap = 0;
  ssize_t len;

  while ((len = getline(&line, &cap, fp)) != -1) {
    /* strip trailing newline */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len == 0)
      continue;

    /* Client line: "<id>\t<appid>\t<title>\t<tagbuf>".
     * Split on the three tabs; the first field must be a numeric id, which
     * also excludes the space-separated status lines. */
    char *f_id = line;

    char *p = strchr(line, '\t');
    if (!p)
      continue; /* no tabs -> status line, skip */
    *p = '\0';

    /* id field must be all digits */
    int numeric = (*f_id != '\0');
    for (char *q = f_id; *q; q++) {
      if (*q < '0' || *q > '9') {
        numeric = 0;
        break;
      }
    }
    if (!numeric)
      continue;

    char *f_appid = p + 1;
    p = strchr(f_appid, '\t');
    if (!p)
      continue;
    *p = '\0';

    char *f_title = p + 1;
    p = strchr(f_title, '\t');
    if (!p)
      continue; /* need the tagbuf field */
    *p = '\0';

    char *f_tag = p + 1; /* tagbuf: binary string, rest of line */

    unsigned tagmask = (unsigned)strtoul(f_tag, NULL, 2); /* base 2 */
    push_client(tagmask, f_appid, f_title);
  }

  free(line);
  pclose(fp);
  return client_count;
}

unsigned mango_tagmask_for(const char *appid, const char *title) {
  if (!appid)
    appid = "";
  if (!title)
    title = "";

  unsigned mask = 0;
  for (int i = 0; i < client_count; i++) {
    if (strcmp(clients[i].appid, appid) == 0 &&
        strcmp(clients[i].title, title) == 0) {
      mask |= clients[i].tagmask;
    }
  }
  return mask;
}

void mango_ipc_cleanup(void) {
  clear_table();
  free(clients);
  clients = NULL;
  client_cap = 0;
}
