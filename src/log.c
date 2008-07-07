/* $Id$ */
/* Gives logging capabilities to audacious-dvb (via glib)

   This file is part of audacious-dvb.
   Copyright (C) 2007  Marius Konitzer
   
   Based on xmms-dvb:
   Copyright (C) 2003, 2004  Christian Motz
  
   audacious-dvb is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   audacious-dvb is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with audacious-dvb; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */

#include <glib.h>
#include <string.h>

#include "log.h"


typedef struct _HLOG
{
  gchar hl_pfx[256];
  gint hl_level;
} HLOG;


gint
log_open (gpointer * hlog, gchar * pfx, gint lvl)
{
  HLOG *hl;

  if (pfx == NULL)
    return RC_LOG_OPEN_PREFIX_NPE;

  if (&hlog == NULL)
    return RC_LOG_OPEN_HLOG_NPE;

  // Allocate memory for HLOG structure
  if ((hl = g_malloc0 (sizeof (HLOG))) == NULL)
    {
      *hlog = NULL;
      return RC_LOG_OPEN_MALLOC_FAILED;
    }

  // We have a maximum length for prefixes
  if (strlen (pfx) >= sizeof (hl->hl_pfx))
    {
      *hlog = NULL;
      return RC_LOG_OPEN_PREFIX_TOO_LONG;
    }

  // Fill in (default) values
  strcpy (hl->hl_pfx, pfx);
  hl->hl_level = lvl;
  *hlog = (void *) hl;

  return RC_OK;
}


gint
log_close (gpointer hlog)
{
  HLOG *hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_LOG_CLOSE_HANDLE_INVALID;

  // Free memory for HLOG structure
  g_free (hl);

  return RC_OK;
}


gint
log_print (gpointer hlog, gint lvl, const gchar * fmt, ...)
{
  HLOG *hl;
  gchar *msg;
  va_list args;

  hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_LOG_PRINT_HANDLE_INVALID;

  // Only print message that has at least current level
  if (hl->hl_level >= lvl)
    {
      va_start (args, fmt);
      msg = g_strdup_vprintf (fmt, args);
      va_end (args);
      g_message ("[%s] %s", hl->hl_pfx, msg);
      g_free (msg);
    }

  return RC_OK;
}

gint
log_set_level (gpointer hlog, gint lvl)
{
  HLOG *hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_LOG_SET_LVL_HANDLE_INVALID;

  // Update current with given level
  hl->hl_level = lvl;
  return RC_OK;
}
