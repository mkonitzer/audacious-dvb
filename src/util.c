/* $Id$ */
/* Global helper functions

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
#include <stdio.h>
#include <string.h>
#include "util.h"
#include "log.h"

extern gpointer hlog;


void
clean_string (gchar * s)
{
  gint i, l;
  gchar *ws;

  if ((ws = g_malloc (1 + strlen (s))) != NULL)
    {
      l = 0;
      for (i = 0; i < strlen (s); i++)
	{
	  if ((s[i] & 0x7f) >= 32)
	    ws[l++] = s[i];
	  else
	    ws[l++] = ' ';
	}
      ws[l] = '\0';
      strcpy (s, ws);
      g_free (ws);
    }
}


gboolean
is_updated (gchar *oldtext, gchar **newtextptr)
{
  gboolean refresh = FALSE;
  if (*newtextptr != NULL)
    {
      if (strcmp (*newtextptr, oldtext) != 0)
	refresh = TRUE;
    }
  else
    refresh = TRUE;
  
  if (refresh)
    {
      g_free(*newtextptr);
      *newtextptr = g_strdup(oldtext);
    }
}
