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

#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#ifdef HAVE_AUDACIOUS_MISC_H
#include <audacious/misc.h>
#endif
#include <audacious/plugin.h>
#include <mad.h>

#include "gui.h"
#include "epg.h"
#include "dvb.h"
#include "log.h"
#include "cfg.h"
#include "rtxt.h"
#include "mmusic.h"
#include "record.h"
#include "util.h"

#define MAX_DVB_TIMEOUT     9

#ifdef __AUDACIOUS_PLUGIN_API__
#define AUD_PLUGIN_API	__AUDACIOUS_PLUGIN_API__
#else
#ifdef _AUD_PLUGIN_VERSION
#define AUD_PLUGIN_API	_AUD_PLUGIN_VERSION
#else
#error "Unable to detect version of plugin API. Aborting."
#endif
#endif

#if _AUD_PLUGIN_VERSION < 19
static void dvb_init (void);
static gint dvb_is_our_file (const gchar *);
static void dvb_play_file (InputPlayback *);
static void dvb_pause (InputPlayback *, gshort);
#else
static gboolean dvb_init (void);
static gint dvb_is_our_file_from_vfs (const gchar *, VFSFile *);
static void dvb_pause (InputPlayback *, gboolean);
#endif
static gboolean dvb_play (InputPlayback *, const gchar *, VFSFile *, gint, gint, gboolean);
static void dvb_stop (InputPlayback *);
static gint dvb_get_time (InputPlayback *);
static void dvb_file_info_box (const gchar *);
static void dvb_exit (void);

static gboolean dvb_pes_pkt (InputPlayback *, const guchar *, gint, gint);
static gboolean dvb_payload (InputPlayback *, const guchar *, gint, gint);
static gboolean write_output (InputPlayback *, const struct mad_pcm *,
			      const struct mad_header *);
static gboolean dvb_mpeg_frame (InputPlayback *, const guchar *, guint);

// Thread functions
static gboolean feed_thread (InputPlayback *);
static gpointer get_name_thread (gpointer);
static gpointer epg_thread (gpointer);
static gpointer mmusic_thread (gpointer);

// Mutexes
static GStaticMutex gmt_feed = G_STATIC_MUTEX_INIT;
static GStaticMutex gmt_get_name = G_STATIC_MUTEX_INIT;
static GStaticMutex gmt_epg = G_STATIC_MUTEX_INIT;
static GStaticMutex gmt_mmusic = G_STATIC_MUTEX_INIT;

// Timer functions
static gboolean infobox_timer (gpointer);
static gboolean dvb_status_timer (gpointer);


// Title/Tuple functions
#if AUD_PLUGIN_API < 12
static gchar * build_file_title (void);
#else
static gboolean update_tuple_str (Tuple *, gint, const gchar *);
static gboolean update_tuple_int (Tuple *, gint, gint);
static gboolean update_tuple (Tuple*, const struct mad_header,
                              const statstruct*, const rtstruct*, const mmstruct*);
#endif


// Miscellaneous globals
static gboolean playing = FALSE, paused = FALSE;
gpointer hlog = NULL;
static gpointer hdvb = NULL;

// Internal interfaces
cfgstruct *config = NULL;
static mmstruct *mmusic = NULL;
static rtstruct *rt = NULL;
static epgstruct *epg = NULL;
static statstruct *station = NULL;
static tunestruct *tune = NULL;
static dvbstatstruct *dvbstat = NULL;
static recstruct *record = NULL;
static Tuple *tuple = NULL;

// Threads
static GThread *gt_get_name = NULL;
static GThread *gt_epg = NULL;
static GThread *gt_mmusic = NULL;

// Timers
static guint infobox_timer_id = 0;
static guint dvb_status_timer_id = 0;

// Audio stuff
static struct mad_stream madstream;
static struct mad_frame madframe;
static struct mad_synth madsynth;
static gint sap = 0;
static gdouble sumarr[512];
static gboolean audio_opened = FALSE;
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

// Recording stuff
static time_t isplit_last = 0;
static time_t vsplit_last = 0;

static InputPlugin dvb_ip = {
  .description = "DVB Input Plugin",
  .init = dvb_init,
  .about = dvb_about,
  .configure = dvb_configure,
  .stop = dvb_stop,
  .pause = dvb_pause,
  .get_time = dvb_get_time,
  .cleanup = dvb_exit,
  .file_info_box = dvb_file_info_box,
#if AUD_PLUGIN_API >= 16
  .play = dvb_play,
#endif
#if AUD_PLUGIN_API < 18
  .is_our_file = dvb_is_our_file,
  .play_file = dvb_play_file,
#else
  .is_our_file_from_vfs  = dvb_is_our_file_from_vfs,
#endif
};

static InputPlugin *dvb_iplist[] = { &dvb_ip, NULL };

SIMPLE_INPUT_PLUGIN (dvb, dvb_iplist);


