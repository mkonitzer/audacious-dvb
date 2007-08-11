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

  if ((hl = malloc (sizeof (HLOG))) == NULL)
    {
      *hlog = NULL;
      return RC_LOG_OPEN_MALLOC_FAILED;
    }

  if (strlen (pfx) >= sizeof (hl->hl_pfx))
    {
      *hlog = NULL;
      return RC_LOG_OPEN_PREFIX_TOO_LONG;
    }
  strcpy (hl->hl_pfx, pfx);

  hl->hl_level = lvl;

  *hlog = (void *) hl;

  return RC_OK;
}


gint
log_close (gpointer hlog)
{
  HLOG *hl;

  if ((hl = (HLOG *) hlog) == NULL)
    return RC_LOG_CLOSE_HANDLE_INVALID;

  free (hl);

  return RC_OK;
}


gint
log_print (gpointer hlog, gint lvl, gchar * fmt, ...)
{
  HLOG *hl;
  va_list args;
  gchar msgbuf[256];

  hl = (HLOG *) hlog;

  if (hl == NULL)
    return RC_LOG_PRINT_HANDLE_INVALID;

  if (hl->hl_level >= lvl)
    {
      va_start (args, fmt);
      g_vsnprintf (msgbuf, 255, fmt, args);
      va_end (args);
      g_message ("[%s] %s", hl->hl_pfx, msgbuf);
    }

  return RC_OK;
}

void
log_set_level (gpointer hlog, gint lvl)
{
  HLOG *hl;
  hl = (HLOG *) hlog;
  hl->hl_level = lvl;
}
