/* $Id$ */
/* audacious-dvb -- Audacious DVB Input Plugin

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

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif

#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/configdb.h>

#include <lame/lame.h>

#include "gui.h"
#include "epg.h"
#include "dvb.h"
#include "log.h"
#include "config.h"


#define RC_PARSE_URL_TOO_LONG             1000
#define RC_PARSE_URL_NOT_DVB_URL          1001
#define RC_PARSE_URL_COLON_MISSING        1002
#define RC_PARSE_URL_ADAPTER_NOT_NUMERIC  1003
#define RC_PARSE_URL_SEPARATOR_MISSING    1004
#define RC_PARSE_URL_DLVRY_SYS_MISSING    1005
#define RC_PARSE_URL_SYMBOLRATE_MISSING   1006
#define RC_PARSE_URL_FEC_DIVIDEND_MISSING 1007
#define RC_PARSE_URL_FEC_DIVISOR_MISSING  1008
#define RC_PARSE_URL_FEC_IMPLAUSIBLE      1009
#define RC_PARSE_URL_FEC_NOT_RECOGNIZED   1010
#define RC_PARSE_URL_UNK_POLARIZATION     1011

#define RC_DVB_GET_PID_SID_NOT_IN_PAT     1100


#define PLUGIN_NAME "DVB Input Plugin"


typedef struct _SVC
{
  int svc_adapter;
  int svc_frequency;
  int svc_symbolrate;
  int svc_viterbi;
  char svc_polarisation;
  char svc_delivery[256];
  char svc_service[256];
} SVC;


void dvb_close_record (void);

static void dvb_init (void);
static int dvb_is_our_file (char *);
static void dvb_play (InputPlayback *);
static void dvb_stop (InputPlayback *);
static void dvb_pause (InputPlayback *, short);
static int dvb_gettime (InputPlayback *);
static void dvb_cleanup (void);

static int dvb_parse_url (char *, SVC *);
static void *dvb_feed (void *);
static void dvb_pes_pkt (InputPlayback *, unsigned char *, int, int);
static void dvb_payload (InputPlayback *, unsigned char *, int, int);
static void dvb_mpeg_frame (InputPlayback *, unsigned char *, int, int);
static int dvb_get_pid (int, int *, int *);
static void *dvb_get_name (void *);
static void *dvb_madmusic (void *);
static void dvb_parse_text (unsigned char *, int);
static void dvb_fixbillshit (unsigned char *);
static void dvb_xlt (unsigned char *);

int dvb_read_conf (char *, char *, char *, char *, int, char *);


/* Miscellaneous globals */
int playing;			/* This is also used in the GUI */
int si_update;			/* This is used in EPG retrieval */
void *hlog;			/* This is used everywhere :) */
void *hdvb;			/* EPG retrieval uses this */

/* Configuration file parameters */
int cf_rec_guard, cf_get_info, cf_rec_sildur, cf_rec_isplit;
int cf_record, cf_rec_append, cf_rec_asplit, cf_rec_stime;
int cf_get_epg, epg_running;
char cf_rec_file[MAXPATHLEN];
float cf_rec_sillvl;

extern char epg_desc[4096];

static int si_previous;
static int paused, audio, file_index, mad_len, trnum, frm_ctr;
static char erfn[MAXPATHLEN], album[256], artist[256], title[256];
static char service_name[MAXPATHLEN];
static FILE *rec_file;
static time_t t_start, isplit_last, mad_time;
static pthread_t pt, ptd, pte;
static unsigned char mad_buf[8192];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t svc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

static int sap;
static int sumarr[512];

static int brt[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
  0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0,
  0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0,
  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
};

static int sft[] = {
  22050, 24000, 16000, 0, 44100, 48000, 32000, 0
};

InputPlugin dvb_ip = {
  0,			/* [handle]         Filled in by Audacious */
  0,			/* [filename]       Filled in by Audacious */
  PLUGIN_NAME,		/* [description] */

  dvb_init,		/* [init]           Called when plugin is enabled */
  dvb_about,		/* [about]          Show the about box */
  dvb_configure,	/* [configure]      Show the configure box */

  dvb_is_our_file,	/* [is_our_file]    Check if file is for our plugin */
  0,			/* [scan_dir] */

  dvb_play,		/* [play_file]      Start Playback */
  dvb_stop,		/* [stop]           Stop Playback */
  dvb_pause,		/* [pause]          Pause playback */
  0,			/* [seek]           Seeking impossible for DVB-streams */

  0,			/* [set_eq]         Set equalizer */

  dvb_gettime,		/* [get_time]       Get current play time */

  0,			/* [get_volume] */
  0,			/* [set_volume] */

  dvb_cleanup,		/* [cleanup] */

  0,			/* [get_vis_type] */
  0,			/* [add_vis_pcm] */

  0,			/* [set_info]       Set player window info */
  0,			/* [set_info_text]  Set song title text */
  0,			/* [get_song_info]  Get song title text to show on Playlist */
  dvb_getinfo,		/* [file_info_box]  Show file info box */

  0			/* [OutputPlugin]   deprecated */
};


InputPlugin *
get_iplugin_info (void)
{
  return &dvb_ip;
}


