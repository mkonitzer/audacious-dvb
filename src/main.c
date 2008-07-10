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
#include <glib/gprintf.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <audacious/output.h>
#include <audacious/plugin.h>
#include <audacious/strings.h>
#include <audacious/vfs.h>
#include <lame/lame.h>

#include "gui.h"
#include "epg.h"
#include "dvb.h"
#include "log.h"
#include "cfg.h"
#include "rtxt.h"
#include "mmusic.h"
#include "util.h"
#include "config.h"


static void dvb_init (void);
static gint dvb_is_our_file (gchar *);
static void dvb_play (InputPlayback *);
static void dvb_stop (InputPlayback *);
static void dvb_pause (InputPlayback *, gshort);
static gint dvb_get_time (InputPlayback *);
static void dvb_file_info_box (gchar * s);
static void dvb_exit (void);

static void dvb_pes_pkt (InputPlayback *, guchar *, gint, gint);
static void dvb_payload (InputPlayback *, guchar *, gint, gint);
static void dvb_mpeg_frame (InputPlayback *, guchar *, gint, gint);
static void dvb_close_record (void);
static gchar *dvb_build_file_title (void);

// Thread functions
static gpointer feed_thread (gpointer);
static gpointer get_name_thread (gpointer);
static gpointer epg_thread (gpointer);
static gpointer mmusic_thread (gpointer);
static gpointer dvb_status_thread (gpointer);

// Timer functions
static gboolean infobox_timer (gpointer data);


// Miscellaneous globals
gboolean playing = FALSE, paused = FALSE;
gpointer hlog = NULL;		// This is used everywhere :)
gpointer hdvb = NULL;		// EPG retrieval uses this

// Internal interfaces
cfgstruct *config = NULL;
mmstruct *mmusic = NULL;
rtstruct *rt = NULL;
epgstruct *epg = NULL;
statstruct *station = NULL;
tunestruct *tune = NULL;
dvbstatstruct *dvbstat = NULL;

static gint audio = 0, file_index = 0, frm_ctr = 0;
static gchar erfn[MAXPATHLEN];
static gchar *title = NULL;
static FILE *rec_file = NULL;
static time_t t_start = 0, isplit_last = 0;

// Threads
static GThread *gt_feed = NULL;
static GThread *gt_get_name = NULL;
static GThread *gt_epg = NULL;
static GThread *gt_mmusic = NULL;
static GThread *gt_dvbstat = NULL;

// Timers
static guint infobox_timer_id = 0;

static gint sap = 0;
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

InputPlugin dvb_ip = {
  .description = "DVB Input Plugin",
  .init = dvb_init,
  .about = dvb_about,
  .configure = dvb_configure,
  .is_our_file = dvb_is_our_file,
  .play_file = dvb_play,
  .stop = dvb_stop,
  .pause = dvb_pause,
  .get_time = dvb_get_time,
  .cleanup = dvb_exit,
  .file_info_box = dvb_file_info_box,
};

InputPlugin *dvb_iplist[] = { &dvb_ip, NULL };

DECLARE_PLUGIN (dvb, NULL, NULL, dvb_iplist, NULL, NULL, NULL, NULL, NULL);


static void
dvb_file_info_box (gchar * s)
{
  // Show infobox
  infobox_show (station, rt, epg, mmusic);
  // Register infobox timer
  if (infobox_is_visible () && playing && infobox_timer_id == 0)
    infobox_timer_id = g_timeout_add (1000, infobox_timer, NULL);
}


static void
dvb_init (void)
{
  if (config == NULL)
    config = config_init ();
  config_from_db (config);

  if (log_open (&hlog, "auddacious-dvb", config->loglvl) != RC_OK)
    hlog = NULL;

  log_print (hlog, LOG_INFO, "logging started");

  if (!g_thread_supported ())
    g_thread_init (NULL);

  aud_uri_set_plugin ("dvb://", &dvb_ip);
}


