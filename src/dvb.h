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
#include <linux/dvb/dmx.h>
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

typedef struct _HDVB
{
  gint dvb_num;
  gchar *dvb_fedn;
  gint dvb_fedh;
  gchar *dvb_dmxdn;
  gint dvb_dmxdh;
  gchar *dvb_audn;
  gint dvb_audh;
  gint dvb_admx;
  gint dvb_ddmx;
  struct dmx_pes_filter_params dvb_dmx;
  struct dvb_frontend_info dvb_fe_info;
} HDVB;

HDVB *dvb_open (gint);
gint dvb_close (HDVB *);
gint dvb_filter (HDVB *, gint);
gint dvb_packet (HDVB *, guchar *, gint);
gint dvb_unfilter (HDVB *);
gint dvb_section (HDVB *, gint, gint, gint, gint, guchar *, gint);
gint dvb_apid (HDVB *, guint);
gint dvb_apkt (HDVB *, guchar *, guint, guint, gint *);
gint dvb_dpid (HDVB *, guint);
gint dvb_dpkt (HDVB *, guchar *, gint, gint, gint *);
gint dvb_get_pid (HDVB *, gint, guint *, guint *);

tunestruct *dvb_tune_init (void);
gint dvb_tune (HDVB *, tunestruct *);
gint dvb_tune_parse_url (const gchar *, tunestruct *);
gchar *dvb_tune_to_text (const HDVB *, const tunestruct *);
void dvb_tune_exit (tunestruct *);

dvbstatstruct *dvb_status_init (void);
gint dvb_get_status (const HDVB *, dvbstatstruct *);
void dvb_status_exit (dvbstatstruct *);

#endif // __AUDACIOUS_DVB_DVB_H__
