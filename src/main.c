/*******************************************************************************
**
** Filename:      main.c
**
** Function List: get_iplugin_info()
**                dvb_init()
**                dvb_is_our_file()
**                dvb_play()
**                dvb_stop()
**                dvb_pause()
**                dvb_equalize()
**                dvb_gettime()
**                dvb_cleanup()
**                dvb_parse_url()
**                dvb_feed()
**                dvb_pes_pkt()
**                dvb_payload()
**                dvb_mpeg_frame()
**                dvb_get_pid()
**                dvb_get_name()
**                dvb_madmusic()
**                dvb_parse_text()
**                dvb_fixbillshit()
**                dvb_xlt()
**
** Function:      This module contains code that implements the
**                framework for a XMMS input plugin. This plugin
**                allows XMMS to directly play DVB audio services.
**                In addition, the plugin itself is able to record
**                the received source stream to file while feeding
**                it to XMMS for playback.
**
** Copyright:     (C) COPYRIGHT CHRISTIAN MOTZ 2003, 2004
**
**                This program is free software; you can redistribute
**                it and/or modify it under the terms of the GNU
**                General Public License as published by the Free
**                Software Foundation; either version 2, or (at your
**                option) any later version.
**
** Version:       $Id$
**
** Change Activity:
**
** 030618 -- CMO: Module created.
**
** 030622 -- CMO: Added logging for bughunting purposes, some more
**                sanity checks in the packet feeder, code to use the
**                SID in the service URL instead of the PID.
**
** 030626 -- CMO: Moved service name retrieval to its own thread and
**                added a (still basically useless) info dialog.
**                Moved GUI code out to a seperate file.
**
** 030628 -- CMO: Introduced code to allow for filesplitting by energy.
**
** 030701 -- CMO: Merged in code to retrieve track info on MAD Music
**                channels (NetMed / Nova package on Hotbird 13° E).
**
** 030704 -- CMO: Cleaned up the code a bit, added some comments,
**
** 030713 -- CMO: Added a few more comments.
**
** 040102 -- CMO: Added a dummy equalizer function, fixed copyright
**                statement to reflect the GPL status of the code.
**
** 040330 -- CMO: Hunted down the bug causing excessive CPU usage in
**                the X server while the plugin is running.
**                Incidentally the same cause was responsible for the
**                flickering visualization display in XMMS.
**
** 040407 -- CMO: Added the missing output buffer free check before
**                passing audio data to XMMS. Its absence caused
**                problems with some output plugins, namely ALSA.
**                Also fixed a bug in Service Name retrieval.
**
*******************************************************************************/

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif


#include <time.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <fcntl.h>

#include <xmms/plugin.h>
#include <xmms/util.h>
#include <xmms/configfile.h>

#include <lame/lame.h>

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


#define PLUGIN_NAME "DVB Audio Player 0.5.0"


typedef struct _SVC {
  int   svc_adapter;
  int   svc_frequency;
  int   svc_symbolrate;
  int   svc_viterbi;
  char  svc_polarisation;
  char  svc_delivery[256];
  char  svc_service[256];
} SVC;


void dvb_close_record(void);

extern void *dvb_epg(void *);
extern void dvb_clean_string(char *);

extern void dvb_gui_init(void);
extern void dvb_about(void);
extern void dvb_configure(void);
extern void dvb_getinfo(char *);
extern void dvb_info_update(char *, char *);

static void dvb_init(void);
static int  dvb_is_our_file(char *);
static void dvb_play(char *);
static void dvb_stop(void);
static void dvb_pause(short);
static void dvb_equalize(int, float, float *);
static int  dvb_gettime(void);
static void dvb_cleanup(void);

static int dvb_parse_url(char *, SVC *);
static void *dvb_feed(void *);
static void dvb_pes_pkt(unsigned char *, int, int);
static void dvb_payload(unsigned char *, int, int);
static void dvb_mpeg_frame(unsigned char *, int, int);
static int dvb_get_pid(int, int *, int *);
static void *dvb_get_name(void *);
static void *dvb_madmusic(void *);
static void dvb_parse_text(unsigned char *, int);
static void dvb_fixbillshit(unsigned char *);
static void dvb_xlt(unsigned char *);

int dvb_read_conf(char *, char *, char *, char *, int, char *);


/*
** Miscellaneous globals
*/

int         playing;                    /* This is also used in the GUI */
int         si_update;                  /* This is used in EPG retrieval */
void        *hlog;                      /* This is used everywhere :) */
void        *hdvb;                      /* EPG retrieval uses this */
ConfigFile  *xcfg;                      /* This is also used in the GUI */

/*
** Configuration file parameters
*/

int         cf_rec_guard, cf_get_info, cf_rec_sildur, cf_rec_isplit;
int         cf_record, cf_rec_append, cf_rec_asplit, cf_rec_stime;
int         cf_get_epg, epg_running;
char        cf_rec_file[MAXPATHLEN];
float       cf_rec_sillvl;

extern char epg_desc[4096];

static int              si_previous;
static int              paused, audio, file_index, mad_len, trnum, frm_ctr;
static char             erfn[MAXPATHLEN], album[256], artist[256], title[256];
static char             service_name[MAXPATHLEN];
static FILE             *rec_file;
static time_t           t_start, isplit_last, mad_time;
static pthread_t        pt, ptd, pte;
static unsigned char    mad_buf[8192];
static pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  svc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t  data_mutex = PTHREAD_MUTEX_INITIALIZER;

static int  sap;
static int  sumarr[512];

static int brt[] = {
  0, 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160, 0,
  0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160, 0,
  0, 32, 48, 56, 64,  80,  96,  112, 128, 144, 160, 176, 192, 224, 256, 0,
  0, 0,  0,  0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 0,
  0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384, 0,
  0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0
};

static int sft[] = {
  22050, 24000, 16000, 0, 44100, 48000, 32000, 0
};

