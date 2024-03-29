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

#include <glib.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>

#include "dvb.h"
#include "log.h"
#include "util.h"

extern gpointer hlog;


// DiSEqC sleep intervals (see Eutelsat reference)
#define DISEQC_SHORT_WAIT       (15 * 1000)
#define DISEQC_LONG_WAIT       (100 * 1000)
#define DISEQC_POWER_OFF_WAIT (1000 * 1000)
#define DISEQC_POWER_ON_WAIT   (500 * 1000)


HDVB *
dvb_open (gint devnum)
{
  HDVB *h = g_malloc0 (sizeof (HDVB));
  if (h == NULL)
    return NULL;

  h->dvb_num = devnum;
  h->dvb_fe_info.type = -1;
  h->dvb_fedn = g_strdup_printf ("/dev/dvb/adapter%d/frontend0", h->dvb_num);
  if ((h->dvb_fedh = open (h->dvb_fedn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "open(%s) failed in dvb_open(), errno = %d (%s)",
		 h->dvb_fedn, errno, g_strerror (errno));
      g_free (h);
      return NULL;
    }
  h->ber_fail = h->snr_fail = h->str_fail = h->unc_fail = 0;

  return h;
}


gint
dvb_close (HDVB * h)
{
  if (h == NULL)
    return RC_NPE;

  if (h->dvb_fedh > 0)
    {
      if (h->dvb_fe_info.type == FE_QPSK)
	{
	  ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
	  //g_usleep (DISEQC_POWER_OFF_WAIT);
	}
      close (h->dvb_fedh);
    }
  if (h->dvb_admx > 0)
    close (h->dvb_admx);
  if (h->dvb_ddmx > 0)
    close (h->dvb_ddmx);
  g_free (h);

  return RC_OK;
}


gint
dvb_filter (HDVB * h, gint pid)
{
  if (h == NULL)
    return RC_NPE;

  if (h->dvb_dmxdn != NULL)
    g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_dmxdh = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "open(%s) failed in dvb_filter(), errno = %d (%s)",
		 h->dvb_dmxdn, errno, g_strerror (errno));
      h->dvb_dmxdh = 0;
      return RC_DVB_ERROR;
    }

  if (ioctl (h->dvb_dmxdh, DMX_SET_BUFFER_SIZE, 256 * 1024) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "DMX_SET_BUFFER_SIZE failed in dvb_filter(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_dmxdh);
      h->dvb_dmxdh = 0;
      return RC_DVB_ERROR;
    }

  h->dvb_dmx.pid = pid;
  h->dvb_dmx.input = DMX_IN_FRONTEND;
  h->dvb_dmx.output = DMX_OUT_TAP;
  h->dvb_dmx.pes_type = DMX_PES_OTHER;
  h->dvb_dmx.flags = DMX_IMMEDIATE_START;

  if (ioctl (h->dvb_dmxdh, DMX_SET_PES_FILTER, &h->dvb_dmx) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "DMX_SET_PES_FILTER failed in dvb_filter(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_dmxdh);
      h->dvb_dmxdh = 0;
      return RC_DVB_ERROR;
    }

  return RC_OK;
}


gint
dvb_unfilter (HDVB * h)
{
  if (h == NULL)
    return RC_NPE;

  if (h->dvb_dmxdh <= 0)
    return RC_OK;

  if (ioctl (h->dvb_dmxdh, DMX_STOP) < 0)
    return RC_OK;

  close (h->dvb_dmxdh);
  h->dvb_dmxdh = 0;
  return RC_OK;
}