static void
dvb_file_info_box (const gchar * s)
{
  // Show infobox
  infobox_show (station, rt, epg, mmusic);
  // Register infobox timer
  if (infobox_is_visible () && playing && infobox_timer_id == 0)
    infobox_timer_id = g_timeout_add (1000, infobox_timer, NULL);
}


#if AUD_PLUGIN_API < 19
static void
dvb_init (void)
#else
static gboolean
dvb_init (void)
#endif
{
  config = config_init ();
  config_from_db (config);

  // Check if we want to log to file or to glib
  if (config->log_tofile && config->log_filename != NULL)
    {
      // Try file logging
      if (log_file_open
	  (&hlog, config->log_filename, config->log_append,
	   config->log_level) != RC_OK)
	{
	  // Fall back to glib logging
	  if (log_glib_open
	      (&hlog, "auddacious-dvb", config->log_level) == RC_OK)
	    log_print (hlog, LOG_INFO,
		       "Logging to file %s failed, falling back to GLib logging",
		       config->log_filename);
	  else
	    hlog = NULL;
	}
    }
  else
    {
      // Try glib logging
      if (log_glib_open (&hlog, "auddacious-dvb", config->log_level) != RC_OK)
	hlog = NULL;
    }

  log_print (hlog, LOG_INFO, "logging started");

  aud_uri_set_plugin ("dvb://", &dvb_ip);
#if _AUD_PLUGIN_VERSION >= 19
  return TRUE;
#endif
}


