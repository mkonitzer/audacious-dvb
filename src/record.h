/* $Id$ */
/* Structures and methods to record to a file and split at given events

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

#ifndef __AUDACIOUS_DVB_RECORD_H__
#define __AUDACIOUS_DVB_RECORD_H__

#include <stdio.h>
#include <glib.h>

#define RECORD_INDEX_MAX    1000

typedef struct _recstruct
{
  FILE *file;
  gchar *fn_prefix;
  guint fn_idx;
  gchar *fn_suffix;
  gchar *filename;
} recstruct;

recstruct *record_init (void);
gboolean record_open (recstruct * rec, const gchar *, gboolean, gboolean);
gboolean record_next (recstruct *, gboolean, gboolean);
gint record_write (recstruct *, guchar *, size_t);
void record_close (recstruct *);
void record_exit (recstruct *);

#endif // __AUDACIOUS_DVB_RECORD_H__
