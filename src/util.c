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
str_remove_non_ascii (gchar * s)
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


void
str_replace_non_printable (gchar * s)
{
  gchar *ch, *chplus1;
  gint i;

  ch = g_utf8_offset_to_pointer (s, 0);

  for (i = 1; i < g_utf8_strlen (s, -1); i++)
    {
      chplus1 = g_utf8_offset_to_pointer (s, i);
      if (!g_unichar_isprint (g_utf8_get_char (ch)))
	memset (ch, ' ', chplus1 - ch);
      ch = chplus1;
    }
}


gchar *
str_beautify (const gchar * s, gint len, gboolean ascii)
{
  int i, skip = 0;
  gchar *newstr, last = '\0';
  // Remove/Replace all non-ascii/-printable characters
  if (ascii)
    {
      if (len > 0)
	newstr = g_strndup (s, len);
      else
	newstr = g_strdup (s);
      str_remove_non_ascii (newstr);
    }
  else
    {
      newstr = g_convert (s, len, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
      str_replace_non_printable (newstr);
    }
  // Remove leading and trailing spaces
  newstr = g_strstrip (newstr);
  // Replace multiple by single space ('s/[ ]*/ /')
  for (i = 0; i + skip < strlen (newstr); i++)
    {
      while (g_ascii_isspace (last) && g_ascii_isspace (newstr[i + skip]))
	skip++;
      if (skip > 0)
	newstr[i] = newstr[i + skip];
      last = newstr[i];
    }
  newstr[i] = '\0';
  if (newstr[0] == '\0')
    {
      g_free (newstr);
      return NULL;
    }
  return newstr;
}


gboolean
is_updated (const gchar * oldtext, gchar ** newtextptr, gboolean ascii)
{
  gboolean refresh = FALSE;

  // FIXME: This can probably be done easier
  if (oldtext != NULL)
    {
      if (*newtextptr != NULL)
	{
	  if (strcmp (*newtextptr, oldtext) != 0)
	    refresh = TRUE;
	}
      else
	refresh = TRUE;
    }
  else
    {
      if (*newtextptr != NULL)
	refresh = TRUE;
    }

  if (refresh)
    {
      g_free (*newtextptr);
      *newtextptr = str_beautify (oldtext, -1, ascii);
    }

  return refresh;
}


void
gtk_entry_printf (GtkWidget * w, const gchar * fmt, ...)
{
  gchar *msg;
  va_list args;

  va_start (args, fmt);
  msg = g_strdup_vprintf (fmt, args);
  va_end (args);
  gtk_entry_set_text (GTK_ENTRY (w), msg);
  g_free (msg);
}
