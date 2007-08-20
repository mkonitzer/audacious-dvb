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
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <linux/dvb/frontend.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>

#include "dvb.h"
#include "log.h"

extern gpointer hlog;


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
} HDVB;


struct diseqc_cmd
{
  struct dvb_diseqc_master_cmd cmd;
  guint wait;
};


gpointer *
dvb_open (gint devnum)
{
  HDVB *h;

  h = g_malloc0 (sizeof (HDVB));
  if (h == NULL)
    return NULL;

  h->dvb_num = devnum;
  h->dvb_fedn = g_strdup_printf ("/dev/dvb/adapter%d/frontend0", h->dvb_num);
  if ((h->dvb_fedh = open (h->dvb_fedn, O_RDWR)) < 0)
    {
      g_free (h);
      return NULL;
    }

  return (gpointer *) h;
}


gint
dvb_close (gpointer hdvb)
{
  HDVB *h;

  h = (HDVB *) hdvb;
  ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);

  if (h->dvb_admx > 0)
    close (h->dvb_admx);
  if (h->dvb_ddmx > 0)
    close (h->dvb_ddmx);
  if (h->dvb_fedh > 0)
    close (h->dvb_fedh);
  g_free (h);

  return RC_OK;
}


gint
dvb_filter (gpointer hdvb, gint pid)
{
  HDVB *h;
  h = (HDVB *) hdvb;
  
  g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_dmxdh = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_filter(), errno = %d (%s)",
		 errno, g_strerror(errno));
      return RC_DVB_FILTER_OPEN_DEMUX;
    }

  if (ioctl (h->dvb_dmxdh, DMX_SET_BUFFER_SIZE, 256 * 1024) < 0)
    {
      close (h->dvb_dmxdh);
      return RC_DVB_FILTER_SET_BUFFER_SIZE;
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
		 errno, g_strerror(errno));
      close (h->dvb_dmxdh);
      return RC_DVB_FILTER_SET_FAILED;
    }

  return RC_OK;
}


gint
dvb_packet (gpointer hdvb, guchar * pkt, gint t)
{
  int r, s;
  HDVB *h;
  fd_set rfd;
  struct timeval tv;

  h = (HDVB *) hdvb;

  tv.tv_sec = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO (&rfd);
  FD_SET (h->dvb_dmxdh, &rfd);

  do
    {
      s = select (h->dvb_dmxdh + 1, &rfd, NULL, NULL, &tv);
    }
  while (s < 0 && errno == EINTR);
  if (s <= 0)
    {
      if (s == 0)
	return RC_DVB_PACKET_SELECT_TIMEOUT;
      else
	return RC_DVB_PACKET_SELECT_FAILED;
    }

  if ((r = read (h->dvb_dmxdh, pkt, 184)) <= 0)
    return RC_DVB_PACKET_READ_FAILED;

  return RC_OK;
}


gint
dvb_unfilter (gpointer hdvb)
{
  HDVB *h;

  h = (HDVB *) hdvb;

  if (ioctl (h->dvb_dmxdh, DMX_STOP) < 0)
    return RC_OK;

  close (h->dvb_dmxdh);

  return RC_OK;
}


gint
dvb_section (gpointer hdvb, gint pid, gint sect, gint sid, gint sct,
	     guchar * s, gint t)
{
  gint sel, r, fd;
  HDVB *h;
  fd_set fds;
  struct timeval tv;
  struct dmx_sct_filter_params fp;

  h = (HDVB *) hdvb;
  g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((fd = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open failed in dvb_section(), errno = %d (%s)",
		 errno, g_strerror(errno));
      return RC_DVB_SECTION_OPEN_DEMUX;
    }

  fp.pid = pid;
  memset (&fp.filter.filter, 0x00, DMX_FILTER_SIZE);
  memset (&fp.filter.mask, 0x00, DMX_FILTER_SIZE);
  memset (&fp.filter.mode, 0x00, DMX_FILTER_SIZE);
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
      close (fd);
      return RC_DVB_SECTION_DMX_SET_FILTER;
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

  if (sel > 0)
    {
      if ((r = read (fd, s, 4096)) < 0)
	{
	  close (fd);
	  return RC_DVB_SECTION_READ_FAILED;
	}
      close (fd);
      return RC_OK;
    }
  else
    {
      close (fd);

      if (sel < 0)
	{
	  log_print (hlog, LOG_WARNING,
		     "select() failed in dvb_section(), errno = %d (%s)", errno, g_strerror(errno));
	  log_print (hlog, LOG_DEBUG, "%04x %04x %02x %d", pid, sect, sct, t);
	  return RC_DVB_SECTION_SELECT_FAILED;
	}
      else
	{
	  log_print (hlog, LOG_DEBUG, "select() timed out in dvb_section()");
	  log_print (hlog, LOG_DEBUG, "%04x %04x %02x %d", pid, sect, sct, t);
	  return RC_DVB_SECTION_SELECT_TIMEOUT;
	}
    }

  close (fd);

  return RC_OK;
}


