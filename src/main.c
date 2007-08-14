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

#include <glib.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/vfs.h>

#include <lame/lame.h>

#include "gui.h"
#include "epg.h"
#include "dvb.h"
#include "log.h"
#include "cfg.h"
#include "rtxt.h"
#include "mmusic.h"
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
#define RC_PARSE_URL_POLARIZATION_MISSING 1012


typedef struct _SVC
{
  gint adapter;
  gint frequency;
  gint symbolrate;
  gint viterbi;
  gchar polarisation;
  gchar delivery[256];
  gint spid;
} SVC;

typedef struct _statstruct
{
  gchar *svc_name;
  gchar *prov_name;
  gboolean refresh;
} statstruct;

void dvb_close_record (void);

static void dvb_init (void);
static gint dvb_is_our_file (gchar *);
static void dvb_play (InputPlayback *);
static void dvb_stop (InputPlayback *);
static void dvb_pause (InputPlayback *, gshort);
static gint dvb_gettime (InputPlayback *);
static void dvb_cleanup (void);

static gint dvb_parse_url (gchar *, SVC *);
static void dvb_pes_pkt (InputPlayback *, guchar *, gint, gint);
static void dvb_payload (InputPlayback *, guchar *, gint, gint);
static void dvb_mpeg_frame (InputPlayback *, guchar *, gint, gint);
gint dvb_read_conf (gchar *, gchar *, gchar *, gchar *, gint, gchar *);

// Threads
static gpointer feed_thread (gpointer);
static gpointer get_name_thread (gpointer);
static gpointer epg_thread (gpointer);
static gpointer mmusic_thread (gpointer);


// Miscellaneous globals
gint playing;			// This is also used in the GUI
gpointer hlog;			// This is used everywhere :)
gpointer hdvb;			// EPG retrieval uses this
gint epg_running;

// Internal interfaces
cfgstruct *config = NULL;
mmstruct *mmusic = NULL;
rtstruct *rt = NULL;
epgstruct *epg = NULL;
statstruct *station = NULL;

static gint paused, audio, file_index, frm_ctr;
static gchar erfn[MAXPATHLEN];
static VFSFile *rec_file;
time_t t_start;
static time_t isplit_last;

static GThread *gt_feed = NULL;
static GThread *gt_get_name = NULL;
static GThread *gt_epg = NULL;
static GThread *gt_mmusic = NULL;

static GMutex *gmt_feed = NULL;
static GMutex *gmt_get_name = NULL;
static GMutex *gmt_epg = NULL;
static GMutex *gmt_mmusic = NULL;

static gint sap;
static gint sumarr[512];

static gint brt[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
  0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0,
  0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0,
  0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0,
  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
};

static gint sft[] = {
  22050, 24000, 16000, 0, 44100, 48000, 32000, 0
};

InputPlugin *dvb_ip = NULL;


InputPlugin *
get_iplugin_info (void)
{
  if (dvb_ip == NULL)
    {
      dvb_ip = g_new0 (InputPlugin, 1);
      dvb_ip->description = g_strdup ("DVB Input Plugin");
      dvb_ip->init = dvb_init;
      dvb_ip->about = dvb_about;
      dvb_ip->configure = dvb_configure;
      dvb_ip->is_our_file = dvb_is_our_file;
      dvb_ip->play_file = dvb_play;
      dvb_ip->stop = dvb_stop;
      dvb_ip->pause = dvb_pause;
      dvb_ip->get_time = dvb_gettime;
      dvb_ip->cleanup = dvb_cleanup;
      dvb_ip->file_info_box = dvb_getinfo;
    }
  return dvb_ip;
}


static void
dvb_init (void)
{
  gint rc;

  if (config == NULL)
    {
      config = g_new (cfgstruct, 1);
      config_from_db (config);
    }

  if (log_open (&hlog, "auddacious-dvb", config->loglvl) != RC_OK)
    hlog = NULL;

  log_print (hlog, LOG_INFO, "logging started.");

  audio = 0;
  t_start = 0;
  playing = 0;
  paused = 0;
  frm_ctr = 0;
  hdvb = NULL;
  rec_file = NULL;
  epg_running = 0;

  if (!g_thread_supported ())
    g_thread_init (NULL);
}