static void
dvb_init (void)
{
  int rc, log_lvl, idb;
  char *s;
  ConfigDb *cfgdb;

  s = NULL;

  /* Plugin configuration parameters. */

  if ((cfgdb = bmp_cfg_db_open ()) != NULL)
    {
      if (!bmp_cfg_db_get_int (cfgdb, "DVB", "Loglevel", &log_lvl))
	log_lvl = 0;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "Record", &cf_record))
	cf_record = 0;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "Append", &cf_rec_append))
	cf_rec_append = 0;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "Autosplit", &cf_rec_asplit))
	cf_rec_asplit = 0;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "Split", &cf_rec_isplit))
	cf_rec_isplit = 0;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "EPG", &cf_get_epg))
	cf_get_epg = 1;

      if (!bmp_cfg_db_get_int (cfgdb, "DVB", "Duration", &cf_rec_sildur))
	cf_rec_sildur = 360;

      if (bmp_cfg_db_get_int (cfgdb, "DVB", "Level", &idb))
	{
	  cf_rec_sillvl = (float) idb;
	  cf_rec_sillvl /= 100;
	}
      else
	{
	  cf_rec_sillvl = -42.80;	/* This is a good start */
	}

      if (!bmp_cfg_db_get_int (cfgdb, "DVB", "Guard", &cf_rec_guard))
	cf_rec_guard = 15;

      if (!bmp_cfg_db_get_bool (cfgdb, "DVB", "Info", &cf_get_info))
	cf_get_info = 0;

      if (!bmp_cfg_db_get_int (cfgdb, "DVB", "Interval", &cf_rec_stime))
	cf_rec_stime = 0;

      if (bmp_cfg_db_get_string (cfgdb, "DVB", "File", &s))
	{
	  if (s != NULL)
	    strcpy (cf_rec_file, s);
	}
      else
	{
	  strcpy (cf_rec_file, "");
	}
    }
  else
    {
      cf_record = 0;
      cf_rec_append = 0;
      cf_rec_asplit = 0;
      cf_rec_isplit = 0;
      cf_rec_sildur = 360;
      cf_rec_guard = 15;
      cf_get_info = 0;
      cf_get_epg = 1;
      cf_rec_stime = 0;
      strcpy (cf_rec_file, "");

      log_lvl = 0;
    }

  if ((rc = log_open (&hlog, "audacious-dvb", log_lvl)) != RC_OK)
    hlog = NULL;

  log_print (hlog, LOG_INFO, "logging started.");
  log_print (hlog, LOG_DEBUG, "%.2f dB (%d), %d ms, %d s", cf_rec_sillvl, idb,
	     cf_rec_sildur, cf_rec_guard);

  audio = 0;
  t_start = 0;
  playing = 0;
  paused = 0;
  frm_ctr = 0;
  hdvb = NULL;
  rec_file = NULL;
  epg_running = 0;

  dvb_gui_init ();

  memset (&pt, 0x00, sizeof (pt));
  memset (&ptd, 0x00, sizeof (ptd));
  memset (&pte, 0x00, sizeof (pte));
}


static int
dvb_is_our_file (char *s)
{
  int rc;

  if ((rc = dvb_parse_url (s, NULL)) == RC_OK)
    return 1;

  log_print (hlog, LOG_DEBUG, "dvb_parse_url() returned rc = %d", rc);

  return 0;
}