gint
dvb_apid (gpointer hdvb, gint pid)
{
  HDVB *h;
  struct dmx_pes_filter_params fp;

  h = (HDVB *) hdvb;
  g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_admx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_apid(), errno = %d (%s)",
		 errno, g_strerror(errno));
      return RC_DVB_APID_OPEN_DEMUX;
    }

  if (ioctl (h->dvb_admx, DMX_SET_BUFFER_SIZE, 256 * 1024) < 0)
    {
      close (h->dvb_admx);
      return RC_DVB_APID_SET_BUFFER_SIZE;
    }

  fp.pid = pid;
  fp.input = DMX_IN_FRONTEND;
  fp.output = DMX_OUT_TAP;
  fp.pes_type = DMX_PES_AUDIO;
  fp.flags = DMX_IMMEDIATE_START;

  if (ioctl (h->dvb_admx, DMX_SET_PES_FILTER, &fp) < 0)
    return RC_DVB_APID_SETFILTER_FAILED;

  return RC_OK;
}


gint
dvb_apkt (gpointer hdvb, guchar * pkt, gint len, gint t, gint * rcvd)
{
  gint r, sel;
  HDVB *h;
  fd_set rfd;
  struct timeval tv;

  h = (HDVB *) hdvb;

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
    return RC_DVB_APKT_SELECT_FAILED;
  if (sel == 0)
    return RC_DVB_APKT_SELECT_TIMEOUT;

  r = read (h->dvb_admx, pkt, len);

  if (r <= 0)
    {
      log_print (hlog, LOG_ERR, "read() failed, errno = %d (%s)", errno, g_strerror(errno));
      return RC_DVB_APKT_READ_FAILED;
    }

  *rcvd = r;

  return RC_OK;
}


gint
dvb_dpid (gpointer hdvb, gint pid)
{
  HDVB *h;
  struct dmx_sct_filter_params fp;
  struct dmx_pes_filter_params pfp;

  h = (HDVB *) hdvb;
  g_free (h->dvb_dmxdn);
  h->dvb_dmxdn = g_strdup_printf ("/dev/dvb/adapter%d/demux0", h->dvb_num);

  if ((h->dvb_ddmx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_dpid(), errno = %d (%s)",
		 errno, g_strerror(errno));
      return RC_DVB_DPID_OPEN_DEMUX;
    }

  if (ioctl (h->dvb_ddmx, DMX_SET_BUFFER_SIZE, 131072) < 0)
    {
      close (h->dvb_ddmx);
      return RC_DVB_DPID_SET_BUFFER_SIZE;
    }

  pfp.pid = pid;
  pfp.input = DMX_IN_FRONTEND;
  pfp.output = DMX_OUT_TAP;
  pfp.pes_type = DMX_PES_AUDIO1;
  pfp.flags = DMX_IMMEDIATE_START;

  if (ioctl (h->dvb_ddmx, DMX_SET_PES_FILTER, &pfp) < 0)
    return RC_DVB_DPID_SETFILTER_FAILED;

  fp.pid = pid;
  memset (&fp.filter.filter, 0x00, DMX_FILTER_SIZE);
  memset (&fp.filter.mask, 0x00, DMX_FILTER_SIZE);
  memset (&fp.filter.mode, 0x00, DMX_FILTER_SIZE);
  fp.timeout = 0;
  fp.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

  fp.filter.filter[0] = 0x80;
  fp.filter.mask[0] = 0xff;

  fp.filter.filter[1] = 0x00;
  fp.filter.mask[1] = 0xff;

  fp.filter.filter[2] = 0x02;
  fp.filter.mask[2] = 0xff;

  if (ioctl (h->dvb_ddmx, DMX_SET_FILTER, &fp) < 0)
    return RC_DVB_DPID_SETFILTER_FAILED;

  return RC_OK;
}