InputPlugin dvb_ip = {
  0,                // Handle, filled in by xmms
  0,                // Filename, filled in by xmms
  PLUGIN_NAME,      // description
  dvb_init,         // Called when plugin is enabled
  dvb_about,        // Show the about box
  dvb_configure,    // Show the configure box
  dvb_is_our_file,  // check if file is for our plugin
  0,                // 
  dvb_play,         // play
  dvb_stop,         // stop
  dvb_pause,        // pause
  0,                // 
  dvb_equalize,     // equalizer
  dvb_gettime,      // get current play time
  0,                // get volume
  0,                // set volume
  dvb_cleanup,      // cleanup
  0,                // obsolete?
  0,                // send visualisation data
  0,                // set player window info
  0,                // set song title text
  0,                // get song title text to show on Playlist
  dvb_getinfo,      // file info box
  0                 // pointer to outputPlugin
};


/*******************************************************************************
** Function: get_iplugin_info()
**
** Description: XMMS entry point for plugin information retrieval.
**              Does nothing but retrieve the pointer to the plugin
**              descriptor which contains the list functions the
**              plugin makes available.
**
*******************************************************************************/

InputPlugin *get_iplugin_info(void)
{
    return &dvb_ip;
}


/*******************************************************************************
** Function: dvb_init()
**
** Description: This code is called a single time  at XMMS startup
**              once it has identified and loaded all plugins. So all
**              things that need to be done to get the plugins' data
**              structures into an initial sane state should be done
**              here.
**
*******************************************************************************/

static void dvb_init(void)
{
  int         rc, log_lvl, idb;
  char        *s, log_fn[MAXPATHLEN];

  s    = NULL;

  /*
  ** Plugin configuration parameters.
  */

  if ((xcfg = xmms_cfg_open_default_file()) != NULL) {
    if (xmms_cfg_read_string(xcfg, "DVB", "Logfile", &s)) {
      if (s != NULL) {
        strcpy(log_fn, s);
      }
    } else {
      strcpy(log_fn, "/tmp/xmms-dvb.log");
    }

    if (!xmms_cfg_read_int(xcfg, "DVB", "Loglevel", &log_lvl)) {
      log_lvl = 0;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "Record", &cf_record)) {
      cf_record = 0;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "Append", &cf_rec_append)) {
      cf_rec_append = 0;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "Autosplit", &cf_rec_asplit)) {
      cf_rec_asplit = 0;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "Split", &cf_rec_isplit)) {
      cf_rec_isplit = 0;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "EPG", &cf_get_epg)) {
      cf_get_epg = 1;
    }

    if (!xmms_cfg_read_int(xcfg, "DVB", "Duration", &cf_rec_sildur)) {
      cf_rec_sildur = 360;
    }

    if (xmms_cfg_read_int(xcfg, "DVB", "Level", &idb)) {
      cf_rec_sillvl = (float)idb;
      cf_rec_sillvl /= 100;
    } else {
      cf_rec_sillvl = -42.80;           /* This is a good start */
    }

    if (!xmms_cfg_read_int(xcfg, "DVB", "Guard", &cf_rec_guard)) {
      cf_rec_guard = 15;
    }

    if (!xmms_cfg_read_boolean(xcfg, "DVB", "Info", &cf_get_info)) {
      cf_get_info = 0;
    }

    if (!xmms_cfg_read_int(xcfg, "DVB", "Interval", &cf_rec_stime)) {
      cf_rec_stime = 0;
    }

    if (xmms_cfg_read_string(xcfg, "DVB", "File", &s)) {
      if (s != NULL) {
        strcpy(cf_rec_file, s);
      }
    } else {
      strcpy(cf_rec_file, "");
    }
  } else {
    cf_record     = 0;
    cf_rec_append = 0;
    cf_rec_asplit = 0;
    cf_rec_isplit = 0;
    cf_rec_sildur = 360;
    cf_rec_guard  = 15;
    cf_get_info   = 0;
    cf_get_epg    = 1;
    cf_rec_stime  = 0;
    strcpy(cf_rec_file, "");

    log_lvl = 0;
    strcpy(log_fn, "/tmp/xmms-dvb.log");
  }

  if ((rc = log_open(log_fn, &hlog, "xmms-dvb", log_lvl)) != RC_OK) {
    hlog = NULL;
  }

  log_print(hlog, LOG_INFO, "logging started.");
  log_print(hlog, LOG_DEBUG, "%.2f dB (%d), %d ms, %d s", cf_rec_sillvl, idb,
            cf_rec_sildur, cf_rec_guard);

  audio       = 0;
  t_start     = 0;
  playing     = 0;
  paused      = 0;
  frm_ctr     = 0;
  hdvb        = NULL;
  rec_file    = NULL;
  epg_running = 0;

  dvb_gui_init();

  memset(&pt, 0x00, sizeof(pt));
  memset(&ptd, 0x00, sizeof(ptd));
  memset(&pte, 0x00, sizeof(pte));

  return;
}


/*******************************************************************************
** Function: dvb_is_our_file()
**
** Description: XMMS uses this function in order to find the right
**              plugin for a specific file or address type. For this
**              the respective equivalents of this function is called
**              for all plugins installed. They are passed the URL or
**              filename that the user wants to play. If a plugin
**              wants to indicate it is responsible for handling the
**              type in question, it returns with a nonzero value.
**
*******************************************************************************/

static int dvb_is_our_file(char *s)
{
  int rc;

  if ((rc = dvb_parse_url(s, NULL)) == RC_OK) {
    return (1);
  }

  log_print(hlog, LOG_DEBUG, "dvb_parse_url() returned rc = %d", rc);

  return (0);
}


/*******************************************************************************
** Function: dvb_play()
**
** Description: This function is invoked by XMMS when the user tells
**              XMMS to play a specific file or location. It is passed
**              the filename or URL as a parameter.
**
**              Note that this does _not_ perform the actual playout
**              itself; it probably could, but that would block the
**              user interface of XMMS (at least). So we prepare
**              everything here and then do the actual playout in a
**              seperate thread.
**
*******************************************************************************/