static void
dvb_play (InputPlayback * playback)
{
  int rc, apid, dpid, sid;
  SVC svc;
  char tfn[MAXPATHLEN];
  struct stat st;
  gchar *s;

  s = playback->filename;

  log_print (hlog, LOG_DEBUG, "dvb_play(\"%s\");", s);

  if (playing)
    return;

  if ((rc = dvb_parse_url (s, &svc)) != RC_OK)
    {
      log_print (hlog, LOG_INFO, "dvb_parse_url() returned rc = %d", rc);
      return;
    }

  playing = 1;
  t_start = 0;
  frm_ctr = 0;

  sid = -1;
  sap = 0;
  memset (sumarr, 0x00, sizeof (sumarr));

  /* Initialize Service Information counters; */
  si_update = si_previous = 0;

  /* Make sure information retrieval is initialized. */
  mad_len = 0;
  mad_time = 0;
  memset (mad_buf, 0x00, sizeof (mad_buf));

  if (rec_file == NULL)
    {
      memset (erfn, 0x00, sizeof (erfn));
    }

  file_index = 0;
  if (cf_rec_asplit || cf_rec_isplit)
    {
      if (index (cf_rec_file, '%'))
	{
	  while (1)
	    {
	      sprintf (tfn, cf_rec_file, file_index);
	      if (stat (tfn, &st) < 0)
		break;
	      file_index++;
	    }
	}
    }

  if ((rc = dvb_open (svc.svc_adapter, &hdvb)) != RC_OK)
    {
      playing = 0;
      return;
    }

  if ((rc = dvb_tune_qpsk (hdvb, svc.svc_delivery[1] - 'A', svc.svc_frequency,
			   svc.svc_polarisation, svc.svc_symbolrate,
			   0)) != RC_OK)
    {
      playing = 0;
      dvb_close (hdvb);
      hdvb = NULL;
      return;
    }

  strcpy (service_name, s);
  dvb_info_update ("", "");

  if (strncasecmp (svc.svc_service, "0x", 2) == 0)
    {
      sscanf (&svc.svc_service[2], "%x", &apid);
    }
  else
    {
      if (strncasecmp (svc.svc_service, "sid=", 4) == 0)
	{
	  if (strncasecmp (&svc.svc_service[4], "0x", 2) == 0)
	    sscanf (&svc.svc_service[6], "%x", &sid);
	  else
	    sid = atoi (&svc.svc_service[4]);

	  if ((rc = dvb_get_pid (sid, &apid, &dpid)) != RC_OK)
	    {
	      log_print (hlog, LOG_WARNING, "dvb_get_pid() returned %d.", rc);
	      dvb_close (hdvb);
	      playing = 0;
	      hdvb = NULL;
	      return;
	    }
	}
      else
	{
	  apid = atoi (svc.svc_service);
	}
    }

  if ((rc = dvb_volume (hdvb, 0)) != RC_OK)
    log_print (hlog, LOG_WARNING, "dvb_volume() returned %d.", rc);

  if ((rc = dvb_apid (hdvb, apid)) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_apid() returned %d.", rc);
      dvb_close (hdvb);
      playing = 0;
      hdvb = NULL;
      return;
    }

  lame_decode_init ();

  if ((dpid > 0) && cf_get_info)
    {
      log_print (hlog, LOG_INFO,
		 "Data service associated on PID %d (0x%04x).", dpid, dpid);

      if ((rc = dvb_dpid (hdvb, dpid)) == RC_OK)
	{
	  if (pthread_create (&ptd, 0, dvb_madmusic, 0) != 0)
	    log_print (hlog, LOG_ERR,
		       "pthread_create() failed for dvb_madmusic()");
	}
      else
	{
	  log_print (hlog, LOG_ERR, "dvb_dpid() returned %d.", rc);
	}
    }
  else
    {
      memset (title, 0x00, sizeof (title));
      memset (artist, 0x00, sizeof (artist));
      memset (album, 0x00, sizeof (album));
    }

  if (sid != -1)
    {
      if (cf_get_epg)
	{
	  if (pthread_create (&pte, 0, dvb_epg, (void *) sid) != 0)
	    log_print (hlog, LOG_ERR,
		       "pthread_create() failed for dvb_epg()");
	  else
	    epg_running = 1;
	}
    }

  if (pthread_create (&pt, 0, dvb_feed, playback) != 0)
    {
      playing = 0;
      log_print (hlog, LOG_CRIT, "pthread_create() failed for dvb_feed()");
    }
}


static void
dvb_stop (InputPlayback * playback)
{
  if (playing)
    {
      playing = 0;
      paused = 0;

      pthread_join (pt, NULL);
      memset (&pt, 0x00, sizeof (pt));

      if (cf_get_info)
	{
	  pthread_join (ptd, NULL);
	  memset (&ptd, 0x00, sizeof (ptd));
	}

      if (epg_running && cf_get_epg)
	{
	  pthread_join (pte, NULL);
	  memset (&ptd, 0x00, sizeof (pte));
	  epg_running = 0;
	}

      playback->output->close_audio ();

      if (hdvb)
	/* Stopping the audio and data PID filters should probably be added. */
	dvb_close (hdvb);

      if (rec_file)
	dvb_close_record ();
    }
}


static void
dvb_pause (InputPlayback * playback, short i)
{
  if (playing)
    paused = i;
}


static int
dvb_gettime (InputPlayback * playback)
{
  if (playing)
    return (playback->output->output_time ());

  return 0;
}


static void
dvb_cleanup (void)
{
  log_print (hlog, LOG_INFO, "logging stopped.");
  log_close (hlog);

  hlog = NULL;
}


