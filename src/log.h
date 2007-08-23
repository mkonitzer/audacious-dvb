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

#ifndef RC_OK
#define RC_OK                         0
#endif

#define RC_LOG_OPEN_MALLOC_FAILED     3000
#define RC_LOG_OPEN_PREFIX_TOO_LONG   3002
#define RC_LOG_CLOSE_HANDLE_INVALID   3100
#define RC_LOG_PRINT_HANDLE_INVALID   3200


#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7


gint log_open (gpointer *, gchar *, gint);
gint log_close (gpointer);
gint log_print (gpointer, gint, const gchar *, ...);
void log_set_level (gpointer hlog, gint lvl);

#endif // __AUDACIOUS_DVB_LOG_H__