gint
dvb_dpkt (void *hdvb, guchar * s, gint len, gint t, gint * rcvd)
{
  gint r, sel;
  HDVB *h;
  fd_set fds;
  struct timeval tv;

  h = (HDVB *) hdvb;

  tv.tv_sec = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO (&fds);
  FD_SET (h->dvb_ddmx, &fds);

  do
    {
      sel = select (h->dvb_ddmx + 1, &fds, NULL, NULL, &tv);
    }
  while (sel < 0 && errno == EINTR);

  if (sel > 0)
    {
      if ((r = read (h->dvb_ddmx, s, len)) < 0)
	{
	  log_print (hlog, LOG_ERR, "Fuck, data read failed, errno = %d (%s)",
		     errno, g_strerror(errno));
	  return RC_DVB_DPKT_READ_FAILED;
	}

      *rcvd = r;
      return RC_OK;
    }

  if (sel == 0)
    return RC_DVB_DPKT_SELECT_TIMEOUT;

  return RC_DVB_DPKT_SELECT_FAILED;
}


gint
dvb_get_pid (gpointer hdvb, gint s, gint * apid, gint * dpid)
{
  gint rc, len, pmt, pil, es, es_type, es_audio, es_data;
  gint sid;
  guchar sct[4096], *p, *q;

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
	    return rc;
	}

      p += 4;
    }

  if (es_audio < 0)
    return RC_DVB_GET_PID_SID_NOT_IN_PAT;

  *apid = es_audio;
  *dpid = es_data;

  return RC_OK;
}


void
dvb_tune_defaults (tunestruct * t)
{
  if (t == NULL)
    return;

  memset (t, 0, sizeof (tunestruct));
  t->slof = (11700 * 1000UL);
  t->lof1 = (9750 * 1000UL);
  t->lof2 = (10600 * 1000UL);
  t->mod = QAM_AUTO;
  t->sinv = INVERSION_AUTO;
  t->tmode = TRANSMISSION_MODE_AUTO;
  t->bandw = BANDWIDTH_8_MHZ;		// intentionally set to 8 MHz (DVB-T)
  t->gival = GUARD_INTERVAL_AUTO;
  t->hpcr = FEC_AUTO;
  t->lpcr = FEC_NONE;
  t->hier = HIERARCHY_AUTO;
}


static int
diseqc_send_msg (gpointer hdvb, fe_sec_voltage_t v, struct diseqc_cmd *cmd,
		 fe_sec_tone_mode_t t, guchar sat_no)
{
  HDVB *h;
  h = (HDVB *) hdvb;

  if (ioctl (h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF) < 0)
    return -1;
  if (ioctl (h->dvb_fedh, FE_SET_VOLTAGE, v) < 0)
    return -1;
  
  usleep (15 * 1000);
  
  if (sat_no >= 1 && sat_no <= 4)	// 1.x compatible DiSEqC
    {
      if (ioctl (h->dvb_fedh, FE_DISEQC_SEND_MASTER_CMD, &cmd->cmd) < 0)
	return -1;
      usleep (cmd->wait * 1000);
    }
  else					// A or B simple DiSEqC
    {
      log_print (hlog, LOG_INFO, "Setting simple %c burst", sat_no);
      if (ioctl (h->dvb_fedh, FE_DISEQC_SEND_BURST,
	   (sat_no == 'B' ? SEC_MINI_B : SEC_MINI_A)) < 0)
	return -1;
    }
  
  usleep (15 * 1000);
  
  if (ioctl (h->dvb_fedh, FE_SET_TONE, t) < 0)
    return -1;

  return 0;
}