static void
dvb_exit (void)
{
  log_print (hlog, LOG_INFO, "shutting down");
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


#if AUD_PLUGIN_API < 19
static gint
dvb_is_our_file (const gchar * filename)
#else
static gint
dvb_is_our_file_from_vfs (const gchar * filename, VFSFile * file)
#endif
{
  gint rc;

  if ((rc = dvb_tune_parse_url (filename, NULL)) == RC_OK)
    return 1;

  log_print (hlog, LOG_DEBUG, "dvb_parse_url() returned rc = %d", rc);
  return 0;
}


static gboolean
dvb_play (InputPlayback * playback, const gchar * filename, VFSFile * file,
	gint start_time, gint stop_time, gboolean pause)
{
  gint rc;

  if (playing)
    return FALSE;

  log_print (hlog, LOG_INFO, "dvb_play(\"%s\");", filename);

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
  isplit_last = vsplit_last = sap = 0;
  memset (sumarr, 0x00, sizeof (sumarr));

  // Open DVB device
  if ((hdvb = dvb_open (config->devno)) == NULL)
    {
      log_print (hlog, LOG_ERR, "dvb_open() failed.");
      playing = FALSE;
      return FALSE;
    }

  // Initialize tuning information
  tune = dvb_tune_init ();
  if ((rc = dvb_tune_parse_url (filename, tune)) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_parse_url() returned %d.", rc);
      playing = FALSE;
      dvb_tune_exit (tune);
      tune = NULL;
      dvb_close (hdvb);
      hdvb = NULL;
      return FALSE;
    }

  // Tune DVB device to stations's frequency
  if ((rc = dvb_tune (hdvb, tune)) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_tune() returned %d.", rc);
      playing = FALSE;
      dvb_tune_exit (tune);
      tune = NULL;
      dvb_close (hdvb);
      hdvb = NULL;
      return FALSE;
    }

  // Initialize service info
  station = g_malloc0 (sizeof (statstruct));
  station->svc_name = g_strdup (filename);

  // Get audio PIDs from SID
  if ((rc =
       dvb_get_pid (hdvb, tune->sid, &(tune->apid), &(tune->dpid))) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_get_pid() returned %d.", rc);
      playing = FALSE;
      dvb_tune_exit (tune);
      tune = NULL;
      dvb_close (hdvb);
      hdvb = NULL;
      return FALSE;
    }

  // Get station and provider name
  if ((gt_get_name =
       g_thread_create (get_name_thread, (gpointer) & tune->sid, TRUE,
			NULL)) == NULL)
    log_print (hlog, LOG_WARN, "Failed to start dvb_get_name() thread.");

  // Set audio PES-filter
  if ((rc = dvb_apid (hdvb, tune->apid)) != RC_OK)
    {
      log_print (hlog, LOG_ERR, "dvb_apid() returned %d.", rc);
      playing = FALSE;
      dvb_tune_exit (tune);
      tune = NULL;
      dvb_close (hdvb);
      hdvb = NULL;
      return FALSE;
    }

  // Initialize recording
  if (config->rec_onplay)
    {
      record = record_init ();
      if (record != NULL)
	{
	  if (record_open
	      (record, config->rec_fname, config->rec_append,
	       config->rec_overwrite) != TRUE)
	    {
	      log_print (hlog, LOG_WARN, "record_open(%s, %d) failed.",
			 config->rec_fname, config->rec_append);
	      record_exit (record);
	      record = NULL;
	    }

	  // Last splitting (=start time) occured now
	  if (config->isplit)
	    time (&isplit_last);
	  if (config->vsplit)
	    time (&vsplit_last);
	}
      else
	log_print (hlog, LOG_WARN, "record_init() failed.");
    }

  // Initialize Radiotext retrieval
  if (config->info_rt)
    {
      rt = radiotext_init ();
      if (rt == NULL)
	log_print (hlog, LOG_WARN, "radiotext_init() failed.");
    }


  // Initialize MadMusic info retrieval
  if (tune->dpid > 0 && config->info_mmusic)
    {
      log_print (hlog, LOG_INFO,
		 "Data service associated on PID %d (0x%04x).", tune->dpid,
		 tune->dpid);

      if ((rc = dvb_dpid (hdvb, tune->dpid)) == RC_OK)
	{
	  if ((gt_mmusic =
	       g_thread_create (mmusic_thread, 0, TRUE, NULL)) == NULL)
	    log_print (hlog, LOG_WARN,
		       "g_thread_create() failed for mmusic_thread().");
	}
      else
	log_print (hlog, LOG_WARN, "dvb_dpid() returned %d.", rc);
    }

  // Initialize EPG info retrieval
  if (config->info_epg)
    {
      if ((gt_epg = g_thread_create (epg_thread, (gpointer) & tune->sid, TRUE,
				     NULL)) == NULL)
	log_print (hlog, LOG_WARN,
		   "g_thread_create() failed for epg_thread().");
    }

  // Initialize DVB status info retrieval (signal strength, ...)
  if (config->info_dvbstat)
    {
      dvbstat = dvb_status_init ();
      if (dvb_status_timer_id == 0)
	dvb_status_timer_id = g_timeout_add (750, dvb_status_timer, NULL);
    }

#if AUD_PLUGIN_API >= 12
  // Initialize tuple info
  tuple = tuple_new_from_filename (filename);
#endif

  // Initialize MPEG decoder
  mad_frame_init (&madframe);
  mad_synth_init (&madsynth);
  mad_stream_init (&madstream);
  mad_stream_options (&madstream, MAD_OPTION_IGNORECRC);

  // Initialize audio packet retrieval (including Radiotext info)
#if AUD_PLUGIN_API < 19
  if (g_thread_self () == NULL)
    {
      log_print (hlog, LOG_CRIT, "g_thread_self() failed for dvb_play().");
      dvb_stop (playback);
      return FALSE;
    }
#endif

  return feed_thread (playback);
}


#if AUD_PLUGIN_API < 19
static void
dvb_play_file (InputPlayback * playback)
{
  dvb_play (playback, playback->filename, NULL, 0, 0, FALSE);
}
#endif


static void
dvb_stop (InputPlayback * playback)
{
  if (playing)
    {
      playing = paused = FALSE;
#if AUD_PLUGIN_API > 15
      playback->output->abort_write();
#endif

      // Stop all helper threads
      if (dvb_status_timer_id != 0)
	{
	  log_print (hlog, LOG_INFO, "Removing dvb_status_timer()...");
	  g_source_remove (dvb_status_timer_id);
	  dvb_status_timer_id = 0;
	}
      if (gt_get_name != NULL)
	{
	  log_print (hlog, LOG_INFO,
		     "Waiting for get_name_thread() to die...");
          g_thread_join (gt_get_name);
	  gt_get_name = NULL;
	}
      if (gt_mmusic != NULL)
	{
	  log_print (hlog, LOG_INFO,
		     "Waiting for mmusic_thread() to die...");
	  g_thread_join (gt_mmusic);
	  gt_mmusic = NULL;
	}
      if (gt_epg != NULL)
	{
	  log_print (hlog, LOG_INFO, "Waiting for epg_thread() to die...");
	  g_thread_join (gt_epg);
	  gt_epg = NULL;
	}

      // Stop feed thread
      log_print (hlog, LOG_INFO, "Waiting for feed_thread() to die...");
      g_static_mutex_lock (&gmt_feed);
      g_static_mutex_unlock (&gmt_feed);

      // Stop infobox timer
      if (infobox_timer_id != 0)
	{
	  log_print (hlog, LOG_DEBUG, "Removing infobox_timer()...");
	  g_source_remove (infobox_timer_id);
	  infobox_timer_id = 0;
	}

      // Free Radiotext structures
      if (rt != NULL)
	{
	  radiotext_exit (rt);
	  rt = NULL;
	}
      // Free record structures
      if (record != NULL)
	{
	  record_exit (record);
	  record = NULL;
	}
      // Free station info
      if (station != NULL)
	{
	  g_free (station);
	  station = NULL;
	}
      // Free DVB stuff
      if (dvbstat != NULL)
	{
	  dvb_status_exit (dvbstat);
	  dvbstat = NULL;
	}
      // Free tuple info
      if (tuple != NULL)
        {
          tuple_free (tuple)
          tuple = NULL;
        }
      // Free tuning structure
      if (tune != NULL)
	{
	  dvb_tune_exit (tune);
	  tune = NULL;
	}
      // Free DVB interface
      if (hdvb != NULL)
	{
	  dvb_unfilter (hdvb);
	  dvb_close (hdvb);
	  hdvb = NULL;
	}

      // Free MPEG decoder stuff
      mad_synth_finish (&madsynth);
      mad_frame_finish (&madframe);
      mad_stream_finish (&madstream);

      // Close output plugin
      if (audio_opened)
	{
	  playback->output->close_audio ();
	  audio_opened = FALSE;
	}
    }
  log_print (hlog, LOG_INFO, "dvb_stop() finished.");
}


#if AUD_PLUGIN_API < 19
static void
dvb_pause (InputPlayback * playback, gshort i)
#else
static void
dvb_pause (InputPlayback * playback, gboolean _paused)
#endif
{
  if (config->rec_onpause)
    {
      // Pause toggles recording
      if (record != NULL)
	{
          // Stop recording
	  record_exit (record);
	  record = NULL;
          return;
	}
      // Start recording
      record = record_init ();
      if (record != NULL)
	{
	  if (record_open
	      (record, config->rec_fname, config->rec_append,
	       config->rec_overwrite) != TRUE)
	    {
	      log_print (hlog, LOG_WARN, "record_open(%s, %d) failed.",
			 config->rec_fname, config->rec_append);
	      record_exit (record);
	      record = NULL;
	    }

	  // Last splitting (=start time) occured now
	  if (config->isplit)
	    time (&isplit_last);
	  if (config->vsplit)
	    time (&vsplit_last);
	}
      else
	log_print (hlog, LOG_WARN, "record_init() failed.");
    }
  else
    {
      // 'Real' pause
#if AUD_PLUGIN_API < 19
      paused = (i == 0 ? FALSE : TRUE);
      playback->output->pause (i);
#else
      paused = _paused;
      playback->output->pause (_paused);
#endif
    }
}


static gint
dvb_get_time (InputPlayback * playback)
{
  if (playing)
    return playback->output->written_time ();

  return 0;
}


static gboolean
feed_thread (InputPlayback * playback)
{
  gint rc, ar;
  gboolean error = FALSE;
  guint toctr = 0;
  guchar pkt[3840];

  log_print (hlog, LOG_INFO, "feed_thread() starting");

  // Prevent feed_thread from being called twice
  g_static_mutex_lock (&gmt_feed);

  // Initialize stream demuxer
  dvb_pes_pkt (playback, NULL, 0, 1);
  dvb_payload (playback, NULL, 0, 1);

  // Main decoding loop
  while (playing && !error)
    {
      // Try to fetch an audio packet from DVB card
      rc = dvb_apkt (hdvb, pkt, sizeof (pkt), 1000, &ar);
      switch (rc)
        {
        case RC_OK:
          toctr = 0;
          if (!paused && !dvb_pes_pkt (playback, pkt, ar, 0))
	    {
	      log_print (hlog, LOG_ERR,
			 "dvb_pes_pkt() failed in feed_thread()");
	      error = TRUE;
	    }
          break;
        case RC_DVB_TIMEOUT:
          log_print (hlog, LOG_DEBUG, "dvb_apkt() timeout");
          if (++toctr > MAX_DVB_TIMEOUT)
            {
              log_print (hlog, LOG_DEBUG,
                         "dvb_apkt() timed out too often, giving up");
	      error = TRUE;
            }
          break;
        default:
          log_print (hlog, LOG_ERR,
                     "dvb_apkt() returned rc = %d, giving up", rc);
	  error = TRUE;
        }
      }

  log_print (hlog, LOG_DEBUG, "play-loop terminated");
  playing = FALSE;

  if (audio_opened)
    {
      playback->output->close_audio ();
      audio_opened = FALSE;
    }

  log_print (hlog, LOG_INFO, "feed_thread() stopping");

  g_static_mutex_unlock (&gmt_feed);
  return !error;
}

static gboolean
dvb_status_timer (gpointer args)
{
  if (!playing || hdvb == NULL || dvbstat == NULL)
    {
      log_print (hlog, LOG_DEBUG, "removing dvb_status_timer()");
      dvb_status_timer_id = 0;
      return FALSE;
    }

  dvb_get_status (hdvb, dvbstat);

  return TRUE;
}


static gboolean
dvb_pes_pkt (InputPlayback * playback, const guchar * buf, gint len,
	     gint reset)
{
  gint i, stream_id, PES_packet_length, j, pp_len;
  static gint pbh, pbl;
  guchar *p, *pp;
  static guchar pesbuf[128 * 1024];

  if (reset)
    {
      pbh = pbl = 0;
      memset (pesbuf, 0x00, sizeof (pesbuf));
      return TRUE;
    }

  if ((pbh + len) > sizeof (pesbuf))
    {
      log_print (hlog, LOG_CRIT, "PES buffer overflow imminent, flushing!");
      pbh = pbl = 0;
      memset (pesbuf, 0x00, sizeof (pesbuf));
      return TRUE;
    }

  memcpy (&pesbuf[pbh], buf, len);
  pbh += len;

  while (1)
    {
      if (pbl >= pbh)
	{
          pbl = 0;
	  pbh = 0;
	  return TRUE;
        }
      if ((pbh - pbl) <= 4)
        return TRUE;
      for (i = pbl; i < (pbh - 4); i++)
        {
          if ((pesbuf[i] == 0x00) &&
              (pesbuf[i + 1] == 0x00) &&
              (pesbuf[i + 2] == 0x01) && ((pesbuf[i + 3] >> 4) > 0xa))
            break;
        }

      if (i >= (pbh - 4))
        {
          memcpy (pesbuf, &pesbuf[i], pbh - i);
          pbl = 0;
          pbh -= i;
          return TRUE;
        }
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

              if (j >= (pbh - 4))
                return TRUE;
              PES_packet_length = j - i - 6;
            }

          if ((pbh - pbl) <= (PES_packet_length + 6))
            return TRUE;
          /* Uhmmmm, complete? */
          p = &pesbuf[i];
          pp = p + 9 + p[8];
          pp_len = PES_packet_length - 3 - p[8];
          if (!dvb_payload (playback, pp, pp_len, 0))
	    {
	      log_print (hlog, LOG_ERR,
			 "dvb_payload() failed in dvb_pes_pkt()");
	      return FALSE;
	    }

          g_memmove (pesbuf,
                     &pesbuf[i + 6 + PES_packet_length],
                     pbh - (i + 6 + PES_packet_length));
          pbl = 0;
          pbh -= (i + 6 + PES_packet_length);
        }
    }
  return TRUE;
}