static void
dvb_exit (void)
{
  log_print (hlog, LOG_INFO, "shutting down");
  if (mmusic != NULL)
    {
      madmusic_exit (mmusic);
      mmusic = NULL;
    }
  if (dvbstat != NULL)
    {
      g_free (dvbstat);
      dvbstat = NULL;
    }
  if (rt != NULL)
    {
      radiotext_exit (rt);
      rt = NULL;
    }
  if (epg != NULL)
    {
      epg_exit (epg);
      epg = NULL;
    }
  if (station != NULL)
    {
      g_free (station);
      station = NULL;
    }
  if (tune != NULL)
    {
      g_free (tune);
      tune = NULL;
    }
  if (hdvb != NULL)
    {
      dvb_close (hdvb);
      hdvb = NULL;
    }
  if (config != NULL)
    {
      config_exit (config);
      config = NULL;
    }
  log_print (hlog, LOG_INFO, "logging stopped");
  if (hlog != NULL)
    {
      log_close (hlog);
      hlog = NULL;
    }
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
  gint rc;
  gchar tfn[MAXPATHLEN];
  struct stat st;

  if (playing)
    return;

  log_print (hlog, LOG_DEBUG, "dvb_play(\"%s\");", playback->filename);

  // Update info box
  if (infobox_is_visible ())
    {
      infobox_update_service (NULL);
      infobox_update_radiotext (NULL);
      infobox_update_epg (NULL);
      infobox_update_mmusic (NULL);
      infobox_update_dvb (NULL, NULL, NULL);
      infobox_redraw ();
      // Register infobox timer
      if (infobox_timer_id == 0)
	infobox_timer_id = g_timeout_add (1000, infobox_timer, NULL);
    }

  // Reset playback structures
  playing = TRUE;
  t_start = frm_ctr = sap = file_index = 0;
  memset (sumarr, 0x00, sizeof (sumarr));
  if (rec_file == NULL)
    memset (erfn, 0x00, sizeof (erfn));
  if (config->isplit || config->vsplit)
    {
      if (index (config->rec_fname, '%'))
	{
	  g_sprintf (tfn, config->rec_fname, file_index);
	  while (stat (tfn, &st) >= 0)
	    g_sprintf (tfn, config->rec_fname, ++file_index);
	}
    }

  // Open DVB device
  if ((hdvb = dvb_open (config->devno)) == NULL)
    {
      playing = FALSE;
      return;
    }

  // Initialize tuning information
  tune = g_malloc0 (sizeof (tunestruct));
  if ((rc = dvb_parse_url (playback->filename, tune)) != RC_OK)
    {
      log_print (hlog, LOG_INFO, "dvb_parse_url() returned rc = %d", rc);
      return;
    }

  // Tune DVB device to stations's frequency
  if ((rc = dvb_tune (hdvb, tune)) != RC_OK)
    {
      playing = FALSE;
      g_free (tune);
      dvb_close (hdvb);
      hdvb = NULL;
      return;
    }

  // Initialize service info
  if (station == NULL)
    station = g_malloc0 (sizeof (statstruct));
  g_free (station->svc_name);
  station->svc_name = g_strdup (playback->filename);

  // Get audio PIDs from SID
  if ((rc =
       dvb_get_pid (hdvb, tune->sid, &(tune->apid), &(tune->dpid))) != RC_OK)
    {
      log_print (hlog, LOG_WARNING, "dvb_get_pid() returned %d.", rc);
      playing = FALSE;
      g_free (tune);
      dvb_close (hdvb);
      hdvb = NULL;
      return;
    }

  // Get station and provider name
  if ((gt_get_name =
       g_thread_create (get_name_thread, (gpointer) & tune->sid, TRUE,
			NULL)) == NULL)
    log_print (hlog, LOG_WARNING, "Failed to start dvb_get_name() thread");

  // Set audio PES-filter
  if ((rc = dvb_apid (hdvb, tune->apid)) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_apid() returned %d.", rc);
      playing = FALSE;
      g_free (tune);
      dvb_close (hdvb);
      hdvb = NULL;
      return;
    }

  // Initialize MPEG decoder
  lame_decode_init ();

  // Initialize MadMusic info retrieval
  if ((tune->dpid > 0) && config->info_mmusic)
    {
      log_print (hlog, LOG_INFO,
		 "Data service associated on PID %d (0x%04x).", tune->dpid,
		 tune->dpid);

      if ((rc = dvb_dpid (hdvb, tune->dpid)) == RC_OK)
	{
	  if ((gt_mmusic =
	       g_thread_create (mmusic_thread, 0, TRUE, NULL)) == NULL)
	    log_print (hlog, LOG_ERR,
		       "g_thread_create() failed for mmusic_thread()");
	}
      else
	log_print (hlog, LOG_ERR, "dvb_dpid() returned %d.", rc);
    }

  // Initialize EPG info retrieval
  if (config->info_epg)
    {
      if ((gt_epg = g_thread_create (epg_thread, (gpointer) & tune->sid, TRUE,
				     NULL)) == NULL)
	log_print (hlog, LOG_ERR,
		   "g_thread_create() failed for epg_thread()");
    }

  // Initialize DVB status info retrieval (signal strength, ...)
  if (config->info_dvbstat)
    {
      if ((gt_dvbstat = g_thread_create (dvb_status_thread, NULL, TRUE,
					 NULL)) == NULL)
	log_print (hlog, LOG_ERR,
		   "g_thread_create() failed for dvb_status_thread()");
    }

  // Initialize audio packet retrieval (including Radiotext info)
  if ((gt_feed = g_thread_self ()) == NULL)
    {
      log_print (hlog, LOG_CRIT, "g_thread_self() failed for dvb_play()");
      dvb_stop (playback);
      return;
    }
  playback->set_pb_ready (playback);
  feed_thread (playback);
}


