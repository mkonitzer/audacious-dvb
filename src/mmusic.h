/* $Id$ */
/* Read Mad Music info, a greek Nova subscription service (Hotbird 13° East)

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

#ifndef __AUDACIOUS_DVB_MMUSIC_H__
#define __AUDACIOUS_DVB_MMUSIC_H__

#include <glib.h>

typedef struct _mmstruct
{
  gchar *title;
  gchar *artist;
  gchar *album;
  gint trnum;
  gboolean refresh;
  /* Don't touch the following! */
  gint mad_len;
  time_t mad_time;
  guchar mad_buf[8192];
} mmstruct;

mmstruct *madmusic_init (void);
void madmusic_read_data (mmstruct*, const guchar *, gint);
void madmusic_exit (mmstruct*);

#endif // __AUDACIOUS_DVB_MMUSIC_H__