static int
dvb_parse_url (char *url, SVC * svc)
{
  int i, a_num, fec1, fec2, sr, qrg, fec;
  char *p, *q, fn[MAXPATHLEN], pol;
  char *sdlv, *sqrg, *sfec1, *sfec2;

  if ((strlen (url) + 1) > sizeof (fn))
    return RC_PARSE_URL_TOO_LONG;
  strcpy (fn, url);

  if (strncasecmp (fn, "dvb", 3) != 0)
    return RC_PARSE_URL_NOT_DVB_URL;

  if ((p = index (fn, ':')) == NULL)
    return RC_PARSE_URL_COLON_MISSING;

  *p++ = '\0';

  if (strlen (&fn[3]) > 0)
    {
      for (i = 0; i < strlen (&fn[3]); i++)
	{
	  if (!isdigit (fn[3 + i]))
	    return RC_PARSE_URL_ADAPTER_NOT_NUMERIC;
	}
    }
  a_num = atoi (&fn[3]);

  if (strncmp (p, "//", 2) != 0)
    return RC_PARSE_URL_SEPARATOR_MISSING;

  p += 2;

  if ((q = index (p, '/')) == NULL)
    return RC_PARSE_URL_DLVRY_SYS_MISSING;
  *q++ = '\0';
  sdlv = p;

  if ((p = index (q, '/')) == NULL)
    return RC_PARSE_URL_DLVRY_SYS_MISSING;
  *p++ = '\0';
  sqrg = q;

  if ((q = index (p, '/')) == NULL)
    return RC_PARSE_URL_SYMBOLRATE_MISSING;
  *q++ = '\0';
  sr = 1000 * atoi (p);

  if ((p = index (q, '/')) == NULL)
    return RC_PARSE_URL_FEC_DIVIDEND_MISSING;
  *p++ = '\0';
  sfec1 = q;
  fec1 = atoi (sfec1);

  fec = 0;

  if ((fec1 > 0) && (fec1 < 9))
    {
      if ((q = index (p, '/')) == NULL)
	return RC_PARSE_URL_FEC_DIVISOR_MISSING;
      *q++ = '\0';
      sfec2 = p;
      fec2 = atoi (sfec2);

      if ((fec1 + 1) != fec2)
	return RC_PARSE_URL_FEC_IMPLAUSIBLE;

      fec = fec1;
    }
  else
    {
      if (strcasecmp (sfec1, "none") == 0)
	{
	  fec = 0;
	}
      else
	{
	  if (strcasecmp (sfec1, "auto") != 0)
	    return RC_PARSE_URL_FEC_NOT_RECOGNIZED;

	  fec = 9;
	}
      q = p;
    }

  pol = ' ';

  if (toupper (sdlv[0]) == 'S')
    {
      switch (toupper (sqrg[strlen (sqrg) - 1]))
	{
	case 'H':
	  pol = 'H';
	  break;
	case 'V':
	  pol = 'V';
	  break;
	default:
	  return RC_PARSE_URL_UNK_POLARIZATION;
	  break;
	}

      sqrg[strlen (sqrg) - 1] = '\0';
    }

  qrg = 1000 * atoi (sqrg);

  if (svc != NULL)
    {
      svc->svc_adapter = a_num;
      svc->svc_frequency = qrg;
      svc->svc_viterbi = fec;
      svc->svc_symbolrate = sr;
      svc->svc_polarisation = pol;
      strcpy (svc->svc_delivery, sdlv);
      strcpy (svc->svc_service, q);
    }

  return RC_OK;
}


static void *
dvb_feed (void *args)
{
  int rc, ar, toctr;
  unsigned char pkt[3840];
  InputPlayback *playback;

  playback = (InputPlayback *) args;
  log_print (hlog, LOG_INFO, "dvb_feed() thread started");

  pthread_mutex_lock (&mutex);

  dvb_pes_pkt (playback, NULL, 0, 1);
  dvb_payload (playback, NULL, 0, 1);

  time (&isplit_last);

  toctr = 0;

  while (playing)
    {
      rc = dvb_apkt (hdvb, pkt, sizeof (pkt), 1000, &ar);
      if (rc == RC_OK)
	{
	  toctr = 0;
	  if (!paused)
	    dvb_pes_pkt (playback, pkt, ar, 0);
	}
      else
	{
	  if (rc == RC_DVB_APKT_SELECT_TIMEOUT)
	    {
	      toctr++;
	      if (toctr > 12)
		{
//          playing = 0;
		}
	      log_print (hlog, LOG_DEBUG, "dvb_apkt() timeout", rc);
	    }
	  else
	    {
	      log_print (hlog, LOG_ERR, "dvb_apkt() returned rc = %d", rc);
	      playing = 0;
	    }
	}
    }

  log_print (hlog, LOG_DEBUG, "play-loop terminated");

  dvb_unfilter (hdvb);
  dvb_close (hdvb);
  hdvb = NULL;
  t_start = 0;

  if (rec_file != NULL)
    dvb_close_record ();

  if (audio)
    {
      playback->output->close_audio ();
      audio = 0;
    }

  log_print (hlog, LOG_INFO, "dvb_feed() thread stopping");

  pthread_mutex_unlock (&mutex);
  pthread_exit (NULL);
}