static void
dvb_stop (InputPlayback * playback)
{
  if (playing)
    {
      playing = paused = FALSE;

      // Stop all threads
      if (gt_feed != NULL)
	{
	  log_print (hlog, LOG_INFO, "Waiting for feed_thread() to die...");
	  g_thread_join (gt_feed);
	  gt_feed = NULL;
	}
      if (gt_dvbstat != NULL)
	{
	  log_print (hlog, LOG_INFO,
		     "Waiting for dvb_status_thread to die...");
	  g_thread_join (gt_dvbstat);
	  gt_dvbstat = NULL;
	}
      if (gt_get_name != NULL)
	{
	  log_print (hlog, LOG_INFO,
		     "Waiting for get_name_thread thread to die...");
	  g_thread_join (gt_get_name);
	  gt_get_name = NULL;
	}
      if (gt_mmusic != NULL)
	{
	  log_print (hlog, LOG_INFO,
		     "Waiting for mmusic_thread thread to die...");
	  g_thread_join (gt_mmusic);
	  gt_mmusic = NULL;
	}
      if (gt_epg != NULL)
	{
	  log_print (hlog, LOG_INFO, "Waiting for epg_thread() to die...");
	  g_thread_join (gt_epg);
	  gt_epg = NULL;
	}

      // Close output plugin
      playback->output->close_audio ();

      g_free (station);
      station = NULL;

      // Remove info box timer
      if (infobox_timer_id != 0)
	{
	  g_source_remove (infobox_timer_id);
	  infobox_timer_id = 0;
	}
      // Update info box
      if (infobox_is_visible ())
	{
	  infobox_update_service (NULL);
	  infobox_update_radiotext (NULL);
	  infobox_update_epg (NULL);
	  infobox_update_mmusic (NULL);
	  infobox_update_dvb (NULL, NULL, NULL);
	  infobox_redraw ();
	}

      if (hdvb != NULL)
	{
	  dvb_unfilter (hdvb);
	  dvb_close (hdvb);
	  hdvb = NULL;
	}

      if (rec_file)
	dvb_close_record ();
    }
  log_print (hlog, LOG_DEBUG, "dvb_stop() finished");
}


static void
dvb_pause (InputPlayback * playback, gshort i)
{
  paused = (i == 0 ? FALSE : TRUE);
  playback->output->pause (i);
}


static gint
dvb_get_time (InputPlayback * playback)
{
  if (playing)
    return (playback->output->output_time ());

  return 0;
}


static gpointer
feed_thread (gpointer args)
{
  gint rc, ar, toctr;
  guchar pkt[3840];
  InputPlayback *playback;

  playback = (InputPlayback *) args;
  log_print (hlog, LOG_INFO, "feed_thread() starting");

  static GStaticMutex gmt_feed = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&gmt_feed);

  if (config->info_rt)
    rt = radiotext_init ();

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
	      log_print (hlog, LOG_DEBUG, "dvb_apkt() timeout", rc);
	      if (toctr > 9)
		{
		  playing = FALSE;
		  log_print (hlog, LOG_DEBUG,
			     "dvb_apkt() timed out too often, giving up", rc);
		}
	    }
	  else
	    {
	      log_print (hlog, LOG_ERR,
			 "dvb_apkt() returned rc = %d, giving up", rc);
	      playing = FALSE;
	    }
	}
    }

  log_print (hlog, LOG_DEBUG, "play-loop terminated");

  t_start = 0;

  if (rec_file != NULL)
    dvb_close_record ();

  playback->output->close_audio ();
  audio = 0;

  if (rt != NULL)
    {
      radiotext_exit (rt);
      rt = NULL;
    }

  log_print (hlog, LOG_INFO, "feed_thread() stopping");

  g_static_mutex_unlock (&gmt_feed);
  return NULL;
}