static gint
dvb_is_our_file (gchar * s)
{
  gint rc;

  if ((rc = dvb_parse_url (s, NULL)) == RC_OK)
    return 1;

  log_print (hlog, LOG_DEBUG, "dvb_parse_url() returned rc = %d", rc);

  return 0;
}


static void
dvb_play (InputPlayback * playback)
{
  gint rc, apid, dpid;
  SVC svc;
  gchar tfn[MAXPATHLEN];
  struct stat st;

  log_print (hlog, LOG_DEBUG, "dvb_play(\"%s\");", playback->filename);

  if (playing)
    return;

  if ((rc = dvb_parse_url (playback->filename, &svc)) != RC_OK)
    {
      log_print (hlog, LOG_INFO, "dvb_parse_url() returned rc = %d", rc);
      return;
    }

  playing = 1;
  t_start = 0;
  frm_ctr = 0;

  sap = 0;
  memset (sumarr, 0x00, sizeof (sumarr));

  if (station == NULL)
    station = g_malloc0(sizeof(statstruct));
  // TODO: initialize statstruct->name with playlist entry

  if (rec_file == NULL)
    memset (erfn, 0x00, sizeof (erfn));

  file_index = 0;
  if (config->isplit || config->vsplit)
    {
      if (index (config->rec_fname, '%'))
	{
	  while (1)
	    {
	      g_sprintf (tfn, config->rec_fname, file_index);
	      if (stat (tfn, &st) < 0)
		break;
	      file_index++;
	    }
	}
    }

  if ((rc = dvb_open (svc.adapter, &hdvb)) != RC_OK)
    {
      playing = 0;
      return;
    }

  if ((rc = dvb_tune_qpsk (hdvb, svc.delivery[1] - 'A', svc.frequency,
			   svc.polarisation, svc.symbolrate, 0)) != RC_OK)
    {
      dvb_close (hdvb);
      playing = 0;
      hdvb = NULL;
      return;
    }

  g_free(station->svc_name);
  station->svc_name = g_strdup(playback->filename);

  infobox_update_service ("", "");

  if ((rc = dvb_get_pid (hdvb, svc.spid, &apid, &dpid)) != RC_OK)
    {
      log_print (hlog, LOG_WARNING, "dvb_get_pid() returned %d.", rc);
      dvb_close (hdvb);
      playing = 0;
      hdvb = NULL;
      return;
    }

  if ((gt_get_name =
       g_thread_create (get_name_thread, (gpointer) & svc.spid, TRUE,
			NULL)) == NULL)
    log_print (hlog, LOG_WARNING, "Failed to start dvb_get_name() thread");

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

  if ((dpid > 0) && config->info_mmusic)
    {
      log_print (hlog, LOG_INFO,
		 "Data service associated on PID %d (0x%04x).", dpid, dpid);

      if ((rc = dvb_dpid (hdvb, dpid)) == RC_OK)
	{
	  if ((gt_mmusic =
	       g_thread_create (mmusic_thread, 0, TRUE, NULL)) == NULL)
	    log_print (hlog, LOG_ERR,
		       "g_thread_create() failed for dvb_madmusic()");
	}
      else
	log_print (hlog, LOG_ERR, "dvb_dpid() returned %d.", rc);
    }

  if (config->info_epg)
    {
      if ((gt_epg =
	   g_thread_create (epg_thread, (gpointer) & svc.spid, TRUE,
			    NULL)) == NULL)
	log_print (hlog, LOG_ERR, "g_thread_create() failed for dvb_epg()");
      else
	epg_running = 1;
    }

  if ((gt_feed = g_thread_create (feed_thread, playback, TRUE, NULL)) == NULL)
    {
      playing = 0;
      log_print (hlog, LOG_CRIT, "g_thread_create() failed for dvb_feed()");
    }
}


static void
dvb_stop (InputPlayback * playback)
{
  if (playing)
    {
      playing = 0;
      paused = 0;
      epg_running = 0;

      // Stop all threads
      if (gt_feed != NULL)
	g_thread_join (gt_feed);
      if (gt_get_name != NULL)
	g_thread_join (gt_get_name);
      if (gt_mmusic != NULL)
	g_thread_join (gt_mmusic);
      if (gt_epg != NULL)
	g_thread_join (gt_epg);
      gt_feed = gt_get_name = gt_mmusic = gt_epg = NULL;

      playback->output->close_audio ();
      
      g_free(station);
      station = NULL;

      if (hdvb)
	{
	  dvb_unfilter (hdvb);
	  dvb_close (hdvb);
	}

      if (rec_file)
	dvb_close_record ();
    }
}


