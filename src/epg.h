/* $Id$ */
/* Methods for retrieving EPG information over DVB

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

#ifndef __AUDACIOUS_DVB_EPG_H__
#define __AUDACIOUS_DVB_EPG_H__

#include <glib.h>

typedef struct _epgstruct
{
  gchar *lang;			//< component_descriptor.ISO_639_language_code
  gchar *stream_type;		//< component_descriptor.{stream_content,component_type}
  gchar *short_ev_name;		//< short_event_descriptor.event_name
  gchar *short_ev_text;		//< short_event_descriptor.text
  gchar *ext_ev_text;		//< extended_event_descriptor.text
  guint pil_mday;		//< PDC_descriptor.pil (day of month)
  guint pil_mon;		//< PDC_descriptor.pil (month)
  guint pil_hour;		//< PDC_descriptor.pil (hour)
  guint pil_min;		//< PDC_descriptor.pil (minute)
  gboolean refresh;
} epgstruct;

epgstruct *epg_init (void);
gint epg_read_data (epgstruct *, const guchar *, gint);
void epg_exit (epgstruct *);

#endif // __AUDACIOUS_DVB_EPG_H__
