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

#ifndef __AUDACIOUS_DVB_UTIL_H__
#define __AUDACIOUS_DVB_UTIL_H__

#include <glib.h>
#include <gtk/gtk.h>

typedef struct _statstruct
{
  gchar *svc_name;
  gchar *prov_name;
  gboolean refresh;
} statstruct;


void str_remove_non_ascii (gchar * s);
gchar *str_beautify (const gchar * s, gint len, gboolean ascii);
gboolean is_updated (const gchar * oldtext, gchar ** newtextptr,
		     gboolean ascii);
void gtk_entry_printf (GtkWidget *, const gchar *, ...);
inline void gtk_entry_set_text_safe (GtkEntry * entry, const gchar * text);

#endif // __AUDACIOUS_DVB_UTIL_H__
