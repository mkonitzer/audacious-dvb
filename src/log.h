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

#ifndef __AUDACIOUS_DVB_LOG_H__
#define __AUDACIOUS_DVB_LOG_H__

#include <glib.h>

#define RC_LOG_ERROR		      3001

enum lvltype
{ LOG_EMERG = 0,
  LOG_ALERT = 1,
  LOG_CRIT = 2,
  LOG_ERR = 3,
  LOG_WARNING = 4,
  LOG_NOTICE = 5,
  LOG_INFO = 6,
  LOG_DEBUG = 7
};


gint log_glib_open (gpointer *, gchar *, enum lvltype);
gint log_file_open (gpointer *, gchar *, gboolean, enum lvltype);
gint log_close (gpointer);
gint log_print (gpointer, enum lvltype, const gchar *, ...);
gint log_set_level (gpointer hlog, enum lvltype);

#endif // __AUDACIOUS_DVB_LOG_H__
