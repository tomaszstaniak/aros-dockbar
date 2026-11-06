/* Dockbar widget requests: installing a widget package into a running
 * Dockbar. MIT licence, see LICENSE in the Dockbar repository.
 *
 * Dockbar 0.6.0 and later have a public port DOCKBAR_WIDGETS_PORT. Its
 * presence is the capability: older versions do not have it. Do not send
 * anything to the port named "Dockbar"; it only means "open Preferences".
 *
 * Fill a DockbarWidgetsMsg (mn_Length = sizeof, magic, version, command,
 * path and flags), PutMsg it to the port under Forbid() after FindPort(),
 * and wait for the reply. The reply comes from Dockbar's main loop, within
 * a second or two plus the time of the copy. */
#ifndef DOCKBAR_WIDGETS_H
#define DOCKBAR_WIDGETS_H
#include <exec/ports.h>

#define DOCKBAR_WIDGETS_PORT "Dockbar.Widgets"
#define DOCKBAR_WIDGETS_MAGIC 0x444B5752UL /* 'DKWR' */
#define DOCKBAR_WIDGETS_VERSION 1

/* command */
#define DOCKBAR_WIDGETS_QUERY 1   /* fill the out fields only */
#define DOCKBAR_WIDGETS_INSTALL 2 /* copy the package drawer named by path */

/* flags (in) */
#define DOCKBAR_WIDGETS_OFFER_ADD 1 /* after a new install, offer to put it on a dock */

/* result (out) */
#define DOCKBAR_WIDGETS_OK 0         /* query answered, or package installed */
#define DOCKBAR_WIDGETS_UPDATED 1    /* an installed package was replaced */
#define DOCKBAR_WIDGETS_BADREQUEST 10 /* wrong magic, version, length or command */
#define DOCKBAR_WIDGETS_REFUSED 11   /* not a package for this Dockbar; text says why */
#define DOCKBAR_WIDGETS_BUSY 12      /* the package is in use right now; try again */
#define DOCKBAR_WIDGETS_FAILED 13    /* copying failed; the old package is kept */

struct DockbarWidgetsMsg {
  struct Message msg;
  ULONG magic;
  UWORD version, command;
  CONST_STRPTR path;
  ULONG flags;
  LONG result;
  UWORD api_min, api_max;   /* widget API versions this Dockbar speaks */
  char dockbar_version[24]; /* "0.6.0" */
  char variant[24];         /* "x86_64-abiv11", "x86_64-v1", "aarch64-raspi-v1" */
  char root[256];           /* the widget drawer in use */
  char text[160];           /* package name, or the reason of a refusal */
};
#endif
