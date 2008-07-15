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
  if (s == NULL)
    return;

  g_strcanon (s,
	      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,?'\"!@#$%^&*()-_=+;:<>/\\|}{[]`~",
	      ' ');
}


void
str_remove_dvbsi_control_codes (gchar * s)
{
  gint i, l = 0, len;
  gchar *ws;

  if (s == NULL)
    return;

  len = strlen (s);
  if ((ws = g_malloc (1 + len)) != NULL)
    {
      for (i = 0; i < len; ++i)
	{
	  // Remove one-/two-byte control-codes
	  // (see ETSI EN 300 468, Annex A.2)
	  gint ch1, ch2, ch3;
	  ch1 = s[i] & 0xff;
	  ch2 = (i + 1 < len ? s[i + 1] & 0xff : 0x00);
	  ch3 = (i + 2 < len ? s[i + 2] & 0xff : 0x00);
	  if (ch1 >= 0x80 && ch1 <= 0x85)	// reserved
	    ;
	  else if (ch1 == 0x86)	// emphasis on
	    ;
	  else if (ch1 == 0x87)	// emphasis off
	    ;
	  else if (ch1 >= 0x88 && ch1 <= 0x89)	// reserved
	    ;
	  else if (ch1 == 0x8a)	// CR/LF
	    ws[l++] = ' ';
	  else if (ch1 >= 0x8b && ch1 <= 0x9f)	// user defined
	    ;
	  else if (ch1 == 0xe0 && ch2 >= 0x80 && ch2 <= 0x9f)	// private use area of ISO/IEC 10646-1
	    {
	      if (ch3 >= 0x80 && ch3 <= 0x85)	// reserved
		;
	      else if (ch3 == 0x86)	// emphasis on
		;
	      else if (ch3 == 0x87)	// emphasis off
		;
	      else if (ch3 >= 0x88 && ch3 <= 0x89)	// reserved
		;
	      else if (ch3 == 0x8a)	// CR/LF
		ws[l++] = ' ';
	      else if (ch3 >= 0x8b && ch3 <= 0x9f)	// reserved
		;
	      ++i;
	    }
	  else
	    ws[l++] = s[i];
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

  // TODO: simplify!
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
str_beautify (const gchar * s, gint len, enum dvb_strtype type)
{
  GRegex *mwsp = NULL;
  gchar from_enc[20], *newstr, *tmp = NULL;

  if (len > 0)
    newstr = g_strndup (s, len);
  else
    newstr = g_strdup (s);

  // Different string sources need different string handling
  switch (type)
    {
      /*
       * Handle standard ISO_646 (ASCII) string
       */
    case DVB_STRING_ASCII:
      // Remove/Replace all non-ascii/-printable characters
      str_remove_non_ascii (newstr);
      break;

      /*
       * Handle Radiotext message string
       * (see IEC 62106:1999, Annex E, code table E.1)
       */
    case DVB_STRING_RADIOTEXT:
      /*
       * Normally Radiotext strings have a unique encoding defined
       * in the a.m. RDS specification. Unfortunately most radio
       * stations don't give a fuck and send ISO_8859-15 anyway. :-(
       */
      tmp = g_convert (newstr, -1, "UTF-8", "ISO_8859-15", NULL, NULL, NULL);
      g_free (newstr);
      newstr = tmp;
      tmp = NULL;
      break;

      /*
       * Handle DVB service information string
       * (see ETSI EN 300 468, Annex A.2)
       */
    case DVB_STRING_DVBSI:
      // Remove all control codes
      str_remove_dvbsi_control_codes (newstr);
      tmp = newstr;

      // First byte(s) of string may reveal used character table
      switch (tmp[0])
	{
	case 0x01:		// ISO/IEC 8859-5
	case 0x02:		// ISO/IEC 8859-6
	case 0x03:		// ISO/IEC 8859-7
	case 0x04:		// ISO/IEC 8859-8
	case 0x05:		// ISO/IEC 8859-9
	case 0x06:		// ISO/IEC 8859-10
	case 0x07:		// ISO/IEC 8859-11
	case 0x08:		// ISO/IEC 8859-12
	case 0x09:		// ISO/IEC 8859-13
	case 0x0a:		// ISO/IEC 8859-14
	case 0x0b:		// ISO/IEC 8859-15
	  g_snprintf (from_enc, sizeof (from_enc), "ISO_8859-%d", tmp[0] + 4);
	  break;
	case 0x10:		// ISO/IEC 8859
	  if (tmp[1] == 0 && tmp[2] >= 1 && tmp[2] <= 15)
	    g_snprintf (from_enc, sizeof (from_enc), "ISO_8859-%d", tmp[2]);
	  else
	    strcpy (from_enc, "ISO_6937");	// ISO-6937 (fallback)
	  break;
	case 0x11:		// ISO/IEC 10646-1
	  strcpy (from_enc, "UCS2");
	  break;
	case 0x12:		// KSC5601-1987
	  strcpy (from_enc, "EUC-KR");
	  break;
	case 0x13:		// GB-2312-1980
	  strcpy (from_enc, "GB2312");
	  break;
	case 0x14:		// Big5 subset of ISO/IEC 10646-1
	  strcpy (from_enc, "BIG5");
	  break;
	case 0x15:		// UTF-8 encoding of ISO/IEC 10646-1
	  strcpy (from_enc, "ISO-10646/UTF8");
	  break;
	default:		// ISO-6937 (fallback)
	  strcpy (from_enc, "ISO_6937");
	}

      // Perform conversion and validation
      if (strcmp (from_enc, "ISO_6937") != 0)
	{
	  log_print (hlog, LOG_DEBUG, "Detected character encoding %s",
		     from_enc);
	  newstr =
	    g_convert (tmp + 1, -1, "UTF-8", from_enc, NULL, NULL, NULL);
	}
      else
	{
	  log_print (hlog, LOG_DEBUG,
		     "Falling back to default character encoding %s",
		     from_enc);
	  newstr = g_convert (tmp, -1, "UTF-8", from_enc, NULL, NULL, NULL);
	}
      g_free (tmp);
      tmp = NULL;
      break;

    default:
      // Should never happen
      g_assert (FALSE);
    }

  // Is string a valid UTF-8 string?
  if (g_utf8_validate (newstr, -1, NULL))
    {
      // Remove non-printable characters
      str_replace_non_printable (newstr);
      // Remove leading and trailing spaces
      tmp = g_strstrip (newstr);
      // Replace multiple whitespaces by single space
      mwsp = g_regex_new ("\\s+", 0, 0, NULL);
      newstr = g_regex_replace_literal (mwsp, tmp, -1, 0, " ", 0, NULL);
      g_regex_unref (mwsp);
      g_free (tmp);
    }
  else
    strcpy (newstr, "");

  return newstr;
}


gboolean
is_updated (const gchar * newtext, gchar ** oldtextptr, enum dvb_strtype type)
{
  gboolean refresh = TRUE;

  if (newtext == NULL && *oldtextptr == NULL)
    return FALSE;

  if (newtext != NULL && *oldtextptr != NULL)
    refresh = (strcmp (*oldtextptr, newtext) != 0);

  if (refresh)
    {
      if (*oldtextptr)
	g_free (*oldtextptr);
      *oldtextptr = str_beautify (newtext, -1, type);
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

inline void
gtk_entry_set_text_safe (GtkEntry * entry, const gchar * text)
{
  gtk_entry_set_text (entry, (text ? text : ""));
}