static void dvb_play(char *s)
{
  int         rc, apid, dpid, sid;
  SVC         svc;
  char        tfn[MAXPATHLEN];
  struct stat st;

  log_print(hlog, LOG_DEBUG, "dvb_play(\"%s\");", s);

  if (playing) {
    return;
  }

  if ((rc = dvb_parse_url(s, &svc)) != RC_OK) {
    log_print(hlog, LOG_INFO, "dvb_parse_url() returned rc = %d", rc);
    return;
  }

  playing = 1;
  t_start = 0;
  frm_ctr = 0;

  sid     = -1;
  sap     = 0;
  memset(sumarr, 0x00, sizeof(sumarr));

  /*
  ** Initialize Service Information counters;
  */

  si_update = si_previous = 0;

  /*
  ** Make sure information retrieval is initialized.
  */

  mad_len  = 0;
  mad_time = 0;
  memset(mad_buf, 0x00, sizeof(mad_buf));

  if (rec_file == NULL) {
    memset(erfn, 0x00, sizeof(erfn));
  }

  file_index = 0;
  if (cf_rec_asplit || cf_rec_isplit) {
    if (index(cf_rec_file, '%')) {
      while (1) {
        sprintf(tfn, cf_rec_file, file_index);
        if (stat(tfn, &st) < 0) {
          break;
        }
        file_index++;
      }
    }
  }

  if ((rc = dvb_open(svc.svc_adapter, &hdvb)) != RC_OK) {
    playing = 0;
    return;
  }

  if ((rc = dvb_tune_qpsk(hdvb, svc.svc_delivery[1] - 'A', svc.svc_frequency,
                          svc.svc_polarisation, svc.svc_symbolrate, 0)) != RC_OK) {
    playing = 0;
    dvb_close(hdvb);
    hdvb = NULL;
    return;
  }

  strcpy(service_name, s);
  dvb_info_update("", "");

  if (strncasecmp(svc.svc_service, "0x", 2) == 0) {
    sscanf(&svc.svc_service[2], "%x", &apid);
  } else {
    if (strncasecmp(svc.svc_service, "sid=", 4) == 0) {
      if (strncasecmp(&svc.svc_service[4], "0x", 2) == 0) {
        sscanf(&svc.svc_service[6], "%x", &sid);
      } else {
        sid = atoi(&svc.svc_service[4]);
      }

      if ((rc = dvb_get_pid(sid, &apid, &dpid)) != RC_OK) {
        log_print(hlog, LOG_WARNING, "dvb_get_pid() returned %d.", rc);
        dvb_close(hdvb);
        playing = 0;
        hdvb = NULL;
        return;
      }
    } else {
      apid = atoi(svc.svc_service);
    }
  }

  if ((rc = dvb_volume(hdvb, 0)) != RC_OK) {
    log_print(hlog, LOG_WARNING, "dvb_volume() returned %d.", rc);
  }

  if ((rc = dvb_apid(hdvb, apid)) != RC_OK) {
    log_print(hlog, LOG_ERR, "dvb_apid() returned %d.", rc);
    dvb_close(hdvb);
    playing = 0;
    hdvb = NULL;
    return;
  }

  lame_decode_init();

  if ((dpid > 0) && cf_get_info) {
    log_print(hlog, LOG_INFO, "Data service associated on PID %d (0x%04x).",
              dpid, dpid);

    if ((rc = dvb_dpid(hdvb, dpid)) == RC_OK) {
      if (pthread_create(&ptd, 0, dvb_madmusic, 0) != 0) {
        log_print(hlog, LOG_ERR, "pthread_create() failed for dvb_madmusic()");
      }
    } else {
      log_print(hlog, LOG_ERR, "dvb_dpid() returned %d.", rc);
    }
  } else {
    memset(title, 0x00, sizeof(title));
    memset(artist, 0x00, sizeof(artist));
    memset(album, 0x00, sizeof(album));
  }

  if (sid != -1) {
    if (cf_get_epg) {
      if (pthread_create(&pte, 0, dvb_epg, (void *)sid) != 0) {
        log_print(hlog, LOG_ERR, "pthread_create() failed for dvb_epg()");
      } else {
        epg_running = 1;
      }
    }
  }

  if (pthread_create(&pt, 0, dvb_feed, 0) != 0) {
    playing = 0;
    log_print(hlog, LOG_CRIT, "pthread_create() failed for dvb_feed()");
  }

  return;
}


/*******************************************************************************
** Function: dvb_stop()
**
** Description: Called by XMMS whenever it wants to stop playing a
**              given location (or file). This not only happens when
**              the user presses stop but also when skipping to the
**              next entry in a playlist, when hitting play while
**              playing or upon termination.
**
*******************************************************************************/

static void dvb_stop(void)
{
  if (playing) {
    playing = 0;
    paused = 0;

    pthread_join(pt, NULL);
    memset(&pt, 0x00, sizeof(pt));

    if (cf_get_info) {
      pthread_join(ptd, NULL);
      memset(&ptd, 0x00, sizeof(ptd));
    }

    if (epg_running && cf_get_epg) {
      pthread_join(pte, NULL);
      memset(&ptd, 0x00, sizeof(pte));
      epg_running = 0;
    }

    dvb_ip.output->close_audio();

    if (hdvb) {
      /*
      ** Stopping the audio and data PID filters should probably be added.
      */
      dvb_close(hdvb);
    }

    if (rec_file) {
      dvb_close_record();
    }
  }

  return;
}


/*******************************************************************************
** Function: dvb_pause()
**
** Description: This is invoked when the user hits the pause button on
**              XMMS. Actually I'm not quite sure what to do with
**              this -- at this point it just cuts the packet feed to
**              the handler. Any other ideas?
**
*******************************************************************************/

static void dvb_pause(short i)
{
  if (playing) {
    paused = i;
  }

  return;
}


/*******************************************************************************
** Function: dvb_equalize()
**
** Description: 
**
*******************************************************************************/

static void dvb_equalize(int on, float preamp, float *bands)
{
  int i;

  static char *eq_lbl[] = {
    " 60 Hz", "170 Hz", "310 Hz", "600 Hz", " 1 kHz",
    " 3 kHz", " 6 kHz", "12 kHz", "14 kHz", "16 kHz"
  };

  log_print(hlog, LOG_DEBUG, "dvb_equalize(%d, %f, 0x%08lx);", on, preamp, bands);

  for (i = 0; i < 10; i++) {
    log_print(hlog, LOG_DEBUG, "%s: %f dB", eq_lbl[i], bands[i]);
  }

  return;
}