static void
dvb_pause (InputPlayback * playback, gshort i)
{
  if (playing)
    paused = i;
}


static gint
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


static gint
dvb_parse_url (gchar * url, SVC * svc)
{
  gint i, a_num, fec1, fec2, sr, qrg, fec, spid;
  gchar *p, *q, fn[MAXPATHLEN], pol;
  gchar *sdlv, *sqrg, *sfec1, *sfec2;

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
    return RC_PARSE_URL_POLARIZATION_MISSING;
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
	fec = 0;
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

  if (strncasecmp (q, "0x", 2) == 0)
    {
      q += 2;
      sscanf ((char *) &q, "%x", &spid);
    }
  else
    spid = atoi (q);

  if (svc != NULL)
    {
      svc->adapter = a_num;
      svc->frequency = qrg;
      svc->viterbi = fec;
      svc->symbolrate = sr;
      svc->polarisation = pol;
      strcpy (svc->delivery, sdlv);
      svc->spid = spid;
    }

  return RC_OK;
}


static gpointer
feed_thread (gpointer args)
{
  gint rc, ar, toctr;
  guchar pkt[3840];
  InputPlayback *playback;

  playback = (InputPlayback *) args;
  log_print (hlog, LOG_INFO, "dvb_feed_thread() starting");

  if (gmt_feed == NULL)
    gmt_feed = g_mutex_new ();
  g_mutex_lock (gmt_feed);
  
  rt = radiotext_init();

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
		playing = 0;
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

  t_start = 0;

  if (rec_file != NULL)
    dvb_close_record ();

  if (audio)
    {
      playback->output->close_audio ();
      audio = 0;
    }

  radiotext_exit(rt);

  
  log_print (hlog, LOG_INFO, "dvb_feed_thread() stopping");

  g_mutex_unlock (gmt_feed);
  gmt_feed = NULL;
  g_thread_exit (0);
}


static void
dvb_pes_pkt (InputPlayback * playback, guchar * buf, gint len, gint reset)
{
  gint i, stream_id, PES_packet_length, j, pp_len;
  static gint pbh, pbl;
  guchar *p, *pp;
  static guchar pesbuf[128 * 1024];

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
      if (pbl < pbh)
	{
	  if ((pbh - pbl) > 4)
	    {
	      for (i = pbl; i < (pbh - 4); i++)
		{
		  if ((pesbuf[i] == 0x00) &&
		      (pesbuf[i + 1] == 0x00) &&
		      (pesbuf[i + 2] == 0x01) && ((pesbuf[i + 3] >> 4) > 0xa))
		    break;
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
				break;
			    }

			  if (j < (pbh - 4))
			    PES_packet_length = j - i - 6;
			  else
			    return;
			}

		      if ((pbh - pbl) > (PES_packet_length + 6))
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
		      else
			return;
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
	    return;
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
dvb_payload (InputPlayback * playback, guchar * buf, gint len, gint reset)
{
  gint br, sf, fl, num_samples;
  gint i, mpv, mpl, crc, bri, tlu, sfi, pad;
  static gint bph = 0;
  static guchar mpbuf[128 * 1024];

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
		  radiotext_read_data (rt, mpbuf, fl);
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
	    /* whoops, that sucks */
	    return;
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
	    /* No sync in buffer yet, that hurts */
	    return;
	}
    }
}


