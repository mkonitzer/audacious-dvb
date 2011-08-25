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
#include <audacious/plugin.h>

#ifdef __AUDACIOUS_PLUGIN_API__
#define AUD_PLUGIN_API	__AUDACIOUS_PLUGIN_API__
#else
#ifdef _AUD_PLUGIN_VERSION
#define AUD_PLUGIN_API	_AUD_PLUGIN_VERSION
#else
#error "Unable to detect version of plugin API. Aborting."
#endif
#endif

#ifndef RC_OK
#define RC_OK                   1000
#endif // RC_OK
#ifndef RC_NPE
#define RC_NPE			1001
#endif // RC_NPE

typedef struct _statstruct
{
  gchar *svc_name;
  gchar *prov_name;
  gboolean refresh;
} statstruct;

enum dvb_strtype
{
  DVB_STRING_ASCII,
  DVB_STRING_RADIOTEXT,
  DVB_STRING_DVBSI
};

gchar *str_beautify (const gchar *, gint, enum dvb_strtype);
gchar *get_alt_logoname (const gchar *, const gchar *, const gchar *);
gboolean is_updated (const gchar *, gchar **, enum dvb_strtype);
void gtk_entry_printf (GtkWidget *, const gchar *, ...);
void gtk_entry_set_text_safe (GtkEntry *, const gchar *);

#endif // __AUDACIOUS_DVB_UTIL_H__