static int
do_diseqc (gpointer hdvb, guchar sat_no, gint polv, gint hi_lo)
{
  HDVB *h;
  h = (HDVB *) hdvb;

  struct diseqc_cmd cmd = { {{0xe0, 0x10, 0x38, 0xf0, 0x00, 0x00}, 4}, 0 };

  if (sat_no != 0)
    {
      guchar d = sat_no;

      // param: high nibble: reset bits, low nibble: set bits,
      // bits are: option, position, polarizaion, band
      cmd.cmd.msg[3] =
	0xf0 | (((sat_no * 4) & 0x0f) | (polv ? 0 : 2) | (hi_lo ? 1 : 0));

      return diseqc_send_msg (hdvb, polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18,
			      &cmd, hi_lo ? SEC_TONE_ON : SEC_TONE_OFF, d);
    }
  else
    {
      log_print (hlog, LOG_INFO, "Setting only tone %s and voltage %dV",
	       (hi_lo ? "ON" : "OFF"), (polv ? 13 : 18));

      if (ioctl (h->dvb_fedh, FE_SET_VOLTAGE, (polv ? SEC_VOLTAGE_13 : SEC_VOLTAGE_18)) < 0)
	return -1;

      if (ioctl (h->dvb_fedh, FE_SET_TONE, (hi_lo ? SEC_TONE_ON : SEC_TONE_OFF)) < 0)
	return -1;

      usleep (15 * 1000);
    }
  
  return 0;
}


int
check_status (gpointer hdvb, int type,
	      struct dvb_frontend_parameters *feparams, unsigned int base)
{
  guint strength;
  fe_status_t festatus;
  struct pollfd pfd[1];
  gint status, locks = 0, ok = 0;
  time_t tm1, tm2;

  HDVB *h;
  h = (HDVB *) hdvb;

  if (ioctl (h->dvb_fedh, FE_SET_FRONTEND, feparams) < 0)
    {
      log_print (hlog, LOG_ERR, "FE_SET_FRONTEND failed in check_status(),"
		 " errno = %d (%s)", errno, g_strerror(errno));
      return -1;
    }

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
      
      usleep (10000);
      
      if ((festatus & FE_TIMEDOUT) || (locks >= 2) || (time (NULL) - tm1 >= 3))
	ok = 1;
    }

  if (festatus & FE_HAS_LOCK)
    {
      if (ioctl (h->dvb_fedh, FE_GET_FRONTEND, feparams) >= 0)
	{
	  switch (type)
	    {
	    case FE_OFDM:
	      log_print(hlog, LOG_INFO, "Event:  Frequency: %d",
		       feparams->frequency);
	      break;
	    case FE_QPSK:
	      log_print(hlog, LOG_INFO, "Event:  Frequency: %d",
		       (unsigned int) (feparams->frequency + base));
	      log_print(hlog, LOG_INFO, "        SymbolRate: %d",
		       feparams->u.qpsk.symbol_rate);
	      log_print(hlog, LOG_INFO, "        FEC_inner:  %d",
		       feparams->u.qpsk.fec_inner);
	      break;
	    case FE_QAM:
	      log_print(hlog, LOG_INFO, "Event:  Frequency: %d",
		       feparams->frequency);
	      log_print(hlog, LOG_INFO, "        SymbolRate: %d",
		       feparams->u.qpsk.symbol_rate);
	      log_print(hlog, LOG_INFO, "        FEC_inner:  %d",
		       feparams->u.qpsk.fec_inner);
	      break;
#ifdef DVB_ATSC
	    case FE_ATSC:
	      log_print(hlog, LOG_INFO, "Event:  Frequency: %d",
		       feparams->frequency);
	      log_print(hlog, LOG_INFO, "        Modulation: %d",
		       feparams->u.vsb.modulation);
	      break;
#endif
	    }
	}

      if (ioctl (h->dvb_fedh, FE_READ_BER, &strength) >= 0)
	log_print(hlog, LOG_DEBUG, "Bit error rate: %d", strength);
      if (ioctl (h->dvb_fedh, FE_READ_SIGNAL_STRENGTH, &strength) >= 0)
	log_print(hlog, LOG_DEBUG, "Signal strength: %d", strength);
      if (ioctl (h->dvb_fedh, FE_READ_SNR, &strength) >= 0)
	log_print(hlog, LOG_DEBUG, "SNR: %d", strength);
      if (ioctl (h->dvb_fedh, FE_READ_UNCORRECTED_BLOCKS, &strength) >= 0)
	log_print(hlog, LOG_DEBUG, "UNC: %d", strength);

      log_print(hlog, LOG_DEBUG, "FE_STATUS:%s%s%s%s%s%s",
		(festatus & FE_HAS_SIGNAL ? " FE_HAS_SIGNAL" : ""),
		(festatus & FE_TIMEDOUT ? " FE_TIMEDOUT" : ""),
		(festatus & FE_HAS_LOCK ? " FE_HAS_LOCK" : ""),
		(festatus & FE_HAS_CARRIER ? " FE_HAS_CARRIER" : ""),
		(festatus & FE_HAS_VITERBI ? " FE_HAS_VITERBI" : ""),
		(festatus & FE_HAS_SYNC ? " FE_HAS_SYNC" : ""));
    }
  else
    {
      log_print(hlog, LOG_ERR,
		"Not able to lock to the signal on the given frequency!");
      return -1;
    }
  return 0;
}