static gpointer
dvb_status_thread (gpointer args)
{
  log_print (hlog, LOG_INFO, "dvb_status_thread() starting");

  static GStaticMutex gmt_dvbstat = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&gmt_dvbstat);

  dvbstat = g_malloc0 (sizeof (dvbstatstruct));

  while (playing)
    {
      dvb_get_status (hdvb, dvbstat);
      g_usleep (750000);
    }

  if (dvbstat != NULL)
    {
      g_free (dvbstat);
      dvbstat = NULL;
    }

  log_print (hlog, LOG_INFO, "dvb_status_thread() stopping");

  g_static_mutex_unlock (&gmt_dvbstat);
  g_thread_exit (0);
  return NULL;
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

			  g_memmove (pesbuf,
				     &pesbuf[i + 6 + PES_packet_length],
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
		  if (rt != NULL)
		    radiotext_read_data (rt, mpbuf, fl);
		  g_memmove (mpbuf, &mpbuf[fl], bph - fl);
		  bph -= fl;
		}
	      else
		{
		  g_memmove (mpbuf, &mpbuf[i], bph - i);
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
	      g_memmove (mpbuf, &mpbuf[i], bph - i);
	      bph -= i;
	    }
	  else
	    /* No sync in buffer yet, that hurts */
	    return;
	}
    }
}


static gchar *
dvb_build_file_title (void)
{
  if (station == NULL)
    return NULL;

  // Title consists of:
  gchar *title;
  // (1) station name
  title = g_strdup (station->svc_name);

  // (2a) Radiotext info
  if (rt && rt->title != NULL)
    {
      gchar *tmp = title;
      if (rt->artist != NULL)
	title = g_strconcat (title, ": ", rt->artist, " - ", rt->title, NULL);
      else
	title = g_strconcat (title, ": ", rt->title, NULL);
      g_free (tmp);
      return title;
    }

  // (2b) MadMusic info
  if (mmusic && mmusic->title != NULL)
    {
      gchar *tmp = title;
      if (mmusic->artist != NULL)
	title =
	  g_strconcat (title, ": ", mmusic->artist, " - ", mmusic->title,
		       NULL);
      else
	title = g_strconcat (title, ": ", mmusic->title, NULL);
      g_free (tmp);
      return title;
    }

  return title;
}


static void
dvb_mpeg_frame (InputPlayback * playback, guchar * frame, gint len, gint smp)
{
  gint nout, i, vu, ms;
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
	      rec_file = fopen (erfn, "ab");
	    }
	  else
	    {
	      log_print (hlog, LOG_INFO, "opening record \"%s\"", erfn);
	      rec_file = fopen (erfn, "wb");
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
	fwrite (frame, sizeof (unsigned char), len, rec_file);
    }

  nout = lame_decode_headers (frame, len, left, right, &mp3d);
  if (nout <= 0)
    return;

  if (mp3d.header_parsed == 1)
    {
      gchar *newtitle;
      if (audio == 0)
	audio = playback->output->open_audio (FMT_S16_NE, mp3d.samplerate, 2);

      newtitle = dvb_build_file_title ();
      if (title == NULL
	  || (newtitle != NULL && strcmp (newtitle, title) != 0))
	{
	  dvb_ip.set_info (aud_str_to_utf8 (newtitle), -1,
			   mp3d.bitrate * 1000, mp3d.samplerate, mp3d.stereo);
	  if (title)
	    g_free (title);
	  title = newtitle;
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

  playback->pass_audio (playback, FMT_S16_NE, 2, nout << 2, stereo, NULL);

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

      g_memmove (sumarr, &sumarr[1], sizeof (int) * (sap - 1));
      sap--;
    }
}


