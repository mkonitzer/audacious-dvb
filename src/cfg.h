/* $Id$ */
/* Methods for configuration file handling

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

#ifndef __AUDACIOUS_DVB_CFG_H__
#define __AUDACIOUS_DVB_CFG_H__

#include <glib.h>

typedef struct _cfgstruct
{
  gchar   *devpath;		/* DVB device path */
  gint     loglvl;		/* Log level */

  gboolean rec;			/* Activate recording of stream? */
  gchar   *rec_fname;		/* Filename for recording */
  gboolean rec_append;		/* Append to existing file? */
  
  gboolean isplit;		/* Activate interval-split? */
  gint 	   isplit_ival;		/* Length of interval */
  
  gboolean vsplit;		/* Activate volume-split? */
  gdouble  vsplit_vol;		/* Split when below XYZ dB ...  */
  gint     vsplit_dur;		/* ... for at least XYZ ms ... */
  gint     vsplit_minlen;	/* ... with minimum length of XYZ s. */
  
  gboolean info_epg;		/* Receive EPG info? */
  gboolean info_mmusic;		/* Receive MADMusic info? */
} cfgstruct;


void config_init(cfgstruct *);
gboolean config_from_db(cfgstruct *);
gboolean config_to_db(cfgstruct *);

#endif // __AUDACIOUS_DVB_CFG_H__