static void
dvb_pes_pkt (InputPlayback * playback, unsigned char *buf, int len, int reset)
{
  int i, stream_id, PES_packet_length, j, pp_len;
  static int pbh, pbl;
  unsigned char *p, *pp;
  static unsigned char pesbuf[128 * 1024];

  if (reset)
    {
      pbh = pbl = 0;
      memset (pesbuf, 0x00, sizeof (pesbuf));
      return;
    }

  if ((pbh + len) > sizeof (pesbuf))
    {
      log_print (hlog, LOG_CRIT, "PES buffer overflow imminent, flushing!");
      pbh = pbl = 0;
      memset (pesbuf, 0x00, sizeof (pesbuf));
      return;
    }

  memcpy (&pesbuf[pbh], buf, len);
  pbh += len;

  while (1)
    {
      if (pbl > pbh)
	{
	  /* This is a serious logic error */
	}

      if (pbl < pbh)
	{
	  if ((pbh - pbl) > 4)
	    {
	      for (i = pbl; i < (pbh - 4); i++)
		{
/*          if ((pesbuf[i] == 0x00) &&
              (pesbuf[i + 1] == 0x00) &&
              (pesbuf[i + 2] == 0x01)) {
            printf("%02x\n", pesbuf[i + 3]);
          } */

		  if ((pesbuf[i] == 0x00) &&
		      (pesbuf[i + 1] == 0x00) &&
		      (pesbuf[i + 2] == 0x01) && ((pesbuf[i + 3] >> 4) > 0xa))
		    {
		      break;
		    }
		}

	      if (i < (pbh - 4))
		{
		  if (i > pbl)
		    {
		      memcpy (pesbuf, &pesbuf[i], pbh - i);
		      pbl = 0;
		      pbh = pbh - i;
		    }
		  else
		    {
		      stream_id = pesbuf[i + 3];
		      PES_packet_length =
			(pesbuf[i + 4] << 8) | pesbuf[i + 5];
		      if (PES_packet_length == 0)
			{
			  /* So now what? */
			  for (j = (i + 4); j < (pbh - 4); j++)
			    {
			      if ((pesbuf[j] == 0x00) &&
				  (pesbuf[j + 1] == 0x00) &&
				  (pesbuf[j + 2] == 0x01) &&
				  ((pesbuf[j + 3] >> 4) > 0xa))
				{
				  break;
				}
			    }

			  if (j < (pbh - 4))
			    {
			      PES_packet_length = j - i - 6;
			    }
			  else
			    {
			      return;
			    }
			}

		      if ((pbh - pbl) <= (PES_packet_length + 6))
			{
			  return;
			}
		      else
			{
			  /* Uhmmmm, complete? */
			  p = &pesbuf[i];

			  pp = p + 9 + p[8];
			  pp_len = PES_packet_length - 3 - p[8];
			  dvb_payload (playback, pp, pp_len, 0);

			  memcpy (pesbuf, &pesbuf[i + 6 + PES_packet_length],
				  pbh - (i + 6 + PES_packet_length));
			  pbl = 0;
			  pbh -= (i + 6 + PES_packet_length);
			}
		    }
		}
	      else
		{
		  memcpy (pesbuf, &pesbuf[i], pbh - i);
		  pbl = 0;
		  pbh -= i;
		  return;
		}
	    }
	  else
	    {
	      return;
	    }
	}
      else
	{
	  pbl = 0;
	  pbh = 0;
	  return;
	}
    }
}


static void
dvb_payload (InputPlayback * playback, unsigned char *buf, int len, int reset)
{
  int br, sf, fl, num_samples;
  int i, mpv, mpl, crc, bri, tlu, sfi, pad;
  static int bph = 0;
  static unsigned char mpbuf[128 * 1024];

  if (reset)
    {
      bph = 0;
      memset (mpbuf, 0x00, sizeof (mpbuf));
      return;
    }

  memcpy (&mpbuf[bph], buf, len);
  bph += len;

  while (1)
    {
      if ((mpbuf[0] == 0xff) && ((mpbuf[1] & 0xf0) == 0xf0))
	{
	  /* Frame sync at buffer start. A Good Thing. */
	  /* Is there a next one yet? */
	  for (i = 1; i < (bph - 4); i++)
	    {
	      if ((mpbuf[i] == 0xff) && ((mpbuf[i + 1] & 0xf0) == 0xf0))
		break;
	    }

	  if (i < (bph - 4))
	    {
	      /* yes, we may have an entire frame */
	      mpv = (mpbuf[1] >> 3) & 1;
	      mpl = (mpbuf[1] >> 1) & 3;
	      crc = mpbuf[1] & 1;
	      bri = (mpbuf[2] >> 4) & 0xf;
	      sfi = (mpbuf[2] >> 2) & 3;
	      pad = (mpbuf[2] >> 1) & 1;

	      tlu = bri | (mpl << 4) | (mpv << 6);
	      br = brt[tlu];

	      tlu = sfi | (mpv << 2);
	      sf = sft[tlu];

	      if ((sf == 0) || (br == 0))
		{
		  /* Uhm, no, this is not it */
		  memcpy (mpbuf, &mpbuf[i], bph - i);
		  bph -= i;
		  return;
		}

	      if (mpl == 3)
		{
		  num_samples = 384;
		  fl = (12 * (br * 1000) / sf + pad) * 4;
		}
	      else
		{
		  num_samples = 1152;
		  fl = 144 * (br * 1000) / sf + pad;
		}

	      if (fl > bph)
		return;

	      if ((mpbuf[fl] == 0xff) && ((mpbuf[fl + 1] & 0xf0) == 0xf0))
		{
		  dvb_mpeg_frame (playback, mpbuf, fl, num_samples);
		  memcpy (mpbuf, &mpbuf[fl], bph - fl);
		  bph -= fl;
		}
	      else
		{
		  memcpy (mpbuf, &mpbuf[i], bph - i);
		  bph -= i;
		}
	    }
	  else
	    {
	      /* whoops, that sucks */
	      return;
	    }
	}
      else
	{
	  for (i = 1; i < (bph - 4); i++)
	    {
	      if ((mpbuf[i] == 0xff) && ((mpbuf[i + 1] & 0xf0) == 0xf0))
		break;
	    }

	  if (i < (bph - 4))
	    {
	      memcpy (mpbuf, &mpbuf[i], bph - i);
	      bph -= i;
	    }
	  else
	    {
	      /* No sync in buffer yet, that hurts */
	      return;
	    }
	}
    }
}