void
dvb_close_record (void)
{
  if (rec_file != NULL)
    {
      log_print (hlog, LOG_INFO, "closing record \"%s\"", erfn);
      fclose (rec_file);
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

  static GStaticMutex gmt_get_name = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&gmt_get_name);

  sct = 0;

  while (playing)
    {
      // for details see ETSI TR 101 211: "Digital Video Broadcasting (DVB);
      // Guidelines on implementation and usage of Service Information (SI)"
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

		  if (dt == 0x48)	// service_descriptor (SDT, SIT)
		    {
		      memcpy (prov, &pp[2], pp[1]);
		      prov[pp[1]] = '\0';
		      if (is_updated (prov, &station->prov_name, FALSE))
			station->refresh = TRUE;

		      memcpy (name, &pp[3 + pp[1]], pp[2 + pp[1]]);
		      name[pp[2 + pp[1]]] = '\0';
		      if (is_updated (name, &station->svc_name, FALSE))
			station->refresh = TRUE;

		      if (station->refresh)
			log_print (hlog, LOG_INFO,
				   "Service name: \"%s\", \"%s\"", prov,
				   name);

		      log_print (hlog, LOG_INFO,
				 "get_name_thread() stopping");

		      g_static_mutex_unlock (&gmt_get_name);
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

  g_static_mutex_unlock (&gmt_get_name);
  g_thread_exit (0);

  return NULL;
}


static gpointer
epg_thread (gpointer arg)
{
  gint sid, rc, len, sct;
  guchar s[4096];

  log_print (hlog, LOG_INFO, "epg_thread() starting");

  static GStaticMutex gmt_epg = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&gmt_epg);

  sid = *((gint *) arg);
  log_print (hlog, LOG_DEBUG, "EPG SID: %d (0x%04x)", sid, sid);

  // Make sure EPG retrieval is initialized
  if ((epg = epg_init ()) == NULL)
    {
      g_static_mutex_unlock (&gmt_epg);
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

  epg_exit (epg);
  epg = NULL;

  log_print (hlog, LOG_INFO, "epg_thread() stopping");

  g_static_mutex_unlock (&gmt_epg);
  g_thread_exit (0);
  return NULL;
}


static gpointer
mmusic_thread (gpointer arg)
{
  gint rc, dr, slen, blen, off, fbf;
  guchar sect[5120], rtxt[32768];

  log_print (hlog, LOG_INFO, "mmusic_thread() starting");

  static GStaticMutex gmt_mmusic = G_STATIC_MUTEX_INIT;
  g_static_mutex_lock (&gmt_mmusic);

  /* Make sure information retrieval is initialized */
  if ((mmusic = madmusic_init ()) == NULL)
    {
      g_static_mutex_unlock (&gmt_mmusic);
      g_thread_exit (0);
      return NULL;
    }

  fbf = off = 0;

  while (playing)
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

  g_static_mutex_unlock (&gmt_mmusic);
  g_thread_exit (0);
  return NULL;
}


static gboolean
infobox_timer (gpointer data)
{
  log_print (hlog, LOG_DEBUG, "infobox_timer() starting");

  if (!infobox_is_visible ())
    {
      infobox_timer_id = 0;
      return FALSE;
    }

  gboolean refreshed = FALSE;

  if (station != NULL && station->refresh)
    {
      log_print (hlog, LOG_DEBUG, "Station info changed!");
      infobox_update_service (station);
      station->refresh = FALSE;
      refreshed = TRUE;
    }
  if (rt != NULL && rt->refresh)
    {
      log_print (hlog, LOG_DEBUG, "Radiotext info changed!");
      infobox_update_radiotext (rt);
      rt->refresh = FALSE;
      refreshed = TRUE;
    }
  if (epg != NULL && epg->refresh)
    {
      log_print (hlog, LOG_DEBUG, "EPG info changed!");
      infobox_update_epg (epg);
      epg->refresh = FALSE;
      refreshed = TRUE;
    }
  if (mmusic != NULL && mmusic->refresh)
    {
      log_print (hlog, LOG_DEBUG, "MadMusic info changed!");
      infobox_update_mmusic (mmusic);
      mmusic->refresh = FALSE;
      refreshed = TRUE;
    }
  if (dvbstat != NULL && dvbstat->refresh)
    {
      infobox_update_dvb (hdvb, dvbstat, tune);
      dvbstat->refresh = FALSE;
      refreshed = TRUE;
    }

  if (refreshed)
    infobox_redraw ();

  log_print (hlog, LOG_DEBUG, "infobox_timer() stopping");
  return TRUE;
}
