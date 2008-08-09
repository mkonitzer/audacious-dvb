/* $Id$ */
/* Structures and methods for communication with DVB adapter

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

#ifndef __AUDACIOUS_DVB_DVB_H__
#define __AUDACIOUS_DVB_DVB_H__

#include <linux/dvb/frontend.h>
#include <linux/dvb/version.h>
#include <glib.h>

#undef DVB_ATSC
#if defined(DVB_API_VERSION_MINOR)
#if DVB_API_VERSION == 3 && DVB_API_VERSION_MINOR >= 1
#define DVB_ATSC 1
#endif
#endif

typedef struct _tunestruct
{
  gulong freq;
  gchar pol;
  gulong srate;
  guchar diseqc;
  guint slof;
  guint lof1;
  guint lof2;
  fe_modulation_t mod;
  fe_spectral_inversion_t sinv;
  fe_transmit_mode_t tmode;
  fe_bandwidth_t bandw;
  fe_guard_interval_t gival;
  fe_code_rate_t hpcr;
  fe_code_rate_t lpcr;
  fe_hierarchy_t hier;
  guint sid;
  guint apid;
  guint dpid;
} tunestruct;

typedef struct _dvbstatstruct
{
  guint str;
  guint snr;
  guint unc;
  guint ber;
  gboolean signal;
  gboolean carrier;
  gboolean viterbi;
  gboolean sync;
  gboolean lock;
  gboolean timedout;
  gboolean refresh;
} dvbstatstruct;

#define RC_DVB_ERROR                    2001
#define RC_DVB_TIMEOUT                  2002


gpointer *dvb_open (gint);
gint dvb_close (gpointer);
gint dvb_filter (gpointer, gint);
gint dvb_packet (gpointer, guchar *, gint);
gint dvb_unfilter (gpointer);
gint dvb_section (gpointer, gint, gint, gint, gint, guchar *, gint);
gint dvb_apid (gpointer, guint);
gint dvb_apkt (gpointer, guchar *, guint, guint, gint *);
gint dvb_dpid (gpointer, guint);
gint dvb_dpkt (gpointer, guchar *, gint, gint, gint *);
gint dvb_get_pid (gpointer, gint, guint *, guint *);

tunestruct *dvb_tune_init (void);
gint dvb_tune (gpointer, tunestruct *);
gint dvb_tune_parse_url (const gchar *, tunestruct *);
gchar *dvb_tune_to_text (gpointer, tunestruct *);
void dvb_tune_exit (tunestruct *);

dvbstatstruct *dvb_status_init (void);
gint dvb_get_status (gpointer, dvbstatstruct *);
void dvb_status_exit (dvbstatstruct *);

#endif // __AUDACIOUS_DVB_DVB_H__