static void
dvb_mpeg_frame (InputPlayback * playback, unsigned char *frame, int len,
		int smp)
{
  int nout, i, vu, ms;
  char info[4096];
  float dB;
  time_t t;
  static short left[34560], right[34560];
  static short stereo[sizeof (left) + sizeof (right)];
  mp3data_struct mp3d;

  frm_ctr++;

  memset (&mp3d, 0x00, sizeof (mp3d));

  if (cf_record)
    {
      time (&t);

      if (cf_rec_asplit && (t_start > 0) &&
	  (rec_file != NULL) && (cf_rec_stime > 0))
	{
	  if ((t - t_start) >= cf_rec_stime)
	      dvb_close_record ();
	}

      if (rec_file == NULL)
	{
	  if (cf_rec_asplit || cf_rec_isplit)
	      sprintf (erfn, cf_rec_file, file_index);
	  else
	      sprintf (erfn, cf_rec_file, 0);

	  if (cf_rec_append)
	    {
	      log_print (hlog, LOG_INFO, "opening record \"%s\" for append",
			 erfn);
	      rec_file = fopen (erfn, "ab");
	    }
	  else
	    {
	      log_print (hlog, LOG_INFO, "opening record \"%s\"", erfn);
	      rec_file = fopen (erfn, "wb");
	    }

	  if (cf_rec_asplit || cf_rec_isplit)
	    {
	      if (rec_file != NULL)
		{
		  time (&t_start);
		  file_index++;
		}
	    }
	}

      if (rec_file != NULL)
	  fwrite (frame, sizeof (unsigned char), len, rec_file);
    }

  if ((nout = lame_decode_headers (frame, len, left, right, &mp3d)) > 0)
    {
      if (mp3d.header_parsed == 1)
	{
	  if (!audio)
	      audio =
		playback->output->open_audio (FMT_S16_NE, mp3d.samplerate, 2);

	  /* 
	   * This is just a quick fix -- we now update the info only
	   * every 120 frames since it's so computationally expensive.
	   * Eventually it should be updated whenever it changes, which
	   * shouldn't be too often in Real World conditions.
	   */
	  if (si_update > si_previous)
	    {
	      si_previous = si_update;
	      if (cf_get_epg && epg_running && (strlen (epg_desc) > 0))
		{
		  sprintf (info, "%s: %s", service_name, epg_desc);
		  dvb_ip.set_info ((gchar *)str_to_utf8(info), -1, mp3d.bitrate * 1000,
				   mp3d.samplerate, mp3d.stereo);
		}
	      else
		{
		  dvb_ip.set_info ((gchar *)str_to_utf8(service_name), -1, mp3d.bitrate * 1000,
				   mp3d.samplerate, mp3d.stereo);
		}
	    }
	}

      /*
       * Unfortunately Audacious' output wants sample data interleaved,
       * not separate, so we have to rearrange it accordingly. But
       * that's ok, we need to look at the PCM data to determine
       * the energy level anyway :)
       */
      vu = 0;
      for (i = 0; i < nout; i++)
	{
	  if (mp3d.stereo == 2)
	    {
	      stereo[2 * i] = left[i];
	      stereo[2 * i + 1] = right[i];
	      vu += (abs (left[i]) + abs (right[i])) / 2;
	    }
	  else
	    {
	      stereo[2 * i] = left[i];
	      stereo[2 * i + 1] = left[i];
	      vu += abs (left[i]);
	    }
	}

      vu /= nout;

      if (audio)
	  produce_audio(playback->output->written_time(), FMT_S16_NE, 2, nout << 2, stereo, NULL);

      sumarr[sap++] = vu;
      ms = (sap * nout) / mp3d.bitrate;
      if (ms >= cf_rec_sildur)
	{
	  vu = 0;

	  for (i = 0; i < sap; i++)
	    vu += sumarr[i];

	  vu /= sap;

	  dB = 20 * log10 ((float) vu / 32767);

	  if (dB < cf_rec_sillvl)
	    {
	      time (&t);
	      if ((t - isplit_last) >= cf_rec_guard)
		{
		  log_print (hlog, LOG_INFO,
			     "Avg: %.2f dB (%d) / %d ms (%d f) / %d:%02d.",
			     dB, vu, ms, sap, (t - isplit_last) / 60,
			     (t - isplit_last) % 60);
		  time (&isplit_last);

		  if (cf_rec_isplit && cf_record)
		    dvb_close_record ();

		  sap = 1;
		  memset (sumarr, 0x00, sizeof (sumarr));
		}
	    }

	  memcpy (sumarr, &sumarr[1], sizeof (int) * (sap - 1));
	  sap--;
	}
    }
}


void
dvb_close_record ()
{
  if (rec_file != NULL)
    {
      log_print (hlog, LOG_INFO, "closing record \"%s\"", erfn);
      fclose (rec_file);
      rec_file = NULL;
      sap = 0;
      memset (sumarr, 0x00, sizeof (sumarr));

      if (strlen (title) > 0)
	{
	  if ((mad_time > 0) && (t_start <= mad_time))
	    {
	      log_print (hlog, LOG_INFO, "%d,%s,%s,%s", trnum, album, artist,
			 title);
	      /*
	       * One idea would be to attach an ID3 tag to the just recorded
	       * file at this point. If I find as some decent code for it I
	       * will probably do just that. But ID3Lib SUCKS!
	       */
	    }
	  else
	    {
	      log_print (hlog, LOG_INFO, "Track info %d:%02d too old",
			 (t_start - mad_time) / 60,
			 (t_start - mad_time) % 60);
	    }
	}
    }
}