static void
dvb_mpeg_frame (InputPlayback * playback, guchar * frame, gint len, gint smp)
{
  gint nout, i, vu, ms;
  gchar info[4096];
  gfloat dB;
  time_t t;
  static gshort left[34560], right[34560];
  static gshort stereo[sizeof (left) + sizeof (right)];
  mp3data_struct mp3d;

  frm_ctr++;

  memset (&mp3d, 0x00, sizeof (mp3d));

  if (config->rec)
    {
      time (&t);

      if (config->isplit && (t_start > 0) &&
	  (rec_file != NULL) && (config->isplit_ival > 0))
	{
	  if ((t - t_start) >= config->isplit_ival)
	    dvb_close_record ();
	}

      if (rec_file == NULL)
	{
	  if (config->isplit || config->vsplit)
	    g_sprintf (erfn, config->rec_fname, file_index);
	  else
	    g_sprintf (erfn, config->rec_fname, 0);

	  if (config->rec_append)
	    {
	      log_print (hlog, LOG_INFO, "opening record \"%s\" for append",
			 erfn);
	      rec_file = vfs_fopen (erfn, "ab");
	    }
	  else
	    {
	      log_print (hlog, LOG_INFO, "opening record \"%s\"", erfn);
	      rec_file = vfs_fopen (erfn, "wb");
	    }

	  if (config->isplit || config->vsplit)
	    {
	      if (rec_file != NULL)
		{
		  time (&t_start);
		  file_index++;
		}
	    }
	}

      if (rec_file != NULL)
	vfs_fwrite (frame, sizeof (unsigned char), len, rec_file);
    }

  if ((nout = lame_decode_headers (frame, len, left, right, &mp3d)) > 0)
    {
      if (mp3d.header_parsed == 1)
	{
	  if (!audio)
	    audio =
	      playback->output->open_audio (FMT_S16_NE, mp3d.samplerate, 2);

	  if (mmusic != NULL && mmusic->refresh)
	    {
	      log_print (hlog, LOG_INFO, "MadMusic info changed!");
	      mmusic->refresh = FALSE;
	    }
	  if (rt != NULL && rt->refresh)
	    {
	      gchar *title = g_strdup_printf("%s: %s - %s", station->svc_name,
					     rt->artist, rt->title);
	      log_print (hlog, LOG_INFO, "Radiotext info changed!");
	      dvb_ip->set_info ((gchar *) str_to_utf8 (title), -1,
				mp3d.bitrate * 1000, mp3d.samplerate,
				mp3d.stereo);
	      g_free(title);
	      rt->refresh = FALSE;
	    }
	  if (epg != NULL && epg->refresh)
	    {
	      log_print (hlog, LOG_INFO, "EPG info changed!");
	      epg->refresh = FALSE;
	    }
	  if (station != NULL && station->refresh)
	    {
	      log_print (hlog, LOG_INFO, "Station info changed!");
	      dvb_ip->set_info ((gchar *) str_to_utf8 (station->svc_name), -1,
				mp3d.bitrate * 1000, mp3d.samplerate,
				mp3d.stereo);
	      station->refresh = FALSE;
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
	produce_audio (playback->output->written_time (), FMT_S16_NE, 2,
		       nout << 2, stereo, NULL);

      sumarr[sap++] = vu;
      ms = (sap * nout) / mp3d.bitrate;
      if (ms >= config->vsplit_dur)
	{
	  vu = 0;

	  for (i = 0; i < sap; i++)
	    vu += sumarr[i];

	  vu /= sap;

	  dB = 20 * log10 ((float) vu / 32767);

	  if (dB < config->vsplit_vol)
	    {
	      time (&t);
	      if ((t - isplit_last) >= config->vsplit_minlen)
		{
		  log_print (hlog, LOG_INFO,
			     "Avg: %.2f dB (%d) / %d ms (%d f) / %d:%02d.",
			     dB, vu, ms, sap, (t - isplit_last) / 60,
			     (t - isplit_last) % 60);
		  time (&isplit_last);

		  if (config->vsplit && config->rec)
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
dvb_close_record (void)
{
  if (rec_file != NULL)
    {
      log_print (hlog, LOG_INFO, "closing record \"%s\"", erfn);
      vfs_fclose (rec_file);
      rec_file = NULL;
      sap = 0;
      memset (sumarr, 0x00, sizeof (sumarr));

      if (mmusic != NULL)
	madmusic_exit (mmusic);
    }
}


static gpointer
get_name_thread (gpointer arg)
{
  gint sct, rc, len, sid, dt, dl, svc_sid;
  gchar prov[256], name[256];
  guchar s[4096], *p, *q, *pp, *qq;

  svc_sid = *((gint *) arg);

  log_print (hlog, LOG_INFO, "get_name_thread(%d) starting", svc_sid);

  if (gmt_get_name == NULL)
    gmt_get_name = g_mutex_new ();
  g_mutex_lock (gmt_get_name);

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
		      clean_string (prov);

		      memcpy (name, &pp[3 + pp[1]], pp[2 + pp[1]]);
		      name[pp[2 + pp[1]]] = '\0';
		      clean_string (name);

		      if (is_updated (prov, &station->prov_name))
			station->refresh = TRUE;

		      if (is_updated (name, &station->svc_name))
			station->refresh = TRUE;

		      if (station->refresh)
			{
			  log_print (hlog, LOG_INFO,
				     "Service name: \"%s\", \"%s\"", prov, name);
			  infobox_update_service (prov, name);
			}

		      log_print (hlog, LOG_INFO,
				 "get_name_thread() stopping");

		      g_mutex_unlock (gmt_get_name);
		      gmt_get_name = NULL;
		      g_thread_exit (0);

		      return NULL;
		    }

		  pp += dl;
		}
	    }

	  p += len;
	}

      if ((s[6] == s[7]) && (s[6] == sct))
	{
	  log_print (hlog, LOG_INFO, "get_name_thread() last section");
	  break;
	}

      sct++;
    }

  log_print (hlog, LOG_INFO, "get_name_thread() stopping");

  g_mutex_unlock (gmt_get_name);
  gmt_get_name = NULL;
  g_thread_exit (0);

  return NULL;
}


static gpointer
epg_thread (gpointer arg)
{
  gint sid, rc, len, sct;
  guchar s[4096];

  log_print (hlog, LOG_INFO, "epg_thread() started");

  if (gmt_epg == NULL)
    gmt_epg = g_mutex_new ();
  g_mutex_lock (gmt_epg);

  sid = *((gint *) arg);
  log_print (hlog, LOG_DEBUG, "EPG SID: %d (0x%04x)", sid, sid);
  
  // Make sure EPG retrieval is initialized
  if ((epg = epg_init ()) == NULL)
    {
      g_mutex_unlock (gmt_epg);
      gmt_epg = NULL;
      g_thread_exit (0);
      return NULL;
    }

  sct = 0;

  epg_read_data (epg, NULL, 0);

  while (playing)
    {
      if ((rc = dvb_section (hdvb, 0x0012, 0x4e, sid, sct, s, 1000)) != RC_OK)
	{
	  if (rc != RC_DVB_SECTION_SELECT_TIMEOUT)
	    break;
	}
      else
	{
	  len = 3 + (((s[1] << 8) | s[2]) & 0xfff);

	  log_print (hlog, LOG_DEBUG, "EPG section lenght = %d", len);

	  rc = epg_read_data (epg, s, len);

	  if (s[6] != s[7])
	    sct++;
	  else
	    sct = 0;
	}
    }

  epg_exit(epg);
  epg = NULL;
  
  log_print (hlog, LOG_INFO, "epg_thread() stopping");

  g_mutex_unlock (gmt_epg);
  gmt_epg = NULL;
  g_thread_exit (0);
}


static gpointer
mmusic_thread (gpointer arg)
{
  gint rc, dr, slen, blen, off, fbf;
  guchar sect[5120], rtxt[32768];

  log_print (hlog, LOG_INFO, "mmusic_thread() starting");

  if (gmt_mmusic == NULL)
    gmt_mmusic = g_mutex_new ();
  g_mutex_lock (gmt_mmusic);

  // Make sure information retrieval is initialized
  if ((mmusic = madmusic_init ()) == NULL)
    {
      g_mutex_unlock (gmt_mmusic);
      gmt_mmusic = NULL;
      g_thread_exit (0);
      return NULL;
    }

  fbf = off = 0;

  while (playing && config->info_mmusic)
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
	    log_print (hlog, LOG_INFO, "data reception stalling ...", rc);
	}
      else
	{
	  slen = ((sect[1] << 8) | sect[2]) & 0xfff;
	  blen = ((sect[22] << 8) | sect[23]) & 0xfff;
	  if (blen <= (slen - 21))
	    {
	      madmusic_read_data (mmusic, &sect[24], blen - 2);
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
		      madmusic_read_data (mmusic, rtxt, blen - 2);
		      fbf = 0;
		    }
		  else
		    fbf += (slen - 21 - 4);
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

  madmusic_exit (mmusic);
  mmusic = NULL;
  
  log_print (hlog, LOG_INFO, "mmusic_thread() stopping");

  g_mutex_unlock (gmt_mmusic);
  gmt_mmusic = NULL;
  g_thread_exit (0);
}