static gboolean
dvb_payload (InputPlayback * playback, const guchar * buf, gint len,
	     gint reset)
{
  gint br, sf, fl, num_samples;
  gint i, mpv, mpl, bri, tlu, sfi, pad;
  static gint bph = 0;
  static guchar mpbuf[128 * 1024 + MAD_BUFFER_GUARD];

  if (reset)
    {
      bph = 0;
      memset (mpbuf, 0x00, sizeof (mpbuf));
      return TRUE;
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
	      if (mpbuf[i] == 0xff && (mpbuf[i + 1] & 0xf0) == 0xf0)
		break;
            }

          if (i >= (bph - 4))
            /* whoops, that sucks */
            return TRUE;

          /* yes, we may have an entire frame */
          mpv = (mpbuf[1] >> 3) & 1;
          mpl = (mpbuf[1] >> 1) & 3;
          bri = (mpbuf[2] >> 4) & 0xf;
          sfi = (mpbuf[2] >> 2) & 3;
          pad = (mpbuf[2] >> 1) & 1;

          tlu = bri | (mpl << 4) | (mpv << 6);
          br = brt[tlu];

          tlu = sfi | (mpv << 2);
          sf = sft[tlu];

          if (sf == 0 || br == 0)
            {
              /* Uhm, no, this is not it */
              memcpy (mpbuf, &mpbuf[i], bph - i);
              bph -= i;
              return TRUE;
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
            return TRUE;

          if ((mpbuf[fl] == 0xff) && ((mpbuf[fl + 1] & 0xf0) == 0xf0))
            {
              if (!dvb_mpeg_frame (playback, mpbuf, fl))
                return FALSE;
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
	{
	  for (i = 1; i < (bph - 4); i++)
	    {
	      if ((mpbuf[i] == 0xff) && ((mpbuf[i + 1] & 0xf0) == 0xf0))
		break;
            }

          if (i >= (bph - 4))
            /* No sync in buffer yet, that hurts */
            return TRUE;

          g_memmove (mpbuf, &mpbuf[i], bph - i);
          bph -= i;
	}
    }
  return TRUE;
}


#if AUD_PLUGIN_API < 12
static gchar *
build_file_title (void)
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

#else

static gboolean
update_tuple_str (Tuple * tuple, gint item, const gchar * newstr)
{
  const gchar * oldstr = tuple_get_string (tuple, item, NULL);
  gboolean changed = (newstr == NULL && oldstr != NULL) ||
          (newstr != NULL && (oldstr == NULL || strcmp (oldstr, newstr)));
  if (changed)
    tuple_associate_string (tuple, item, NULL, newstr);
  return changed;
}


static gboolean
update_tuple_int (Tuple * tuple, gint item, gint newint)
{
  gint oldint = tuple_get_int (tuple, item, NULL);
  gboolean changed = (oldint != newint);
  if (changed)
    tuple_associate_int (tuple, item, NULL, newint);
  return changed;
}


static gboolean
update_tuple (Tuple* tuple, const struct mad_header mh,
    const statstruct* st, const rtstruct* rt, const mmstruct* mm)
{
  gboolean changed = FALSE;
  gchar *stname = NULL;

  // Audio format
  changed = changed || update_tuple_int (tuple, FIELD_LENGTH, -1);
  changed = changed || update_tuple_int(tuple, FIELD_BITRATE, mh.bitrate / 1000);
  changed = changed || update_tuple_str (tuple, FIELD_MIMETYPE, "audio/mpeg");
  changed = changed || update_tuple_str(tuple, FIELD_CODEC, "MPEG Audio (MP2)");
  changed = changed || update_tuple_str(tuple, FIELD_QUALITY, "lossy");

  // Station Name
  if (st && st->svc_name)
    stname = g_strdup (st->svc_name);
  else
    stname = g_strdup ("Unknown");

  // RadioText/MadMusic info
  if (rt && (rt->title != NULL || rt->artist != NULL))
    {
      // Radiotext info available
      changed = changed || update_tuple_str (tuple, FIELD_ALBUM, stname);
      if (rt->title != NULL)
        changed = changed || update_tuple_str (tuple, FIELD_TITLE, rt->title);
      if (rt->artist != NULL)
	stname = g_strconcat (stname, ": ", rt->artist, NULL);
      changed = changed || update_tuple_str (tuple, FIELD_ARTIST, stname);
    }
  else if (mm && (mm->title != NULL || mm->artist != NULL))
    {
      // MadMusic info available
      changed = changed || update_tuple_str (tuple, FIELD_ALBUM, stname);
      if (mm->title != NULL)
        changed = changed || update_tuple_str (tuple, FIELD_TITLE, mm->title);
      if (mm->artist != NULL)
	stname = g_strconcat (stname, ": ", mm->artist, NULL);
      changed = changed || update_tuple_str (tuple, FIELD_ARTIST, stname);
    }
  else
    {
      // Neither RadioText nor MadMusic info available
      changed = changed || update_tuple_str (tuple, FIELD_TITLE, stname);
      changed = changed || update_tuple_str (tuple, FIELD_ARTIST, NULL);
      changed = changed || update_tuple_str (tuple, FIELD_ALBUM, NULL);
    }

  g_free (stname);
  return changed;
}
#endif


static gboolean
write_output (InputPlayback * playback, const struct mad_pcm *pcm,
	      const struct mad_header *header)
{
  guint i, ms, channel, channels = MAD_NCHANNELS (header);
#ifdef HAVE_DECL_FMT_FIXED32
  guint outbyte = sizeof (mad_fixed_t) * channels * pcm->length;
  mad_fixed_t * output = g_malloc (outbyte);
  mad_fixed_t * outend = output + channels * pcm->length;
#else
  guint outbyte = sizeof (gfloat) * channels * pcm->length;
  gfloat * output = g_malloc (outbyte);
  gfloat * outend = output + channels * pcm->length;
#endif
  gdouble vu = 0;

  for (channel = 0; channel < channels; channel++)
    {
      const mad_fixed_t * from = pcm->samples[channel];
#ifdef HAVE_DECL_FMT_FIXED32
      mad_fixed_t * to = output + channel;
#else
      gfloat * to = output + channel;
#endif

      while (to < outend)
        {
#ifdef HAVE_DECL_FMT_FIXED32
          mad_fixed_t sample = (*from++);
          *to = sample;
          vu += fabs (mad_f_todouble (sample));
#else
          gdouble sample = mad_f_todouble (*from++);
          *to = (gfloat) sample;
          vu += fabs (sample);
#endif
          to += channels;
        }
    }
  vu /= channels * pcm->length;

  // Check audio energy level if we need to split files
  if (record != NULL && config->vsplit)
    {
      sumarr[sap++] = vu;
      ms = sap * pcm->length * 1000 / header->bitrate;
      if (ms >= config->vsplit_dur)
	{
	  vu = 0;
	  for (i = 0; i < sap; i++)
	    vu += sumarr[i];
	  vu /= sap;

	  gdouble dB = 20 * log10 (vu);

	  if (dB < config->vsplit_vol)
	    {
	      time_t t;
	      time (&t);
	      if ((t - vsplit_last) >= config->vsplit_minlen)
		{
		  log_print (hlog, LOG_INFO,
			     "Avg: %.2f dB (%d) / %d ms (%d f) / %d:%02d.",
			     dB, vu, ms, sap, (t - vsplit_last) / 60,
			     (t - vsplit_last) % 60);
		  vsplit_last = t;

		  // Split record file
		  record_next (record, config->rec_append,
			       config->rec_overwrite);

		  // Reset energy level structures
		  sap = 1;
		  memset (sumarr, 0x00, sizeof (sumarr));
		}
	    }

	  g_memmove (sumarr, &sumarr[1], sizeof (int) * --sap);
	}
    }

  if (audio_opened)
#ifdef HAVE_DECL_FMT_FIXED32
    playback->pass_audio (playback, FMT_FIXED32, MAD_NCHANNELS (header),
			  outbyte, output, NULL);
#else
    playback->output->write_audio (output, outbyte);
#endif

  g_free (output);
  return audio_opened;
}


static gboolean
dvb_mpeg_frame (InputPlayback * playback, const guchar * frame, guint len)
{
  if (playback == NULL)
    {
      log_print (hlog, LOG_ERR,
		 "dvb_mpeg_frame() called with (playback == NULL)");
      return FALSE;
    }
  if (frame == NULL)
    {
      log_print (hlog, LOG_ERR,
		 "dvb_mpeg_frame() called with (frame == NULL)");
      return FALSE;
    }

  // Only if we're recording to file
  if (record != NULL)
    {
      // Get current time
      time_t t;
      time (&t);

      // Split audio file (in fixed interval mode)
      if (config->isplit && (t - isplit_last) >= config->isplit_ival)
	{
	  record_next (record, config->rec_append, config->rec_overwrite);
	  isplit_last = t;
	}

      // Write MPEG frame to file
      record_write (record, frame, len);
    }

  // pass buffer to libmad
  mad_stream_buffer (&madstream, frame, len + MAD_BUFFER_GUARD);

  // parse frame header
  if (mad_header_decode (&madframe.header, &madstream) == -1)
    {
      if (!MAD_RECOVERABLE (madstream.error))
	{
	  log_print (hlog, LOG_WARN,
		     "unrecovered error decoding header: %s",
		     mad_stream_errorstr (&madstream));
	  return FALSE;
	}

      log_print (hlog, LOG_WARN, "recovered error decoding header: %s",
		 mad_stream_errorstr (&madstream));
    }

  // decode frame
  if (mad_frame_decode (&madframe, &madstream) == -1)
    {
      if (!MAD_RECOVERABLE (madstream.error))
	{
	  log_print (hlog, LOG_WARN,
		     "unrecovered error decoding frame %d: %s",
		     mad_stream_errorstr (&madstream));
	  return FALSE;
	}

      log_print (hlog, LOG_WARN, "recovered error decoding frame %d: %s",
		 mad_stream_errorstr (&madstream));
    }
  mad_synth_frame (&madsynth, &madframe);

  // open audio card (if not already done)
  if (!audio_opened)
    {
#ifdef HAVE_DECL_FMT_FIXED32
      if (!playback->
 	  output->open_audio (FMT_FIXED32, madframe.header.samplerate,
			      MAD_NCHANNELS (&madframe.header)))
#else
      if (!playback->
	  output->open_audio (FMT_FLOAT, madframe.header.samplerate,
			      MAD_NCHANNELS (&madframe.header)))
#endif
	{
#if AUD_PLUGIN_API < 18
	  playback->error = TRUE;
#endif
	  log_print (hlog, LOG_WARN,
		     "open_audio() failed in dvb_mpeg_frame()");
	  return FALSE;
	}

      // Tell Audacious how we're doing
#if AUD_PLUGIN_API < 18
      playback->playing = TRUE;
      playback->error = FALSE;
#endif
      playback->set_pb_ready(playback);

      audio_opened = TRUE;
    }

#if AUD_PLUGIN_API < 12
  // Look if file title has changed
  gchar *newtitle;
  newtitle = build_file_title ();
  if (playback->title == NULL
      || (newtitle != NULL && strcmp (newtitle, playback->title) != 0))
    {
      playback->set_params (playback, newtitle, -1,
			    madframe.header.bitrate,
			    madframe.header.samplerate,
			    MAD_NCHANNELS (&madframe.header));
      if (playback->title != NULL)
	{
	  g_free (playback->title);
	  playback->title = newtitle;
	}
    }
#else
  // Check if tuple info has changed
  if (update_tuple (tuple, madframe.header, station, rt, mmusic))
    {
      mowgli_object_ref (tuple);
      playback->set_tuple (playback, tuple);
#if AUD_PLUGIN_API < 19
      playback->set_params (playback, NULL, 0, madframe.header.bitrate,
			    madframe.header.samplerate,
			    MAD_NCHANNELS (&madframe.header));
#else
      playback->set_params (playback, madframe.header.bitrate,
			    madframe.header.samplerate,
			    MAD_NCHANNELS (&madframe.header));
#endif
    }
#endif

  return write_output (playback, &madsynth.pcm, &madframe.header);
}


static gpointer
get_name_thread (gpointer arg)
{
  gint sct = 0, rc, len, sid, dt, dl, svc_sid;
  gchar prov[256], name[256];
  guchar s[4096], *p, *q, *pp, *qq;

  svc_sid = *((gint *) arg);

  log_print (hlog, LOG_INFO, "get_name_thread(%d) starting", svc_sid);

  g_static_mutex_lock (&gmt_get_name);

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

      while (p < q)
	{
	  sid = (p[0] << 8) | p[1];
	  len = ((p[3] << 8) | p[4]) & 0xfff;
	  p += 5;

	  log_print (hlog, LOG_DEBUG, "SDT service ID = %d", sid);

	  if (sid == svc_sid)
	    {
	      pp = p;
	      qq = pp + len;

	      while (pp < qq)
		{
		  dt = *pp++;
		  dl = *pp++;

		  if (dt == 0x48)	// service_descriptor (SDT, SIT)
		    {
		      memcpy (prov, &pp[2], pp[1]);
		      prov[pp[1]] = '\0';
		      if (is_updated
			  (prov, &station->prov_name, DVB_STRING_DVBSI))
			station->refresh = TRUE;

		      memcpy (name, &pp[3 + pp[1]], pp[2 + pp[1]]);
		      name[pp[2 + pp[1]]] = '\0';
		      if (is_updated
			  (name, &station->svc_name, DVB_STRING_DVBSI))
			station->refresh = TRUE;

		      if (station->refresh)
			log_print (hlog, LOG_NOTICE,
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
  gint sid, rc, len, sct = 0;
  guint toctr = 0;
  guchar s[4096];
  gboolean epg_playing = TRUE;

  log_print (hlog, LOG_INFO, "epg_thread() starting");

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

  epg_read_data (epg, NULL, 0);

  while (playing && epg_playing)
    {
      rc = dvb_section (hdvb, 0x0012, 0x4e, sid, sct, s, 1000);
      switch (rc)
        {
        case RC_OK:
          toctr = 0;
          len = 3 + (((s[1] << 8) | s[2]) & 0xfff);
          log_print (hlog, LOG_DEBUG, "EPG section lenght = %d", len);

          epg_read_data (epg, s, len);
          if (s[6] != s[7])
            sct++;
          else
            sct = 0;
          break;
        case RC_DVB_TIMEOUT:
          log_print (hlog, LOG_DEBUG, "dvb_section() timeout");
          if (++toctr > MAX_DVB_TIMEOUT)
            {
              log_print (hlog, LOG_DEBUG,
                         "dvb_section() timed out too often, giving up");
              epg_playing = FALSE;
            }
          break;
        default:
          log_print (hlog, LOG_ERR,
                     "dvb_section() returned rc = %d, giving up", rc);
          epg_playing = FALSE;
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
  gint rc, dr, slen, blen, off = 0, fbf = 0;
  guint toctr = 0;
  guchar sect[5120], rtxt[32768];
  gboolean mm_playing = TRUE;

  log_print (hlog, LOG_INFO, "mmusic_thread() starting");

  g_static_mutex_lock (&gmt_mmusic);

  /* Make sure information retrieval is initialized */
  if ((mmusic = madmusic_init ()) == NULL)
    {
      g_static_mutex_unlock (&gmt_mmusic);
      g_thread_exit (0);
      return NULL;
    }

  while (playing && mm_playing)
    {
      memset (sect, 0xff, sizeof (sect));
      rc = dvb_dpkt (hdvb, sect, sizeof (sect), 1000, &dr);
      switch (rc)
        {
        case RC_OK:
          toctr = 0;
          slen = ((sect[1] << 8) | sect[2]) & 0xfff;
          blen = ((sect[22] << 8) | sect[23]) & 0xfff;
          if (blen <= (slen - 21))
            {
              madmusic_read_data (mmusic, &sect[24], blen - 2);
              fbf = 0;
              break;
            }

          off = ((sect[18] << 8) | sect[19]);
          if (fbf != off)
            {
              log_print (hlog, LOG_WARN,
                         "Warning! Offset out of whack, is %d, should be %d!",
                         off, fbf);
              fbf = 0;
              break;
            }

          memcpy (&rtxt[off], &sect[24], slen - 21);
          if ((off + (slen - 21)) >= blen)
            {
              madmusic_read_data (mmusic, rtxt, blen - 2);
              fbf = 0;
            }
          else
            fbf += (slen - 21 - 4);
          break;
        case RC_DVB_TIMEOUT:
          log_print (hlog, LOG_DEBUG, "dvb_dpkt() timeout");
          if (++toctr > MAX_DVB_TIMEOUT)
            {
              log_print (hlog, LOG_DEBUG,
                         "dvb_dpkt() timed out too often, giving up");
              mm_playing = FALSE;
            }
          break;
        default:
          log_print (hlog, LOG_ERR,
                     "dvb_dpkt() returned rc = %d, giving up", rc);
          mm_playing = FALSE;
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
  if (!infobox_is_visible ())
    {
      log_print (hlog, LOG_DEBUG, "removing infobox_timer()");
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

  return TRUE;
}