gint
dvb_section (HDVB * h, gint pid, gint sect, gint sid, gint sct,
	     guchar * s, gint t)
{
  gint sel, r, fd;
  fd_set fds;
  struct timeval tv;
  struct dmx_sct_filter_params fp;

  if (h == NULL)
    return RC_NPE;

  if (h->dvb_dmxdn)
    g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((fd = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "open(%s) failed in dvb_section(), errno = %d (%s)",
		 h->dvb_dmxdn, errno, g_strerror (errno));
      return RC_DVB_ERROR;
    }

  memset (&fp, 0x00, sizeof (fp));
  fp.pid = pid;
  fp.timeout = 0;
  fp.flags = DMX_IMMEDIATE_START | DMX_ONESHOT | DMX_CHECK_CRC;

  if (sect != -1)
    {
      fp.filter.filter[0] = sect & 0xff;
      fp.filter.mask[0] = 0xff;

      if (sect == 2)
	{
	  fp.filter.filter[1] = (sid >> 8) & 0xff;
	  fp.filter.filter[2] = sid & 0xff;
	  fp.filter.mask[1] = 0xff;
	  fp.filter.mask[2] = 0xff;
	}

      if (sect == 0x42)
	{
	  fp.filter.filter[4] = sct & 0xff;
	  fp.filter.mask[4] = 0xff;
	}

      if (sect == 0x4e)
	{
	  fp.filter.filter[1] = (sid >> 8) & 0xff;
	  fp.filter.filter[2] = sid & 0xff;
	  fp.filter.mask[1] = 0xff;
	  fp.filter.mask[2] = 0xff;
	  fp.filter.filter[4] = sct & 0xff;
	  fp.filter.mask[4] = 0xff;
	}
    }

  if (ioctl (fd, DMX_SET_FILTER, &fp) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "DMX_SET_FILTER failed in dvb_section(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (fd);
      return RC_DVB_ERROR;
    }

  tv.tv_sec = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO (&fds);
  FD_SET (fd, &fds);

  do
    {
      sel = select (fd + 1, &fds, NULL, NULL, &tv);
    }
  while (sel < 0 && errno == EINTR);

  if (sel < 0)
    {
      log_print (hlog, LOG_ERR,
		 "select() failed in dvb_section(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (fd);
      return RC_DVB_ERROR;
    }

  if (sel == 0)
    {
      log_print (hlog, LOG_DEBUG, "select() timed out in dvb_section()");
      close (fd);
      return RC_DVB_TIMEOUT;
    }

  if ((r = read (fd, s, 4096)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "read() failed in dvb_section(), errno = %d (%s)", errno,
		 g_strerror (errno));
      close (fd);
      return RC_DVB_ERROR;
    }

  close (fd);
  return RC_OK;
}


gint
dvb_apid (HDVB * h, guint pid)
{
  struct dmx_pes_filter_params fp;
  gint rc;

  if (h == NULL)
    return RC_NPE;

  if (h->dvb_dmxdn)
    g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_admx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "open() failed in dvb_apid(), errno = %d (%s)", errno,
		 g_strerror (errno));
      h->dvb_admx = 0;
      return RC_DVB_ERROR;
    }

  if ((rc = ioctl (h->dvb_admx, DMX_SET_BUFFER_SIZE, 256 * 1024)) < 0)
    {
      log_print (hlog, LOG_WARN,
		 "DMX_SET_BUFFER_SIZE failed in dvb_apid(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_admx);
      h->dvb_admx = 0;
      return RC_DVB_ERROR;
    }

  memset (&fp, 0x00, sizeof (fp));
  fp.pid = pid;
  fp.input = DMX_IN_FRONTEND;
  fp.output = DMX_OUT_TAP;
  fp.pes_type = DMX_PES_AUDIO;
  fp.flags = DMX_IMMEDIATE_START;

  if ((rc = ioctl (h->dvb_admx, DMX_SET_PES_FILTER, &fp)) < 0)
    {
      log_print (hlog, LOG_WARN,
		 "DMX_SET_PES_FILTER failed in dvb_apid(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_admx);
      h->dvb_admx = 0;
      return RC_DVB_ERROR;
    }

  return RC_OK;
}


gint
dvb_apkt (HDVB * h, guchar * pkt, guint len, guint t, gint * rcvd)
{
  gint r, sel;
  fd_set rfd;
  struct timeval tv;

  if (h == NULL)
    return RC_NPE;

  memset (&tv, 0x00, sizeof (tv));
  tv.tv_sec = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO (&rfd);
  FD_SET (h->dvb_admx, &rfd);

  do
    {
      sel = select (h->dvb_admx + 1, &rfd, NULL, NULL, &tv);
    }
  while (sel < 0 && errno == EINTR);

  if (sel < 0)
    {
      log_print (hlog, LOG_ERR,
		 "select() failed in dvb_apkt(), errno = %d (%s)", errno,
		 g_strerror (errno));
      return RC_DVB_ERROR;
    }

  if (sel == 0)
    {
      log_print (hlog, LOG_DEBUG, "select() timed out in dvb_apkt()");
      return RC_DVB_TIMEOUT;
    }

  if ((r = read (h->dvb_admx, pkt, len)) <= 0)
    {
      log_print (hlog, LOG_ERR,
		 "read() failed in dvb_apkt(), errno = %d (%s)", errno,
		 g_strerror (errno));
      return RC_DVB_ERROR;
    }

  *rcvd = r;
  return RC_OK;
}


gint
dvb_dpid (HDVB * h, guint pid)
{
  struct dmx_sct_filter_params fp;
  struct dmx_pes_filter_params pfp;

  if (h == NULL)
    return RC_NPE;

  if (h->dvb_dmxdn)
    g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_ddmx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "open() failed in dvb_dpid(), errno = %d (%s)", errno,
		 g_strerror (errno));
      h->dvb_ddmx = 0;
      return RC_DVB_ERROR;
    }

  if (ioctl (h->dvb_ddmx, DMX_SET_BUFFER_SIZE, 131072) < 0)
    {
      log_print (hlog, LOG_WARN,
		 "DMX_SET_BUFFER_SIZE failed in dvb_dpid(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_ddmx);
      h->dvb_ddmx = 0;
      return RC_DVB_ERROR;
    }

  memset (&pfp, 0x00, sizeof (pfp));
  pfp.pid = pid;
  pfp.input = DMX_IN_FRONTEND;
  pfp.output = DMX_OUT_TAP;
  pfp.pes_type = DMX_PES_AUDIO1;
  pfp.flags = DMX_IMMEDIATE_START;

  if (ioctl (h->dvb_ddmx, DMX_SET_PES_FILTER, &pfp) < 0)
    {
      log_print (hlog, LOG_WARN,
		 "DMX_SET_PES_FILTER failed in dvb_dpid(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_ddmx);
      h->dvb_ddmx = 0;
      return RC_DVB_ERROR;
    }

  memset (&fp, 0x00, sizeof (fp));
  fp.pid = pid;
  fp.timeout = 0;
  fp.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;
  fp.filter.filter[0] = 0x80;
  fp.filter.mask[0] = 0xff;
  fp.filter.filter[1] = 0x00;
  fp.filter.mask[1] = 0xff;
  fp.filter.filter[2] = 0x02;
  fp.filter.mask[2] = 0xff;

  if (ioctl (h->dvb_ddmx, DMX_SET_FILTER, &fp) < 0)
    {
      log_print (hlog, LOG_WARN,
		 "DMX_SET_FILTER failed in dvb_dpid(), errno = %d (%s)",
		 errno, g_strerror (errno));
      close (h->dvb_ddmx);
      h->dvb_ddmx = 0;
      return RC_DVB_ERROR;
    }

  return RC_OK;
}


gint
dvb_dpkt (HDVB * h, guchar * s, gint len, gint t, gint * rcvd)
{
  gint r, sel;
  fd_set fds;
  struct timeval tv;

  if (h == NULL)
    return RC_NPE;

  memset (&tv, 0x00, sizeof (tv));
  tv.tv_sec = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO (&fds);
  FD_SET (h->dvb_ddmx, &fds);

  do
    {
      sel = select (h->dvb_ddmx + 1, &fds, NULL, NULL, &tv);
    }
  while (sel < 0 && errno == EINTR);

  if (sel < 0)
    {
      log_print (hlog, LOG_ERR,
		 "select() failed in dvb_dpkt(), errno = %d (%s)", errno,
		 g_strerror (errno));
      return RC_DVB_ERROR;
    }

  if (sel == 0)
    {
      log_print (hlog, LOG_DEBUG, "select() timed out in dvb_dpkt()");
      return RC_DVB_TIMEOUT;
    }

  if ((r = read (h->dvb_ddmx, s, len)) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "read() failed in dvb_dpkt(), errno = %d (%s)", errno,
		 g_strerror (errno));
      return RC_DVB_ERROR;
    }

  *rcvd = r;
  return RC_OK;
}