/*******************************************************************************
** Function: dvb_gettime()
**
** Description: Ok, I'll admit it: I have _no_ idea whatsoever what
**              this is really supposed to do. I just stole a code
**              fragment from another plugin.
**
*******************************************************************************/

static int dvb_gettime(void)
{
  if (playing) {
    return(dvb_ip.output->output_time());
  }

  return (0);
}


/*******************************************************************************
** Function: dvb_cleanup()
**
** Description: 
**
*******************************************************************************/

static void dvb_cleanup(void)
{
  log_print(hlog, LOG_INFO, "logging stopped.");
  log_close(hlog);

  hlog = NULL;

  return;
}


/*******************************************************************************
** Function: dvb_parse_url()
**
** Description: This parses an URL describing a DVB service.
**
*******************************************************************************/

static int dvb_parse_url(char *url, SVC *svc)
{
  int   i, a_num, fec1, fec2, sr, qrg, fec;
  char  *p, *q, fn[MAXPATHLEN], pol;
  char  *sdlv, *sqrg, *sfec1, *sfec2;

  if ((strlen(url) + 1) > sizeof(fn)) {
    return (RC_PARSE_URL_TOO_LONG);
  }
  strcpy(fn, url);

  if (strncasecmp(fn, "dvb", 3) != 0) {
    return (RC_PARSE_URL_NOT_DVB_URL);
  }

  if ((p = index(fn, ':')) == NULL) {
    return (RC_PARSE_URL_COLON_MISSING);
  }

  *p++ = '\0';

  if (strlen(&fn[3]) > 0) {
    for (i = 0; i < strlen(&fn[3]); i++) {
      if (!isdigit(fn[3 + i])) {
        return (RC_PARSE_URL_ADAPTER_NOT_NUMERIC);
      }
    }
  }
  a_num = atoi(&fn[3]);

  if (strncmp(p, "//", 2) != 0) {
    return (RC_PARSE_URL_SEPARATOR_MISSING);
  }

  p += 2;

  if ((q = index(p, '/')) == NULL) {
    return (RC_PARSE_URL_DLVRY_SYS_MISSING);
  }
  *q++ = '\0';
  sdlv = p;

  if ((p = index(q, '/')) == NULL) {
    return (RC_PARSE_URL_DLVRY_SYS_MISSING);
  }
  *p++ = '\0';
  sqrg = q;

  if ((q = index(p, '/')) == NULL) {
    return (RC_PARSE_URL_SYMBOLRATE_MISSING);
  }
  *q++ = '\0';
  sr = 1000 * atoi(p);

  if ((p = index(q, '/')) == NULL) {
    return (RC_PARSE_URL_FEC_DIVIDEND_MISSING);
  }
  *p++ = '\0';
  sfec1 = q;
  fec1 = atoi(sfec1);

  fec = 0;

  if ((fec1 > 0) && (fec1 < 9)) {
    if ((q = index(p, '/')) == NULL) {
      return (RC_PARSE_URL_FEC_DIVISOR_MISSING);
    }
    *q++ = '\0';
    sfec2 = p;
    fec2 = atoi(sfec2);

    if ((fec1 + 1) != fec2) {
      return (RC_PARSE_URL_FEC_IMPLAUSIBLE);
    }

    fec = fec1;
  } else {
    if (strcasecmp(sfec1, "none") == 0) {
      fec = 0;
    } else {
      if (strcasecmp(sfec1, "auto") != 0) {
        return (RC_PARSE_URL_FEC_NOT_RECOGNIZED);
      }

      fec = 9;
    }
    q = p;
  }

  pol = ' ';

  if (toupper(sdlv[0]) == 'S') {
    switch (toupper(sqrg[strlen(sqrg) - 1])) {
      case 'H':
        pol = 'H';
        break;

      case 'V':
        pol = 'V';
        break;

      default:
        return (RC_PARSE_URL_UNK_POLARIZATION);
        break;
    }

    sqrg[strlen(sqrg) - 1] = '\0';
  }

  qrg = 1000 * atoi(sqrg);

  if (svc != NULL) {
    svc->svc_adapter      = a_num;
    svc->svc_frequency    = qrg;
    svc->svc_viterbi      = fec;
    svc->svc_symbolrate   = sr;
    svc->svc_polarisation = pol;
    strcpy(svc->svc_delivery, sdlv);
    strcpy(svc->svc_service, q);
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_feed()
**
** Description: This is the audio packet feeder. It is started in its
**              own thread and will attempt to get audio packets from
**              the DVB adapter for as long as XMMS is playing.
**
*******************************************************************************/

static void *dvb_feed(void *args)
{
  int           rc, ar, toctr;
  unsigned char pkt[3840];

  log_print(hlog, LOG_INFO, "dvb_feed() thread started");

  pthread_mutex_lock(&mutex);

  dvb_pes_pkt(NULL, 0, 1);
  dvb_payload(NULL, 0, 1);

  time(&isplit_last);

  toctr = 0;

  while (playing) {
    rc = dvb_apkt(hdvb, pkt, sizeof(pkt), 1000, &ar);
    if (rc == RC_OK) {
      toctr = 0;
      if (!paused) {
        dvb_pes_pkt(pkt, ar, 0);
      }
    } else {
      if (rc == RC_DVB_APKT_SELECT_TIMEOUT) {
        toctr++;
        if (toctr > 12) {
//          playing = 0;
        }
        log_print(hlog, LOG_DEBUG, "dvb_apkt() timeout", rc);
      } else {
        log_print(hlog, LOG_ERR, "dvb_apkt() returned rc = %d", rc);
        playing = 0;
      }
    }
  }

  log_print(hlog, LOG_DEBUG, "play-loop terminated");

  dvb_unfilter(hdvb);
  dvb_close(hdvb);
  hdvb = NULL;
  t_start = 0;

  if (rec_file != NULL) {
    dvb_close_record();
  }

  if (audio) {
    dvb_ip.output->close_audio();
    audio = 0;
  }

  log_print(hlog, LOG_INFO, "dvb_feed() thread stopping");

  pthread_mutex_unlock(&mutex);
  pthread_exit(NULL);
}


/*******************************************************************************
** Function: dvb_pes_pkt()
**
** Description: Collects data packets from fed to it and re-assembles
**              them into PES packets. Consider this as an additional
**              sanity check -- I am quite sure this could be handled
**              'on the fly' without re-assembly.
**
*******************************************************************************/

static void dvb_pes_pkt(unsigned char *buf, int len, int reset)
{
  int                   i, stream_id, PES_packet_length, j, pp_len;
  static int            pbh, pbl;
  unsigned char         *p, *pp;
  static unsigned char  pesbuf[128 * 1024];

  if (reset) {
    pbh = pbl = 0;
    memset(pesbuf, 0x00, sizeof(pesbuf));
    return;
  }

  if ((pbh + len) > sizeof(pesbuf)) {
    log_print(hlog, LOG_CRIT, "PES buffer overflow imminent, flushing!");
    pbh = pbl = 0;
    memset(pesbuf, 0x00, sizeof(pesbuf));
    return;
  }

  memcpy(&pesbuf[pbh], buf, len);
  pbh += len;

  while (1) {
    if (pbl > pbh) {
      /* This is a serious logic error */
    }

    if (pbl < pbh) {
      if ((pbh - pbl) > 4) {
        for (i = pbl; i < (pbh - 4); i++) {
/*          if ((pesbuf[i] == 0x00) &&
              (pesbuf[i + 1] == 0x00) &&
              (pesbuf[i + 2] == 0x01)) {
            printf("%02x\n", pesbuf[i + 3]);
          } */

          if ((pesbuf[i] == 0x00) &&
              (pesbuf[i + 1] == 0x00) &&
              (pesbuf[i + 2] == 0x01) &&
              ((pesbuf[i + 3] >> 4) > 0xa)) {
            break;
          }
        }

        if (i < (pbh - 4)) {
          if (i > pbl) {
            memcpy(pesbuf, &pesbuf[i], pbh - i);
            pbl = 0;
            pbh = pbh - i;
          } else {
            stream_id = pesbuf[i + 3];
            PES_packet_length = (pesbuf[i + 4] << 8) | pesbuf[i + 5];
            if (PES_packet_length == 0) {
              /* So now what? */
              for (j = (i + 4); j < (pbh - 4); j++) {
                if ((pesbuf[j] == 0x00) &&
                    (pesbuf[j + 1] == 0x00) &&
                    (pesbuf[j + 2] == 0x01) &&
                    ((pesbuf[j + 3] >> 4) > 0xa)) {
                  break;
                }
              }

              if (j < (pbh - 4)) {
                PES_packet_length = j - i - 6;
              } else {
                return;
              }
            }

            if ((pbh - pbl) <= (PES_packet_length + 6)) {
              return;
            } else {
              /* Uhmmmm, complete? */

              p = &pesbuf[i];

              pp = p + 9 + p[8];
              pp_len = PES_packet_length - 3 - p[8];
              dvb_payload(pp, pp_len, 0);

              memcpy(pesbuf, &pesbuf[i + 6 + PES_packet_length],
                     pbh - (i + 6 + PES_packet_length));
              pbl = 0;
              pbh -= (i + 6 + PES_packet_length);
            }
          }
        } else {
          memcpy(pesbuf, &pesbuf[i], pbh - i);
          pbl = 0;
          pbh -= i;
          return;
        }
      } else {
        return;
      }
    } else {
      pbl = 0;
      pbh = 0;
      return;
    }
  }

  return;
}


/*******************************************************************************
** Function: dvb_payload()
**
** Description: Handles the payload of an audio PES packet. It checks
**              for individual MPEG frames and feeds them to the frame
**              handler one by one.
**
*******************************************************************************/

static void dvb_payload(unsigned char *buf, int len, int reset)
{
  int                   br, sf, fl, num_samples;
  int                   i, mpv, mpl, crc, bri, tlu, sfi, pad;
  static int            bph = 0;
  static unsigned char  mpbuf[128 * 1024];

  if (reset) {
    bph = 0;
    memset(mpbuf, 0x00, sizeof(mpbuf));
    return;
  }

  memcpy(&mpbuf[bph], buf, len);
  bph += len;

  while (1) {
    if ((mpbuf[0] == 0xff) && ((mpbuf[1] & 0xf0) == 0xf0)) {
      /* Frame sync at buffer start. A Good Thing. */

      /* Is there a next one yet? */

      for (i = 1; i < (bph - 4); i++) {
        if ((mpbuf[i] == 0xff) && ((mpbuf[i + 1] & 0xf0) == 0xf0)) {
          break;
        }
      }

      if (i < (bph - 4)) {
        /* yes, we may have an entire frame */

        mpv = (mpbuf[1] >> 3) & 1;
        mpl = (mpbuf[1] >> 1) & 3;
        crc = mpbuf[1] & 1;
        bri = (mpbuf[2] >> 4) & 0xf;
        sfi = (mpbuf[2] >> 2) & 3;
        pad = (mpbuf[2] >> 1) & 1;

        tlu = bri | (mpl << 4) | (mpv << 6);
        br  = brt[tlu];

        tlu = sfi | (mpv << 2);
        sf  = sft[tlu];

        if ((sf == 0) || (br == 0)) {
          /* Uhm, no, this is not it */
          memcpy(mpbuf, &mpbuf[i], bph - i);
          bph -= i;
          return;
        }

        if (mpl == 3) {
          num_samples = 384;
          fl = (12 * (br * 1000) / sf + pad) * 4;
        } else {
          num_samples = 1152;
          fl = 144 * (br * 1000) / sf + pad;
        }

        if (fl > bph) {
          return;
        }

        if ((mpbuf[fl] == 0xff) && ((mpbuf[fl + 1] & 0xf0) == 0xf0)) {
          dvb_mpeg_frame(mpbuf, fl, num_samples);
          memcpy(mpbuf, &mpbuf[fl], bph - fl);
          bph -= fl;
        } else {
          memcpy(mpbuf, &mpbuf[i], bph - i);
          bph -= i;
	}
      } else {
        /* whoops, that sucks */
        return;
      }
    } else {
      for (i = 1; i < (bph - 4); i++) {
        if ((mpbuf[i] == 0xff) && ((mpbuf[i + 1] & 0xf0) == 0xf0)) {
          break;
        }
      }

      if (i < (bph - 4)) {
        memcpy(mpbuf, &mpbuf[i], bph - i);
        bph -= i;
      } else {
        /* No sync in buffer yet, that hurts */
        return;
      }
    }
  }
}


/*******************************************************************************
** Function: dvb_mpeg_frame()
**
** Description: Takes 1-n MPEG audio frames, decodes them using
**              libmp3lame and passes the resulting samples to XMMS
**              for output and visualization. Also passes any
**              additional information to XMMS for display.
**
*******************************************************************************/

static void dvb_mpeg_frame(unsigned char *frame, int len, int smp)
{
  int             nout, i, vu, ms;
  char            xmms_info[4096];
  float           dB;
  time_t          t;
  static short    left[34560], right[34560];
  static short    stereo[sizeof(left) + sizeof(right)];
  mp3data_struct  mp3d;

  frm_ctr++;

  memset(&mp3d, 0x00, sizeof(mp3d));

  if (cf_record) {
    time(&t);

    if (cf_rec_asplit && (t_start > 0) &&
        (rec_file != NULL) && (cf_rec_stime > 0)) {
      if ((t - t_start) >= cf_rec_stime) {
        dvb_close_record();
      }
    }

    if (rec_file == NULL) {
      if (cf_rec_asplit || cf_rec_isplit) {
        sprintf(erfn, cf_rec_file, file_index);
      } else {
        sprintf(erfn, cf_rec_file, 0);
      }

      if (cf_rec_append) {
        log_print(hlog, LOG_INFO, "opening record \"%s\" for append", erfn);
        rec_file = fopen(erfn, "ab");
      } else {
        log_print(hlog, LOG_INFO, "opening record \"%s\"", erfn);
        rec_file = fopen(erfn, "wb");
      }

      if (cf_rec_asplit || cf_rec_isplit) {
        if (rec_file != NULL) {
          time(&t_start);
          file_index++;
        }
      }
    }

    if (rec_file != NULL) {
      fwrite(frame, sizeof(unsigned char), len, rec_file);
    }
  }

  if ((nout = lame_decode_headers(frame, len, left, right, &mp3d)) > 0) {
    if (mp3d.header_parsed == 1) {
      if (!audio) {
        audio = dvb_ip.output->open_audio(FMT_S16_NE, mp3d.samplerate, 2);
      }

      /*
      ** This is just a quick fix -- we now update the info only
      ** every 120 frames since it's so computationally expensive.
      ** Eventually it should be updated whenever it changes, which
      ** shouldn't be too often in Real World conditions.
      */

      if (si_update > si_previous) {
        si_previous = si_update;
        if (cf_get_epg && epg_running && (strlen(epg_desc) > 0)) {
          sprintf(xmms_info, "%s: %s", service_name, epg_desc);
          dvb_ip.set_info(xmms_info, -1, mp3d.bitrate * 1000,
                          mp3d.samplerate, mp3d.stereo);
        } else {
          dvb_ip.set_info(service_name, -1, mp3d.bitrate * 1000,
                          mp3d.samplerate, mp3d.stereo);
        }
      }
    }

    /*
    ** Unfortunately XMMS output wants sample data interleaved,
    ** not seperate, so we have to rearrange it accordingly. But
    ** that's ok, we need to look at the PCM data to determine
    ** the energy level anyway :)
    */

    vu = 0;

    for (i = 0; i < nout; i++) {
      if (mp3d.stereo == 2) {
        stereo[2 * i]     = left[i];
        stereo[2 * i + 1] = right[i];
        vu += (abs(left[i]) + abs(right[i])) / 2;
      } else {
        stereo[2 * i]     = left[i];
        stereo[2 * i + 1] = left[i];
        vu += abs(left[i]);
      }
    }

    vu /= nout;

    if (audio) {
      dvb_ip.add_vis_pcm(dvb_ip.output->written_time(),
                         FMT_S16_NE, 2, nout << 2, stereo);

      while ((dvb_ip.output->buffer_free() < (nout << 2)) && playing) {
        xmms_usleep(10000);
      }

      dvb_ip.output->write_audio(stereo, nout << 2);
    }

    sumarr[sap++] = vu;
    ms = (sap * nout) / mp3d.bitrate;
    if (ms >= cf_rec_sildur) {
      vu = 0;

      for (i = 0; i < sap; i++) {
        vu += sumarr[i];
      }

      vu /= sap;

      dB = 20 * log10((float)vu / 32767);

      if (dB < cf_rec_sillvl) {
        time(&t);
        if ((t - isplit_last) >= cf_rec_guard) {
          log_print(hlog, LOG_INFO,
                    "Avg: %.2f dB (%d) / %d ms (%d f) / %d:%02d.",
                    dB, vu, ms, sap, (t - isplit_last) / 60,
                    (t - isplit_last) % 60);
          time(&isplit_last);

          if (cf_rec_isplit && cf_record) {
            dvb_close_record();
          }

          sap = 1;
          memset(sumarr, 0x00, sizeof(sumarr));
        }
      }

      memcpy(sumarr, &sumarr[1], sizeof(int) * (sap - 1));
      sap--;
    }
  }

  return;
}


/*******************************************************************************
** Function: dvb_close_record()
**
** Description: Close the current recording file if one is open.
**              Resets the parameters for silence detection as well
**              and checks if any track info is available for the just
**              recorded file if information retrieval is active.
**
*******************************************************************************/

void dvb_close_record()
{
  if (rec_file != NULL) {
    log_print(hlog, LOG_INFO, "closing record \"%s\"", erfn);
    fclose(rec_file);
    rec_file = NULL;
    sap = 0;
    memset(sumarr, 0x00, sizeof(sumarr));

    if (strlen(title) > 0) {
      if ((mad_time > 0) && (t_start <= mad_time)) {
        log_print(hlog, LOG_INFO, "%d,%s,%s,%s", trnum, album, artist, title);
        /*
        ** One idea would be to attach an ID3 tag to the just recorded
        ** file at this point. If I find as some decent code for it I
        ** will probably do just that. But ID3Lib SUCKS!
        */
      } else {
        log_print(hlog, LOG_INFO, "Track info %d:%02d too old",
                  (t_start - mad_time) / 60, (t_start - mad_time) % 60);
      }
    }
  }

  return;
}


/*******************************************************************************
** Function: dvb_get_pid()
**
** Description: Find the PID of a service's audio stream from the SID.
**              For this the PAT must be read, the PMT PID determined
**              from it and be read and parsed to find the ES PID. If
**              a service provides several audio streams, the first
**              one listed in the PMT will be selected.
**
*******************************************************************************/

static int dvb_get_pid(int s, int *apid, int *dpid)
{
  int           rc, len, pmt, pil, es, es_type, es_audio, es_data;
  static int    sid;
  unsigned char sct[4096], *p, *q;

  if ((rc = dvb_section(hdvb, 0, 0, 0, 0, sct, 10000)) != RC_OK) {
    return (rc);
  }

  len = 3 + (((sct[1] << 8) | sct[2]) & 0xfff);
  p = &sct[8];
  q = (sct + len) - 4;

  es_audio = es_data = -1;

  while(p < q) {
    sid = (p[0] << 8) | p[1];
    pmt = ((p[2] << 8) | p[3]) & 0x1fff;

    if (sid == s) {
      if ((rc = dvb_section(hdvb, pmt, 2, sid, 0, sct, 10000)) == RC_OK) {
        len = 3 + (((sct[1] << 8) | sct[2]) & 0xfff);
        pil = ((sct[10] << 8) | sct[11]) & 0xfff;
        p = sct + 12 + pil;
        q = sct + len - 4;

        while (p < q) {
          es = ((p[1] << 8) | p[2]) & 0x1fff;
          es_type = p[0];
          pil = ((p[3] << 8) | p[4]) & 0xfff;
          p += 5;

          if ((es_type == 0x03) || (es_type == 0x04)) {
            if (es_audio < 0) {
              log_print(hlog, LOG_INFO, "Service Audio PID = %d (0x%04x)",
                        es, es);
              if (pthread_create(&pt, 0, dvb_get_name, (void *)&sid) != 0) {
                log_print(hlog, LOG_WARNING,
                          "Failed to start dvb_get_name() thread");
              }

              es_audio = es;
            }
          }

          if (es_type == 0x05) {
            if (es_data < 0) {
              log_print(hlog, LOG_INFO, "Service Data PID = %d (0x%04x)",
                        es, es);

              es_data = es;
            }
          }

          p += pil;
        }
      } else {
        return (rc);
      }
    }

    p += 4;
  }

  if (es_audio < 0) {
    return (RC_DVB_GET_PID_SID_NOT_IN_PAT);
  }

  *apid = es_audio;
  *dpid = es_data;

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_get_name()
**
** Description: This is run as a seperate thread in order to keep XMMS
**              moving -- it can be a lengthy process taking up to
**              quite a few seconds. It retrieves a service's Name
**              from the SDT for a given SID. To be honest, this kind
**              of is chrome, fluff, however you want to call it. It
**              is seriously _not_ needed for functionality :)
**
*******************************************************************************/

static void *dvb_get_name(void *arg)
{
  int           sct, rc, len, sid, dt, dl, svc_sid;
  char          prov[256], name[256];
  unsigned char s[4096], *p, *q, *pp, *qq;

  svc_sid = *(int *)arg;

  pthread_mutex_lock(&svc_mutex);

  log_print(hlog, LOG_INFO, "dvb_get_name(%d) thread starting", svc_sid);

  sct = 0;

  while (playing) {
    if ((rc = dvb_section(hdvb, 0x0011, 0x42, 0, sct, s, 10000)) != RC_OK) {
      log_print(hlog, LOG_INFO, "dvb_section() returned %d", rc);
      break;
    }
    len = 3 + (((s[1] << 8) | s[2]) & 0xfff);
    log_print(hlog, LOG_DEBUG, "SDT section length = %d", len);

    p = &s[11];
    q = (s + len) - 4;

    while((p < q) && playing) {
      sid = (p[0] << 8) | p[1];
      len = ((p[3] << 8) | p[4]) & 0xfff;
      p += 5;

      log_print(hlog, LOG_DEBUG, "SDT service ID = %d", sid);

      if (sid == svc_sid) {
        pp = p;
        qq = pp + len;
        while ((pp < qq) && playing) {
          dt = *pp++;
          dl = *pp++;

          if (dt == 0x48) {
            memcpy(prov, &pp[2], pp[1]);
            prov[pp[1]] = '\0';
            dvb_clean_string(prov);

            memcpy(name, &pp[3 + pp[1]], pp[2 + pp[1]]);
            name[pp[2 + pp[1]]] = '\0';
            dvb_clean_string(name);

            log_print(hlog, LOG_INFO, "Service name: \"%s\", \"%s\"", prov, name);

            dvb_info_update(prov, name);

            if (strlen(prov) > 0) {
              sprintf(service_name, "%s - %s", prov, name);
            } else {
              sprintf(service_name, "%s", name);
            }
            si_update++;

            log_print(hlog, LOG_INFO, "dvb_get_name() thread stopping");

            pthread_mutex_unlock(&svc_mutex);
            pthread_exit(NULL);
            return (NULL);
          }

          pp += dl;
        }
      }

      p += len;
    }

    if ((s[6] == s[7]) && (s[6] == sct)) {
      log_print(hlog, LOG_INFO, "dvb_get_name() last section");
      break;
    }

    sct++;
  }

  log_print(hlog, LOG_INFO, "dvb_get_name() thread stopping");

  pthread_mutex_unlock(&svc_mutex);
  pthread_exit(NULL);
  return (NULL);
}


/*******************************************************************************
** Function: dvb_madmusic()
**
** Description: This is reverse-engineered code to extract track
**              information from the OpenTV application that
**              accompanies the 'MAD Music' channels of the Nova /
**              NetMed / Multichoice Hellas subscription service on
**              13° east. This function retrieves the entire OpenTV
**              section that contains the information.
**
*******************************************************************************/

static void *dvb_madmusic(void *arg)
{
  int           rc, dr, slen, blen, off, fbf;
  unsigned char sect[5120], rtxt[32768];

  log_print(hlog, LOG_INFO, "dvb_madmusic() thread started");

  pthread_mutex_lock(&data_mutex);

  fbf = off = 0;

  while (playing && cf_get_info) {
    memset(sect, 0xff, sizeof(sect));
    rc = dvb_dpkt(hdvb, sect, sizeof(sect), 1000, &dr);
    if (rc != RC_OK) {
      if (rc != RC_DVB_DPKT_SELECT_TIMEOUT) {
        log_print(hlog, LOG_ERR, "Error: data reception died, rc = %d!", rc);
        break;
      } else {
        log_print(hlog, LOG_INFO, "data reception stalling ...", rc);
      }
    } else {
      slen = ((sect[1] << 8) | sect[2]) & 0xfff;
      blen = ((sect[22] << 8) | sect[23]) & 0xfff;
      if (blen <= (slen - 21)) {
        dvb_parse_text(&sect[24], blen - 2);
        fbf = 0;
      } else {
        off  = ((sect[18] << 8) | sect[19]);
        if (fbf == off) {
          memcpy(&rtxt[off], &sect[24], slen - 21);
          if ((off + (slen - 21)) >= blen) {
            dvb_parse_text(rtxt, blen - 2);
            fbf = 0;
          } else {
            fbf += (slen - 21 - 4);
          }
        } else {
          log_print(hlog, LOG_WARNING,
                    "Warning! Offset out of whack, is %d, should be %d!",
                    off, fbf);
          fbf = 0;
        }
      }
    }          
  }

  log_print(hlog, LOG_INFO, "dvb_madmusic() thread stopping");

  pthread_mutex_unlock(&data_mutex);
  pthread_exit(NULL);
}


/*******************************************************************************
** Function: dvb_parse_text()
**
** Description: The data contained in the OpenTV section has some
**              specific quirks that need to be worked around, and of
**              course the relevant data needs to be seperated from
**              the stuff we don't want and need. This is done here.
**
*******************************************************************************/

static void dvb_parse_text(unsigned char *buf, int len)
{
  int           field, ftna, toai;
  char          toan[32];
  unsigned char *p, *q, *r, *rr, wbuf[8192];

  if (len == mad_len) {
    if (memcmp(mad_buf, buf, len) == 0) {
      return;
    }
  }

  memcpy(mad_buf, buf, len);
  mad_len = len;
  time(&mad_time);

  toai = 0;
  toan[toai] = '\0';

  trnum = 0;
  memset(artist, 0x00, sizeof(artist));
  memset(title, 0x00, sizeof(title));
  memset(album, 0x00, sizeof(album));

  memset(wbuf, 0x00, sizeof(wbuf));
  memcpy(wbuf, buf, len);
  dvb_fixbillshit(wbuf);
  p     = wbuf;
  field = 0;

  ftna = 0;

  while (1) {
    q = index(p, '|');
    if (q == NULL) {
      break;
    }

    *q = '\0';
    q++;

    switch (field) {
      case 3:
        strcpy(artist, p);
        dvb_xlt(artist);
        break;

      case 4:
        strcpy(title, p);
        dvb_xlt(title);
        ftna = 1;
        break;

      case 5:
        strcpy(album, p);
        dvb_xlt(album);
        break;

      default:
        break;
    }

    if (ftna) {
      if ((r = strstr(p, title)) != NULL) {
        rr = r - 4;
        if (rr < p) {
          rr = p;
        }

        toai = 0;
        toan[toai] = '\0';
        while (rr < r) {
          if ((*rr >= '0') && (*rr <= '9')) {
            toan[toai++] = *rr;
          }
          rr++;
        }
        toan[toai] = '\0';
        trnum = atoi(toan);
      }
    }

    p = q;
    field++;

    if (p >= (wbuf + len)) {
      break;
    }

    if (*p == '^') {
      ftna = 0;
    }

  }

  log_print(hlog, LOG_INFO, "Album : %s", album);
  log_print(hlog, LOG_INFO, "Track : %d", trnum);
  log_print(hlog, LOG_INFO, "Artist: %s", artist);
  log_print(hlog, LOG_INFO, "Title : %s", title);

  if (strlen(artist) > 0) {
    sprintf(service_name, "%s - %s", artist, title);
  } else {
    sprintf(service_name, "%s", title);
  }
  si_update++;

  return;
}


/*******************************************************************************
** Function: dvb_fixbillshit()
**
** Description: Sorry, but I just couldn't hold back. Seems that the
**              MAD Music playout software is Windows-based, and thus
**              uses a greek Windows-Codepage, which, of course, is
**              almost completely identical to ISO Latin 7 -- but that
**              would be a standard, and M$ doesn't like standards, so
**              they shuffeled a few characters around. This function
**              undoes this mutilation.
**
*******************************************************************************/

static void dvb_fixbillshit(unsigned char *s)
{
  while (*s) {
    if (*s == 0xa1) {
      *s = 0xb5;
    } else {
      if (*s == 0xa2) {
        *s = 0xb6;
      }
    }

    s++;
  }
}


/*******************************************************************************
** Function: dvb_xlt()
**
** Description: Since data fields may contain CRs or LFs, we need to
**              get rid of these because we don't want them. They are
**              replaced with spaces here.
**
*******************************************************************************/

static void dvb_xlt(unsigned char *s)
{
  while (*s) {
    if ((*s == '\n') || (*s == '\r')) {
      *s = ' ';
    }
    s++;
  }
}