static int
dvb_get_pid (int s, int *apid, int *dpid)
{
  int rc, len, pmt, pil, es, es_type, es_audio, es_data;
  static int sid;
  unsigned char sct[4096], *p, *q;

  if ((rc = dvb_section (hdvb, 0, 0, 0, 0, sct, 10000)) != RC_OK)
    return rc;

  len = 3 + (((sct[1] << 8) | sct[2]) & 0xfff);
  p = &sct[8];
  q = (sct + len) - 4;

  es_audio = es_data = -1;

  while (p < q)
    {
      sid = (p[0] << 8) | p[1];
      pmt = ((p[2] << 8) | p[3]) & 0x1fff;

      if (sid == s)
	{
	  if ((rc = dvb_section (hdvb, pmt, 2, sid, 0, sct, 10000)) == RC_OK)
	    {
	      len = 3 + (((sct[1] << 8) | sct[2]) & 0xfff);
	      pil = ((sct[10] << 8) | sct[11]) & 0xfff;
	      p = sct + 12 + pil;
	      q = sct + len - 4;

	      while (p < q)
		{
		  es = ((p[1] << 8) | p[2]) & 0x1fff;
		  es_type = p[0];
		  pil = ((p[3] << 8) | p[4]) & 0xfff;
		  p += 5;

		  if ((es_type == 0x03) || (es_type == 0x04))
		    {
		      if (es_audio < 0)
			{
			  log_print (hlog, LOG_INFO,
				     "Service Audio PID = %d (0x%04x)", es,
				     es);
			  if (pthread_create
			      (&pt, 0, dvb_get_name, (void *) &sid) != 0)
			    {
			      log_print (hlog, LOG_WARNING,
					 "Failed to start dvb_get_name() thread");
			    }

			  es_audio = es;
			}
		    }

		  if (es_type == 0x05)
		    {
		      if (es_data < 0)
			{
			  log_print (hlog, LOG_INFO,
				     "Service Data PID = %d (0x%04x)", es,
				     es);

			  es_data = es;
			}
		    }

		  p += pil;
		}
	    }
	  else
	    {
	      return rc;
	    }
	}

      p += 4;
    }

  if (es_audio < 0)
    {
      return RC_DVB_GET_PID_SID_NOT_IN_PAT;
    }

  *apid = es_audio;
  *dpid = es_data;

  return RC_OK;
}


static void *
dvb_get_name (void *arg)
{
  int sct, rc, len, sid, dt, dl, svc_sid;
  char prov[256], name[256];
  unsigned char s[4096], *p, *q, *pp, *qq;

  svc_sid = *(int *) arg;

  pthread_mutex_lock (&svc_mutex);

  log_print (hlog, LOG_INFO, "dvb_get_name(%d) thread starting", svc_sid);

  sct = 0;

  while (playing)
    {
      if ((rc = dvb_section (hdvb, 0x0011, 0x42, 0, sct, s, 10000)) != RC_OK)
	{
	  log_print (hlog, LOG_INFO, "dvb_section() returned %d", rc);
	  break;
	}
      len = 3 + (((s[1] << 8) | s[2]) & 0xfff);
      log_print (hlog, LOG_DEBUG, "SDT section length = %d", len);

      p = &s[11];
      q = (s + len) - 4;

      while ((p < q) && playing)
	{
	  sid = (p[0] << 8) | p[1];
	  len = ((p[3] << 8) | p[4]) & 0xfff;
	  p += 5;

	  log_print (hlog, LOG_DEBUG, "SDT service ID = %d", sid);

	  if (sid == svc_sid)
	    {
	      pp = p;
	      qq = pp + len;
	      while ((pp < qq) && playing)
		{
		  dt = *pp++;
		  dl = *pp++;

		  if (dt == 0x48)
		    {
		      memcpy (prov, &pp[2], pp[1]);
		      prov[pp[1]] = '\0';
		      dvb_clean_string (prov);

		      memcpy (name, &pp[3 + pp[1]], pp[2 + pp[1]]);
		      name[pp[2 + pp[1]]] = '\0';
		      dvb_clean_string (name);

		      log_print (hlog, LOG_INFO,
				 "Service name: \"%s\", \"%s\"", prov, name);

		      dvb_info_update (prov, name);

		      if (strlen (prov) > 0)
			sprintf (service_name, "%s - %s", prov, name);
		      else
			sprintf (service_name, "%s", name);
		      si_update++;

		      log_print (hlog, LOG_INFO,
				 "dvb_get_name() thread stopping");

		      pthread_mutex_unlock (&svc_mutex);
		      pthread_exit (NULL);
		      return NULL;
		    }

		  pp += dl;
		}
	    }

	  p += len;
	}

      if ((s[6] == s[7]) && (s[6] == sct))
	{
	  log_print (hlog, LOG_INFO, "dvb_get_name() last section");
	  break;
	}

      sct++;
    }

  log_print (hlog, LOG_INFO, "dvb_get_name() thread stopping");

  pthread_mutex_unlock (&svc_mutex);
  pthread_exit (NULL);
  return NULL;
}