gint
dvb_get_pid (HDVB * h, gint s, guint * apid, guint * dpid)
{
  gint rc, len, pmt, pil, es, es_type, es_audio, es_data;
  guint sid;
  guchar sct[4096], *p, *q;

  if (h == NULL)
    return RC_NPE;

  if ((rc = dvb_section (h, 0, 0, 0, 0, sct, 10000)) != RC_OK)
    {
      log_print (hlog, LOG_ERR,
		 "dvb_section() failed in dvb_get_pid(), rc = %d", rc);
      return rc;
    }

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
	  if ((rc = dvb_section (h, pmt, 2, sid, 0, sct, 10000)) != RC_OK)
	    {
	      log_print (hlog, LOG_ERR,
			 "dvb_section() failed in dvb_get_pid(), rc = %d",
			 rc);
	      return rc;
	    }

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
		  log_print (hlog, LOG_DEBUG,
			  "Service Audio PID = %d (0x%04x)", es, es);
		  if (es_audio < 0)
		    es_audio = es;
		}

	      if (es_type == 0x05)
		{
		  log_print (hlog, LOG_DEBUG,
			  "Service Data PID = %d (0x%04x)", es, es);
		  if (es_data < 0)
		    es_data = es;
		}

	      p += pil;
	    }
	}

      p += 4;
    }

  if (apid != NULL && es_audio < 0)
    {
      log_print (hlog, LOG_ERR,
		 "No corresponding Audio PID to SID %d found in PAT.", s);
      return RC_DVB_ERROR;
    }
  if (apid != NULL)
    {
      log_print (hlog, LOG_INFO,
		 "Setting Audio PID to %d (0x%04x)", es_audio, es_audio);
      *apid = es_audio;
    }
  if (dpid != NULL)
    {
      log_print (hlog, LOG_INFO,
		 "Setting Data PID to %d (0x%04x)", es_data, es_data);
      *dpid = es_data;
    }

  return RC_OK;
}


static gint
diseqc_send_msg (const HDVB * h, fe_sec_voltage_t v,
		 struct dvb_diseqc_master_cmd cmd, fe_sec_tone_mode_t t,
		 guchar sat_no)
{
  if (h == NULL)
    return RC_NPE;

  if (ioctl (h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "FE_SET_TONE failed in diseqc_send_msg(), errno = %d (%s)",
		 errno, g_strerror (errno));
      return RC_DVB_ERROR;
    }
  g_usleep (DISEQC_SHORT_WAIT);

  if (ioctl (h->dvb_fedh, FE_SET_VOLTAGE, v) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "FE_SET_VOLTAGE failed in diseqc_send_msg(), errno = %d (%s)",
		 errno, g_strerror (errno));
      return RC_DVB_ERROR;
    }
  g_usleep (v ==
	    SEC_VOLTAGE_OFF ? DISEQC_POWER_OFF_WAIT : DISEQC_POWER_ON_WAIT);

  if (sat_no >= 1 && sat_no <= 4)	// 1.x compatible DiSEqC
    {
      if (ioctl (h->dvb_fedh, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
	{
	  log_print (hlog, LOG_ERR,
		     "FE_DISEQC_SEND_MASTER_CMD failed in diseqc_send_msg(), errno = %d (%s)",
		     errno, g_strerror (errno));
	  return RC_DVB_ERROR;
	}
    }
  else				// A or B simple DiSEqC
    {
      log_print (hlog, LOG_INFO, "Setting simple %c burst", sat_no);
      if (ioctl (h->dvb_fedh, FE_DISEQC_SEND_BURST,
		 (sat_no == 'B' ? SEC_MINI_B : SEC_MINI_A)) < 0)
	{
	  log_print (hlog, LOG_ERR,
		     "FE_DISEQC_SEND_BURST failed in diseqc_send_msg(), errno = %d (%s)",
		     errno, g_strerror (errno));
	  return RC_DVB_ERROR;
	}
    }
  g_usleep (DISEQC_SHORT_WAIT);

  if (ioctl (h->dvb_fedh, FE_SET_TONE, t) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "FE_SET_TONE failed in diseqc_send_msg(), errno = %d (%s)",
		 errno, g_strerror (errno));
      return RC_DVB_ERROR;
    }
  g_usleep (DISEQC_SHORT_WAIT);

  return RC_OK;
}