gint
dvb_tune (gpointer hdvb, tunestruct * t)
{
  gint res, hi_lo, dfd;
  guint base;
  struct dvb_frontend_parameters feparams;
  struct dvb_frontend_info fe_info;

  HDVB *h;
  h = (HDVB *) hdvb;

  if ((res = ioctl (h->dvb_fedh, FE_GET_INFO, &fe_info) < 0))
    {
      log_print (hlog, LOG_ERR, "FE_GET_INFO failed in dvb_tune(),"
		 " errno = %d (%s)", errno, g_strerror(errno));
      return -1;
    }

  log_print (hlog, LOG_INFO, "Using DVB card '%s', freq=%d", fe_info.name,
	     t->freq);

  if (t->freq < 1000000)
    t->freq *= 1000UL;
  
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
      log_print (hlog, LOG_INFO, "tuning DVB-T to %d Hz, Bandwidth: %d",
	       t->freq, t->bandw == BANDWIDTH_8_MHZ ? 8 :
	       (t->bandw == BANDWIDTH_7_MHZ ? 7 : 6));
      break;
    case FE_QPSK:
      if (t->freq > 2200000)
	{
	  if (t->freq < t->slof)
	    {
	      feparams.frequency = (t->freq - t->lof1);
	      hi_lo = 0;
	      base = t->lof1;
	    }
	  else
	    {
	      feparams.frequency = (t->freq - t->lof2);
	      hi_lo = 1;
	      base = t->lof2;
	    }
	}
      else
	{
	  feparams.frequency = t->freq;
	  base = 0;
	}

      log_print (hlog, LOG_INFO, 
		 "tuning DVB-S to Freq: %u, Pol:%c Srate=%d, 22kHz tone=%s, "
		 "LNB: %d, SLOF %d, LOF1: %d, LOF2: %d",
		 feparams.frequency, t->pol, t->srate,
		 hi_lo == 1 ? "ON" : "OFF", t->diseqc, t->slof / 1000UL,
		 t->lof1 / 1000UL, t->lof2 / 1000UL);
      feparams.inversion = t->sinv;
      feparams.u.qpsk.symbol_rate = t->srate;
      feparams.u.qpsk.fec_inner = FEC_AUTO;

      if (do_diseqc (hdvb, t->diseqc, (t->pol == 'V' ? 1 : 0), hi_lo) == 0)
	log_print (hlog, LOG_INFO, "DiSEqC setting succeeded");
      else
	{
	  log_print (hlog, LOG_ERR, "DiSEqC setting failed");
	  return -1;
	}
      break;
    case FE_QAM:
      log_print (hlog, LOG_INFO, "tuning DVB-C to %d, srate=%d",
		 t->freq, t->srate);
      feparams.frequency = t->freq;
      feparams.inversion = INVERSION_OFF;
      feparams.u.qam.symbol_rate = t->srate;
      feparams.u.qam.fec_inner = FEC_AUTO;
      feparams.u.qam.modulation = t->mod;
      break;
#ifdef DVB_ATSC
    case FE_ATSC:
      log_print (hlog, LOG_INFO, "tuning ATSC to %d, modulation=%d", t->freq,
	       t->mod);
      feparams.frequency = t->freq;
      feparams.u.vsb.modulation = t->mod;
      break;
#endif
    default:
      log_print (hlog, LOG_ERR, "Unknown FE type. Aborting");
      return -1;
    }
  usleep (100000);

  return (check_status (hdvb, fe_info.type, &feparams, base));
}