static void *
dvb_madmusic (void *arg)
{
  int rc, dr, slen, blen, off, fbf;
  unsigned char sect[5120], rtxt[32768];

  log_print (hlog, LOG_INFO, "dvb_madmusic() thread started");

  pthread_mutex_lock (&data_mutex);

  fbf = off = 0;

  while (playing && cf_get_info)
    {
      memset (sect, 0xff, sizeof (sect));
      rc = dvb_dpkt (hdvb, sect, sizeof (sect), 1000, &dr);
      if (rc != RC_OK)
	{
	  if (rc != RC_DVB_DPKT_SELECT_TIMEOUT)
	    {
	      log_print (hlog, LOG_ERR,
			 "Error: data reception died, rc = %d!", rc);
	      break;
	    }
	  else
	    {
	      log_print (hlog, LOG_INFO, "data reception stalling ...", rc);
	    }
	}
      else
	{
	  slen = ((sect[1] << 8) | sect[2]) & 0xfff;
	  blen = ((sect[22] << 8) | sect[23]) & 0xfff;
	  if (blen <= (slen - 21))
	    {
	      dvb_parse_text (&sect[24], blen - 2);
	      fbf = 0;
	    }
	  else
	    {
	      off = ((sect[18] << 8) | sect[19]);
	      if (fbf == off)
		{
		  memcpy (&rtxt[off], &sect[24], slen - 21);
		  if ((off + (slen - 21)) >= blen)
		    {
		      dvb_parse_text (rtxt, blen - 2);
		      fbf = 0;
		    }
		  else
		    {
		      fbf += (slen - 21 - 4);
		    }
		}
	      else
		{
		  log_print (hlog, LOG_WARNING,
			     "Warning! Offset out of whack, is %d, should be %d!",
			     off, fbf);
		  fbf = 0;
		}
	    }
	}
    }

  log_print (hlog, LOG_INFO, "dvb_madmusic() thread stopping");

  pthread_mutex_unlock (&data_mutex);
  pthread_exit (NULL);
}


static void
dvb_parse_text (unsigned char *buf, int len)
{
  int field, ftna, toai;
  char toan[32];
  unsigned char *p, *q, *r, *rr, wbuf[8192];

  if (len == mad_len)
    {
      if (memcmp (mad_buf, buf, len) == 0)
	return;
    }

  memcpy (mad_buf, buf, len);
  mad_len = len;
  time (&mad_time);

  toai = 0;
  toan[toai] = '\0';

  trnum = 0;
  memset (artist, 0x00, sizeof (artist));
  memset (title, 0x00, sizeof (title));
  memset (album, 0x00, sizeof (album));

  memset (wbuf, 0x00, sizeof (wbuf));
  memcpy (wbuf, buf, len);
  dvb_fixbillshit (wbuf);
  p = wbuf;
  field = 0;

  ftna = 0;

  while (1)
    {
      q = index (p, '|');
      if (q == NULL)
	break;

      *q = '\0';
      q++;

      switch (field)
	{
	case 3:
	  strcpy (artist, p);
	  dvb_xlt (artist);
	  break;
	case 4:
	  strcpy (title, p);
	  dvb_xlt (title);
	  ftna = 1;
	  break;
	case 5:
	  strcpy (album, p);
	  dvb_xlt (album);
	  break;
	default:
	  break;
	}

      if (ftna)
	{
	  if ((r = strstr (p, title)) != NULL)
	    {
	      rr = r - 4;
	      if (rr < p)
		rr = p;

	      toai = 0;
	      toan[toai] = '\0';
	      while (rr < r)
		{
		  if ((*rr >= '0') && (*rr <= '9'))
		    toan[toai++] = *rr;
		  rr++;
		}
	      toan[toai] = '\0';
	      trnum = atoi (toan);
	    }
	}

      p = q;
      field++;

      if (p >= (wbuf + len))
	break;

      if (*p == '^')
	ftna = 0;
    }

  log_print (hlog, LOG_INFO, "Album : %s", album);
  log_print (hlog, LOG_INFO, "Track : %d", trnum);
  log_print (hlog, LOG_INFO, "Artist: %s", artist);
  log_print (hlog, LOG_INFO, "Title : %s", title);

  if (strlen (artist) > 0)
    sprintf (service_name, "%s - %s", artist, title);
  else
    sprintf (service_name, "%s", title);
  si_update++;
}


static void
dvb_fixbillshit (unsigned char *s)
{
  while (*s)
    {
      switch (*s)
	{
	case 0xa1:
	  *s = 0xb5;
	  break;
	case 0xa2:
	  *s = 0xb6;
	  break;
	}
      s++;
    }
}


static void
dvb_xlt (unsigned char *s)
{
  while (*s)
    {
      if ((*s == '\n') || (*s == '\r'))
	*s = ' ';
      s++;
    }
}