static gint
do_diseqc (const HDVB * h, guchar sat_no, gint polv, gint hi_lo)
{
  struct dvb_diseqc_master_cmd cmd =
    { {0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4 };

  if (h == NULL)
    return RC_NPE;

  if (sat_no != 0)
    {
      // param: high nibble: reset bits, low nibble: set bits,
      // bits are: option, position, polarizaion, band
      cmd.msg[3] =
	0xf0 | (((sat_no * 4) & 0x0f) | (polv ? 0 : 2) | (hi_lo ? 1 : 0));

      return diseqc_send_msg (h, polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
			      cmd, hi_lo ? SEC_TONE_ON : SEC_TONE_OFF,
			      sat_no);
    }
  else
    {
      log_print (hlog, LOG_INFO, "Setting only tone %s and voltage %dV",
		 (hi_lo ? "ON" : "OFF"), (polv ? 13 : 18));

      if (ioctl
	  (h->dvb_fedh, FE_SET_VOLTAGE,
	   (polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18)) < 0)
	{
	  log_print (hlog, LOG_ERR,
		     "FE_SET_VOLTAGE failed in do_diseqc(), errno = %d (%s)",
		     errno, g_strerror (errno));
	  return RC_DVB_ERROR;
	}
      g_usleep (DISEQC_POWER_ON_WAIT);

      if (ioctl
	  (h->dvb_fedh, FE_SET_TONE,
	   (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
	{
	  log_print (hlog, LOG_ERR,
		     "FE_SET_TONE failed in do_diseqc(), errno = %d (%s)",
		     errno, g_strerror (errno));
	  return RC_DVB_ERROR;
	}
      g_usleep (DISEQC_SHORT_WAIT);
    }

  return RC_OK;
}


gint
dvb_read_fe_safe (gint dvb_fedh, guint * failcnt, guint * val, gulong req, const gchar * reqname)
{
  gint rc;
  if (*failcnt > MAX_STATUS_ERR)
    return RC_DVB_ERROR;
  if ((rc = ioctl (dvb_fedh, req, val)) < 0)
    {
      *val = -2;
      if (++(*failcnt) > MAX_STATUS_ERR)
        log_print (hlog, LOG_WARN, "%s failed (rc = %d) "
                   "too often in dvb_read_fe_safe(), disabling.", reqname, rc);
      else
        log_print (hlog, LOG_WARN, "%s failed (rc = %d) "
                   "in dvb_read_fe_safe().", reqname, rc);
      return RC_DVB_ERROR;
    }
  *failcnt = 0;
  return RC_OK;
}


static gint
check_status (HDVB * h, gint type,
	      const struct dvb_frontend_parameters *feparams, guint base)
{
  guint val;
  fe_status_t festatus;
  struct pollfd pfd[1];
  gint locks = 0, ok = 0;
  time_t tm1, tm2;

  if (h == NULL)
    return RC_NPE;

  if (ioctl (h->dvb_fedh, FE_SET_FRONTEND, feparams) < 0)
    {
      log_print (hlog, LOG_ERR, "FE_SET_FRONTEND failed in check_status(),"
		 " errno = %d (%s)", errno, g_strerror (errno));
      return RC_DVB_ERROR;
    }

  memset (&pfd, 0x00, sizeof (pfd));
  pfd[0].fd = h->dvb_fedh;
  pfd[0].events = POLLPRI;

  tm1 = tm2 = time ((time_t *) NULL);
  log_print (hlog, LOG_DEBUG, "Getting frontend status");
  while (!ok)
    {
      festatus = 0;
      if (poll (pfd, 1, 3000) > 0)
	{
	  if (pfd[0].revents & POLLPRI)
	    {
	      if (ioctl (h->dvb_fedh, FE_READ_STATUS, &festatus) >= 0)
		if (festatus & FE_HAS_LOCK)
		  locks++;
	    }
	}

      g_usleep (10000);

      if ((festatus & FE_TIMEDOUT) || (locks >= 2)
	  || (time (NULL) - tm1 >= 3))
	ok = 1;
    }

  if ((festatus & FE_HAS_LOCK) == 0)
    {
      log_print (hlog, LOG_ERR,
		 "Not able to lock to the signal on the given frequency!");
      return RC_DVB_ERROR;
    }

  if (ioctl (h->dvb_fedh, FE_GET_FRONTEND, feparams) >= 0)
    {
      switch (type)
	{
	case FE_OFDM:
	  log_print (hlog, LOG_INFO, "Event:  Frequency: %d",
		     feparams->frequency);
	  break;
	case FE_QPSK:
	  log_print (hlog, LOG_INFO, "Event:  Frequency: %d",
		     (unsigned int) (feparams->frequency + base));
	  log_print (hlog, LOG_INFO, "        SymbolRate: %d",
		     feparams->u.qpsk.symbol_rate);
	  log_print (hlog, LOG_INFO, "        FEC_inner:  %d",
		     feparams->u.qpsk.fec_inner);
	  break;
	case FE_QAM:
	  log_print (hlog, LOG_INFO, "Event:  Frequency: %d",
		     feparams->frequency);
	  log_print (hlog, LOG_INFO, "        SymbolRate: %d",
		     feparams->u.qpsk.symbol_rate);
	  log_print (hlog, LOG_INFO, "        FEC_inner:  %d",
		     feparams->u.qpsk.fec_inner);
	  break;
	case FE_ATSC:
#ifdef DVB_ATSC
	  log_print (hlog, LOG_INFO, "Event:  Frequency: %d",
		     feparams->frequency);
	  log_print (hlog, LOG_INFO, "        Modulation: %d",
		     feparams->u.vsb.modulation);
	  break;
#endif
	default:
	  log_print (hlog, LOG_INFO, "Unknown FE type.");
	}
    }

  if (dvb_read_fe_safe (h->dvb_fedh, &h->str_fail, &val,
                    FE_READ_SIGNAL_STRENGTH, "FE_READ_SIGNAL_STRENGTH") == RC_OK)
    log_print (hlog, LOG_DEBUG, "Signal strength: %d", val);
  if (dvb_read_fe_safe (h->dvb_fedh, &h->snr_fail, &val,
                    FE_READ_SNR, "FE_READ_SNR") == RC_OK)
    log_print (hlog, LOG_DEBUG, "SNR: %d", val);
  if (dvb_read_fe_safe (h->dvb_fedh, &h->ber_fail, &val,
                    FE_READ_BER, "FE_READ_BER") == RC_OK)
    log_print (hlog, LOG_DEBUG, "Bit error rate: %d", val);
  if (dvb_read_fe_safe (h->dvb_fedh, &h->unc_fail, &val,
                    FE_READ_UNCORRECTED_BLOCKS, "FE_READ_UNCORRECTED_BLOCKS") == RC_OK)
    log_print (hlog, LOG_DEBUG, "UNC: %d", val);

  log_print (hlog, LOG_DEBUG, "FE_STATUS:%s%s%s%s%s%s",
	     (festatus & FE_HAS_SIGNAL ? " FE_HAS_SIGNAL" : ""),
	     (festatus & FE_TIMEDOUT ? " FE_TIMEDOUT" : ""),
	     (festatus & FE_HAS_LOCK ? " FE_HAS_LOCK" : ""),
	     (festatus & FE_HAS_CARRIER ? " FE_HAS_CARRIER" : ""),
	     (festatus & FE_HAS_VITERBI ? " FE_HAS_VITERBI" : ""),
	     (festatus & FE_HAS_SYNC ? " FE_HAS_SYNC" : ""));

  return RC_OK;
}


gint
dvb_get_status (HDVB * h, dvbstatstruct * st)
{
  guint status;
  dvbstatstruct _st;

  if (h == NULL || st == NULL)
    return RC_NPE;

  if (ioctl (h->dvb_fedh, FE_READ_STATUS, &status) == -1)
    {
      log_print (hlog, LOG_WARN,
		 "FE_READ_STATUS failed in dvb_get_status().");
      return RC_DVB_ERROR;
    }

  // Fill in values (if we can get them)
  memset (&_st, 0, sizeof (dvbstatstruct));
  _st.signal = (status & FE_HAS_SIGNAL);
  _st.carrier = (status & FE_HAS_CARRIER);
  _st.viterbi = (status & FE_HAS_VITERBI);
  _st.sync = (status & FE_HAS_SYNC);
  _st.lock = (status & FE_HAS_LOCK);
  _st.timedout = (status & FE_TIMEDOUT);
  dvb_read_fe_safe (h->dvb_fedh, &h->str_fail, &_st.str,
                    FE_READ_SIGNAL_STRENGTH, "FE_READ_SIGNAL_STRENGTH");
  dvb_read_fe_safe (h->dvb_fedh, &h->snr_fail, &_st.snr,
                    FE_READ_SNR, "FE_READ_SNR");
  dvb_read_fe_safe (h->dvb_fedh, &h->ber_fail, &_st.ber,
                    FE_READ_BER, "FE_READ_BER");
  dvb_read_fe_safe (h->dvb_fedh, &h->unc_fail, &_st.unc,
                    FE_READ_UNCORRECTED_BLOCKS, "FE_READ_UNCORRECTED_BLOCKS");
  _st.refresh = TRUE;
  memcpy (st, &_st, sizeof (dvbstatstruct));

  log_print (hlog, LOG_DEBUG, "status %02x | signal %04x | snr %04x | "
	     "ber %08x | unc %08x | ", status, st->str, st->snr, st->ber,
	     st->unc);

  return RC_OK;
}


gint
dvb_tune (HDVB * h, tunestruct * t)
{
  gint res, hi_lo = 0;
  guint base = 0;
  gchar *tunetext = NULL;
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;

  if (h == NULL)
    return RC_NPE;

  // Do we already know the frontend type (DVB-S/-C/-T)?
  if (h->dvb_fe_info.type == -1)
    {
      // Try to detect frontend type
      if ((res = ioctl (h->dvb_fedh, FE_GET_INFO, &fe_info) < 0))
	{
	  log_print (hlog, LOG_ERR, "FE_GET_INFO failed in dvb_tune(),"
		     " errno = %d (%s)", errno, g_strerror (errno));
	  return RC_DVB_ERROR;
	}
      memcpy (&(h->dvb_fe_info), &fe_info, sizeof (struct dvb_frontend_info));
    }

  log_print (hlog, LOG_INFO, "Using DVB card '%s'", fe_info.name);
  tunetext = dvb_tune_to_text (h, t);

  if (t->freq < 1000000)
    t->freq *= 1000UL;

  memset (&feparams, 0x00, sizeof (feparams));
  switch (fe_info.type)
    {
    case FE_OFDM:
      feparams.frequency = t->freq;
      feparams.inversion = INVERSION_OFF;
      feparams.u.ofdm.bandwidth = t->bandw;
      feparams.u.ofdm.code_rate_HP = t->hpcr;
      feparams.u.ofdm.code_rate_LP = t->lpcr;
      feparams.u.ofdm.constellation = t->mod;
      feparams.u.ofdm.transmission_mode = t->tmode;
      feparams.u.ofdm.guard_interval = t->gival;
      feparams.u.ofdm.hierarchy_information = t->hier;
      log_print (hlog, LOG_INFO, "tuning DVB-T to %s", tunetext);
      g_free (tunetext);
      break;
    case FE_QPSK:
      if (t->freq > 2200000)
	{
	  base = (t->freq < t->slof ? t->lof1 : t->lof2);
	  hi_lo = (t->freq < t->slof ? 0 : 1);
	}
      feparams.frequency = (t->freq - base);

      log_print (hlog, LOG_INFO, "tuning DVB-S to %s", tunetext);
      g_free (tunetext);
      feparams.inversion = t->sinv;
      feparams.u.qpsk.symbol_rate = t->srate;
      feparams.u.qpsk.fec_inner = FEC_AUTO;

      if (do_diseqc (h, t->diseqc, (t->pol == 'V' ? 1 : 0), hi_lo) != RC_OK)
	{
	  log_print (hlog, LOG_ERR, "DiSEqC setting failed");
	  return RC_DVB_ERROR;
	}

      log_print (hlog, LOG_INFO, "DiSEqC setting succeeded");
      break;
    case FE_QAM:
      log_print (hlog, LOG_INFO, "tuning DVB-C to %s", tunetext);
      g_free (tunetext);
      feparams.frequency = t->freq;
      feparams.inversion = INVERSION_OFF;
      feparams.u.qam.symbol_rate = t->srate;
      feparams.u.qam.fec_inner = FEC_AUTO;
      feparams.u.qam.modulation = t->mod;
      break;
    case FE_ATSC:
#ifdef DVB_ATSC
      log_print (hlog, LOG_INFO, "tuning ATSC to %s", tunetext);
      g_free (tunetext);
      feparams.frequency = t->freq;
      feparams.u.vsb.modulation = t->mod;
      break;
#endif
    default:
      log_print (hlog, LOG_ERR, "Unknown FE type. Aborting");
      return RC_DVB_ERROR;
    }

  return check_status (h, fe_info.type, &feparams, base);
}


gchar *
dvb_tune_to_text (const HDVB * h, const tunestruct * t)
{
  gchar *text = NULL;

  if (h == NULL)
    return NULL;

  switch (h->dvb_fe_info.type)
    {
    case FE_OFDM:
      text = g_strdup_printf ("%lu Hz, Bandwidth: %d, Modulation: %d",
			      t->freq, t->bandw == BANDWIDTH_8_MHZ ? 8 :
			      (t->bandw == BANDWIDTH_7_MHZ ? 7 : 6), t->mod);
      break;
    case FE_QPSK:
      text = g_strdup_printf ("%lu kHz, Pol: %c, Srate: %lu, 22kHz Tone: %s, "
			      "LNB: %d, SLOF: %lu, LOF1: %lu, LOF2: %lu",
			      t->freq, t->pol, t->srate,
			      (t->freq >= t->slof) ? "ON" : "OFF", t->diseqc,
			      t->slof / 1000UL, t->lof1 / 1000UL,
			      t->lof2 / 1000UL);
      break;
    case FE_QAM:
      text =
	g_strdup_printf ("%lu Hz, Srate: %lu, Modulation: %d", t->freq,
			 t->srate, t->mod);
      break;
    case FE_ATSC:
#ifdef DVB_ATSC
      text = g_strdup_printf ("%lu Hz, Modulation: %d", t->freq, t->mod);
#endif
      break;
    }

  return text;
}

gchar *
dvb_get_authority_from_url (const gchar * url)
{
  // Our URLs always have syntax "dvb://..."
  if (url == NULL || !g_str_has_prefix (url, "dvb://"))
    return NULL;

  // Extract authority-part (substring between dvb:// and ?)
  const gchar *substr_start, *substr_end;
  substr_start = g_strstr_len (url, 50, "//");
  if (substr_start == NULL)
    return NULL;
  substr_start += 2;
  substr_end = g_strstr_len (substr_start, 50, "?");
  if (substr_end == NULL)
    return NULL;
  if (substr_end-substr_start == 0)
    return NULL;

  return g_uri_unescape_segment (substr_start, substr_end, "");
}

gboolean
dvb_tune_check_url (const gchar * url)
{
  if (url == NULL)
    return FALSE;

  gchar *auth = dvb_get_authority_from_url (url);
  if (auth == NULL)
    return FALSE;

  g_free (auth);
  return TRUE;
}

gint
dvb_tune_parse_url (const gchar * url, gchar ** authptr, tunestruct * tune)
{
  gint i;
  tunestruct *t = NULL;
  gchar ch;

  if (url == NULL || &tune == NULL)
    return RC_NPE;

  // Get authority-part of URL
  gchar *auth = dvb_get_authority_from_url (url);
  if (auth == NULL)
    return RC_DVB_ERROR;
  if (authptr != NULL)
    *authptr = auth;
  else
    g_free (auth);

  // Initialize temporary tuning structure
  t = dvb_tune_init ();

  // Parse each (parameter=value)-pair
  gchar *substr = g_strstr_len (url, 50, "?");
  gchar **args = g_strsplit (substr+1, ":", 0);
  for (i = 0; args[i]; ++i)
    {
      gchar **pair = g_strsplit (args[i], "=", 2);
      gchar *par = pair[0];
      gchar *val = pair[1];

      if (par == NULL || val == NULL)
	{
	  g_strfreev (pair);
	  g_strfreev (args);
	  dvb_tune_exit (t);
	  return RC_NPE;
	}

      if (g_ascii_strcasecmp (par, "sid") == 0)
	{
	  // Service ID
	  t->sid = atol (val);
	}
      else if (g_ascii_strcasecmp (par, "apid") == 0)
	{
	  // Audio PID
	  t->apid = atol (val);
	}
      else if (g_ascii_strcasecmp (par, "dpid") == 0)
	{
	  // Data PID
	  t->dpid = atol (val);
	}
      else if (g_ascii_strcasecmp (par, "freq") == 0)
	{
	  // Frequency of transponder (DVB-T/-C: in Hz, DVB-S: in kHz)
	  t->freq = atol (val);
	}
      else if (g_ascii_strcasecmp (par, "pol") == 0)
	{
	  // Polarisation (DVB-S)
	  if (val[1] == 0)
	    {
	      ch = g_ascii_toupper (val[0]);
	      switch (ch)
		{
		case 'H':
		case 'V':
		  t->pol = ch;
		  break;
		default:
		  log_print (hlog, LOG_ERR, "Invalid polarisation value '%c'",
			     ch);
		}
	    }
	}
      else if (g_ascii_strcasecmp (par, "slof") == 0)
	{
	  // Switch frequency of LNB (DVB-S)
	  t->slof = atol (val) * 1000UL;
	}
      else if (g_ascii_strcasecmp (par, "lof1") == 0)
	{
	  // Local frequency of lower LNB band (DVB-S)
	  t->lof1 = atol (val) * 1000UL;
	}
      else if (g_ascii_strcasecmp (par, "lof2") == 0)
	{
	  // Local frequency of upper LNB band (DVB-S)
	  t->lof2 = atol (val) * 1000UL;
	}
      else if (g_ascii_strcasecmp (par, "srate") == 0)
	{
	  // Symbol rate in symbols per second (DVB-S/-T/-C)
	  t->srate = atol (val) * 1000UL;
	}
      else if (g_ascii_strcasecmp (par, "diseqc") == 0)
	{
	  // DiSEqC address of the used LNB (DVB-S)
	  if (val[1] == 0)
	    {
	      ch = g_ascii_toupper (val[0]);
	      switch (ch)
		{
		case 'A':
		case 'B':
		  t->diseqc = ch;
		  break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		  t->diseqc = g_ascii_digit_value (ch);
		  break;
		default:
		  log_print (hlog, LOG_ERR, "Invalid DiSEqC address '%c'",
			     ch);
		}
	    }
	}
      else if (g_ascii_strcasecmp (par, "sinv") == 0)
	{
	  // Spectral inversion (DVB-S)
	  switch (atol (val))
	    {
	    case 0:
	      t->sinv = INVERSION_OFF;
	      break;
	    case 1:
	      t->sinv = INVERSION_ON;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid spectral inversion '%s'",
			 val);
	    }
	}
      else if (g_ascii_strcasecmp (par, "qam") == 0)
	{
	  // Quadrature modulation (DVB-T/-C)
	  switch (atol (val))
	    {
	    case 16:
	      t->mod = QAM_16;
	      break;
	    case 32:
	      t->mod = QAM_32;
	      break;
	    case 64:
	      t->mod = QAM_64;
	      break;
	    case 128:
	      t->mod = QAM_128;
	      break;
	    case 256:
	      t->mod = QAM_256;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid QAM value '%s'", val);
	    }
	}
#ifdef DVB_ATSC
      else if (g_ascii_strcasecmp (par, "vsb") == 0)
	{
	  // Vestigial sideband modulation (ATSC)
	  switch (atol (val))
	    {
	    case 8:
	      t->mod = VSB_8;
	      break;
	    case 16:
	      t->mod = VSB_16;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid ATSC VSB modulation '%s'",
			 val);
	    }
	}
#endif
      else if (g_ascii_strcasecmp (par, "gival") == 0)
	{
	  // Guard interval (DVB-T)
	  switch (atol (val))
	    {
	    case 32:
	      t->gival = GUARD_INTERVAL_1_32;
	      break;
	    case 16:
	      t->gival = GUARD_INTERVAL_1_16;
	      break;
	    case 8:
	      t->gival = GUARD_INTERVAL_1_8;
	      break;
	    case 4:
	      t->gival = GUARD_INTERVAL_1_4;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid guard interval '%s'", val);
	    }
	}
      else if (g_ascii_strcasecmp (par, "tmode") == 0)
	{
	  // Transmission mode (DVB-T)
	  switch (atol (val))
	    {
	    case 8:
	      t->tmode = TRANSMISSION_MODE_8K;
	      break;
	    case 2:
	      t->tmode = TRANSMISSION_MODE_2K;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid transmission mode '%s'",
			 val);
	    }
	}
      else if (g_ascii_strcasecmp (par, "bandw") == 0)
	{
	  // Bandwidth (DVB-T)
	  switch (atol (val))
	    {
	    case 8:
	      t->bandw = BANDWIDTH_8_MHZ;
	      break;
	    case 7:
	      t->bandw = BANDWIDTH_7_MHZ;
	      break;
	    case 6:
	      t->bandw = BANDWIDTH_6_MHZ;
	      break;
	    default:
	      log_print (hlog, LOG_ERR, "Invalid DVB-T bandwidth '%s'", val);
	    }
	}
      else if (g_ascii_strcasecmp (par, "hpcr") == 0)
	{
	  // (High priority) Stream code rate (DVB-S/-T/-C)
	  if (g_ascii_strcasecmp (val, "NONE") == 0)
	    t->hpcr = FEC_NONE;
	  else if (g_ascii_strcasecmp (val, "AUTO") == 0)
	    t->hpcr = FEC_AUTO;
	  else if (g_ascii_strcasecmp (val, "1_2") == 0)
	    t->hpcr = FEC_1_2;
	  else if (g_ascii_strcasecmp (val, "2_3") == 0)
	    t->hpcr = FEC_2_3;
	  else if (g_ascii_strcasecmp (val, "3_4") == 0)
	    t->hpcr = FEC_3_4;
	  else if (g_ascii_strcasecmp (val, "5_6") == 0)
	    t->hpcr = FEC_5_6;
	  else if (g_ascii_strcasecmp (val, "7_8") == 0)
	    t->hpcr = FEC_7_8;
	  else
	    log_print (hlog, LOG_ERR, "Invalid code rate '%s'", val);
	}
      else if (g_ascii_strcasecmp (par, "lpcr") == 0)
	{
	  // Low priority stream code rate (DVB-T)
	  if (g_ascii_strcasecmp (val, "NONE") == 0)
	    t->lpcr = FEC_NONE;
	  else if (g_ascii_strcasecmp (val, "AUTO") == 0)
	    t->lpcr = FEC_AUTO;
	  else if (g_ascii_strcasecmp (val, "1_2") == 0)
	    t->lpcr = FEC_1_2;
	  else if (g_ascii_strcasecmp (val, "2_3") == 0)
	    t->lpcr = FEC_2_3;
	  else if (g_ascii_strcasecmp (val, "3_4") == 0)
	    t->lpcr = FEC_3_4;
	  else if (g_ascii_strcasecmp (val, "5_6") == 0)
	    t->lpcr = FEC_5_6;
	  else if (g_ascii_strcasecmp (val, "7_8") == 0)
	    t->lpcr = FEC_7_8;
	  else
	    log_print (hlog, LOG_ERR, "Invalid LP code rate '%s'", val);
	}
      else if (g_ascii_strcasecmp (par, "hier") == 0)
	{
	  // Hierarchy (DVB-T)
	  if (g_ascii_strcasecmp (val, "NONE") == 0)
	    t->hier = HIERARCHY_NONE;
	  else if (g_ascii_strcasecmp (val, "AUTO") == 0)
	    t->hier = HIERARCHY_AUTO;
	  else if (g_ascii_strcasecmp (val, "1") == 0)
	    t->hier = HIERARCHY_1;
	  else if (g_ascii_strcasecmp (val, "2") == 0)
	    t->hier = HIERARCHY_2;
	  else if (g_ascii_strcasecmp (val, "4") == 0)
	    t->hier = HIERARCHY_4;
	  else
	    log_print (hlog, LOG_ERR, "Invalid hierarchy value '%s'", val);
	}
      else
	log_print (hlog, LOG_ERR, "Unknown parameter '%s' (with value '%s')",
		   par, val);

      g_strfreev (pair);
    }
  g_strfreev (args);

  if (t->freq == 0)
    {
      dvb_tune_exit (t);
      return RC_DVB_ERROR;
    }

  if (tune != NULL)
    memcpy (tune, t, sizeof (tunestruct));
  dvb_tune_exit (t);

  return RC_OK;
}


tunestruct *
dvb_tune_init (void)
{
  tunestruct *t;
  t = g_malloc0 (sizeof (tunestruct));
  if (t == NULL)
    return NULL;

  // Fill in default values
  t->slof = (11700 * 1000UL);
  t->lof1 = (9750 * 1000UL);
  t->lof2 = (10600 * 1000UL);
  t->mod = QAM_AUTO;
  t->sinv = INVERSION_AUTO;
  t->tmode = TRANSMISSION_MODE_AUTO;
  t->bandw = BANDWIDTH_8_MHZ;	// intentionally set to 8 MHz (for DVB-T)
  t->gival = GUARD_INTERVAL_AUTO;
  t->hpcr = FEC_AUTO;
  t->lpcr = FEC_NONE;
  t->hier = HIERARCHY_AUTO;
  return t;
}


void
dvb_tune_exit (tunestruct * tune)
{
  if (tune != NULL)
    g_free (tune);
}


dvbstatstruct *
dvb_status_init (void)
{
  return g_malloc0 (sizeof (dvbstatstruct));
}


void
dvb_status_exit (dvbstatstruct * dvbstat)
{
  if (dvbstat != NULL)
    g_free (dvbstat);
}
