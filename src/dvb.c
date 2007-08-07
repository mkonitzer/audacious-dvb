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

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>

#include "dvb.h"
#include "log.h"

extern void *hlog;


typedef struct _HDVB
{
  int dvb_num;
  char dvb_fedn[MAXPATHLEN];
  int dvb_fedh;
  int dvb_excl;
  char dvb_dmxdn[MAXPATHLEN];
  int dvb_dmxdh;
  char dvb_audn[MAXPATHLEN];
  int dvb_audh;
  int dvb_admx;
  int dvb_ddmx;
  int dvb_pid;
  struct dmx_pes_filter_params dvb_dmx;
} HDVB;


int
dvb_open (int dev, void **hdvb)
{
  HDVB *h;

  if ((h = malloc (sizeof (HDVB))) == NULL)
    return RC_DVB_OPEN_MALLOC_FAILED;

  memset (h, 0x00, sizeof (HDVB));

  h->dvb_num = dev;
  sprintf (h->dvb_fedn, "/dev/dvb/adapter%d/frontend0", h->dvb_num);
  h->dvb_excl = 1;

  if ((h->dvb_fedh = open (h->dvb_fedn, O_RDWR)) < 0)
    {
      if (errno == EBUSY)
	{
	  h->dvb_excl = 0;
	}
      else
	{
	  free (h);
	  *hdvb = NULL;
	  return RC_DVB_OPEN_FRONTEND_FAILED;
	}
    }
  *hdvb = h;

  return RC_OK;
}


int
dvb_close (void *hdvb)
{
  HDVB *h;

  h = (HDVB *) hdvb;

  if (h->dvb_excl)
    ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);

  if (h->dvb_admx > 0)
    close (h->dvb_admx);

  if (h->dvb_ddmx > 0)
    close (h->dvb_ddmx);

  if (h->dvb_fedh > 0)
    close (h->dvb_fedh);

  free (h);

  return RC_OK;
}


int
dvb_tune_qpsk (void *hdvb, int lnb, int qrg, char pol, int sr, int npow)
{
  int zf, tone, volt, err;
  HDVB *h;
  struct dvb_diseqc_master_cmd cmd;
  struct dvb_frontend_parameters fep;

  h = (HDVB *) hdvb;

  if (qrg > 10000000)
    {
      if (qrg < 11600000)
	{
	  tone = 0;
	  zf = qrg - 9750000;
	}
      else
	{
	  tone = 1;
	  zf = qrg - 10600000;
	}
    }
  else
    {
      zf = qrg;
      tone = 0;
    }

  if (!h->dvb_excl)
    return RC_OK;

  if (ioctl (h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF) < 0)
    return RC_DVB_TUNE_QPSK_TONE_RESET;

  if (npow)
    {
      if (toupper (pol) == 'H')
	volt = 2;
      else
	volt = 0;

      err = ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_13);
    }
  else
    {
      if (toupper (pol) == 'H')
	{
	  volt = 2;
	  err = ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
	}
      else
	{
	  volt = 0;
	  err = ioctl (h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_13);
	}
    }

  if (err < 0)
    return RC_DVB_TUNE_QPSK_PWR_FAILED;

  if (npow == 0)
    {
      cmd.msg[0] = 0xe0;
      cmd.msg[1] = 0x10;
      cmd.msg[2] = 0x38;
      cmd.msg[3] = 0xf0 | ((lnb << 2) & 0x0f) | tone | volt;
      cmd.msg_len = 4;

      usleep (15000);

      if (ioctl (h->dvb_fedh, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0)
	return RC_DVB_TUNE_QPSK_DISEQC_FAILED;

      usleep (15000);

      if (tone)
	err = ioctl (h->dvb_fedh, FE_SET_TONE, SEC_TONE_ON);
      else
	err = ioctl (h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF);

      if (err < 0)
	return RC_DVB_TUNE_QPSK_TONE_SET;
    }

  fep.frequency = zf;
  fep.inversion = INVERSION_AUTO;
  fep.u.qpsk.fec_inner = FEC_AUTO;
  fep.u.qpsk.symbol_rate = sr;

  if (ioctl (h->dvb_fedh, FE_SET_FRONTEND, &fep) < 0)
    return RC_DVB_TUNE_QPSK_FE_SET_FAILED;

  return RC_OK;
}


int
dvb_status (void *hdvb)
{
  int qual;
  HDVB *h;
  long sstr, snr;
  fe_status_t fest;
  unsigned long ber;

  h = (HDVB *) hdvb;

  sstr = snr = ber = 0;

  if (ioctl (h->dvb_fedh, FE_READ_STATUS, &fest) < 0)
    return -1;

  if (ioctl (h->dvb_fedh, FE_READ_BER, &ber) < 0)
    return -1;

  if (ioctl (h->dvb_fedh, FE_READ_SIGNAL_STRENGTH, &sstr) < 0)
    return -1;

  if (ioctl (h->dvb_fedh, FE_READ_SNR, &snr) < 0)
    return -1;

  qual = 2097152 - ber;
  if (qual < 0)
    qual = 0;

  qual /= 20971;

  if (fest & FE_HAS_SIGNAL)
    printf ("S");
  else
    printf (" ");

  if (fest & FE_HAS_CARRIER)
    printf ("C");
  else
    printf (" ");

  if (fest & FE_HAS_VITERBI)
    printf ("V");
  else
    printf (" ");

  if (fest & FE_HAS_SYNC)
    printf ("Y");
  else
    printf (" ");

  if (fest & FE_HAS_LOCK)
    printf ("L");
  else
    printf (" ");

  return RC_OK;
}


int
dvb_filter (void *hdvb, int pid)
{
  HDVB *h;

  h = (HDVB *) hdvb;

  sprintf (h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_dmxdh = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_filter(), errno = %d\n",
		 errno);
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
		 "DMX_SET_PES_FILTER failed in dvb_filter(), errno = %d\n",
		 errno);
      close (h->dvb_dmxdh);
      return RC_DVB_FILTER_SET_FAILED;
    }

  return RC_OK;
}


int
dvb_packet (void *hdvb, unsigned char *pkt, int t)
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


int
dvb_unfilter (void *hdvb)
{
  HDVB *h;

  h = (HDVB *) hdvb;

  if (ioctl (h->dvb_dmxdh, DMX_STOP) < 0)
    return RC_OK;

  close (h->dvb_dmxdh);

  return RC_OK;
}


int
dvb_section (void *hdvb, int pid, int sect, int sid, int sct,
	     unsigned char *s, int t)
{
  int sel, r, fd;
  HDVB *h;
  fd_set fds;
  struct timeval tv;
  struct dmx_sct_filter_params fp;

  h = (HDVB *) hdvb;

  sprintf (h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((fd = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open failed in dvb_section(), errno = %d\n",
		 errno);
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
		     "select() failed in dvb_section(), errno = %d", errno);
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


int
dvb_apid (void *hdvb, int pid)
{
  HDVB *h;
  struct dmx_pes_filter_params fp;

  h = (HDVB *) hdvb;

  sprintf (h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_admx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_apid(), errno = %d\n",
		 errno);
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


int
dvb_apkt (void *hdvb, unsigned char *pkt, int len, int t, int *rcvd)
{
  int r, sel;
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
  if (sel <= 0)
    {
      if (sel < 0)
	return RC_DVB_APKT_SELECT_FAILED;
      return RC_DVB_APKT_SELECT_TIMEOUT;
    }

  r = read (h->dvb_admx, pkt, len);

  if (r <= 0)
    {
      log_print (hlog, LOG_ERR, "read() failed, errno = %d.\n", errno);
      return RC_DVB_APKT_READ_FAILED;
    }

  *rcvd = r;

  return RC_OK;
}


int
dvb_dpid (void *hdvb, int pid)
{
  HDVB *h;
  struct dmx_sct_filter_params fp;
  struct dmx_pes_filter_params pfp;

  h = (HDVB *) hdvb;

  sprintf (h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_ddmx = open (h->dvb_dmxdn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_dpid(), errno = %d\n",
		 errno);
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


int
dvb_dpkt (void *hdvb, unsigned char *s, int len, int t, int *rcvd)
{
  int r, sel;
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
	  log_print (hlog, LOG_ERR, "Fuck, data read failed, errno = %d\n",
		     errno);
	  return RC_DVB_DPKT_READ_FAILED;
	}

      *rcvd = r;
      return RC_OK;
    }

  if (sel == 0)
    return RC_DVB_DPKT_SELECT_TIMEOUT;

  return RC_DVB_DPKT_SELECT_FAILED;
}


int
dvb_volume (void *hdvb, int vol)
{
  HDVB *h;
  audio_mixer_t mix;

  h = (HDVB *) hdvb;

  if (h == NULL)
    return RC_DVB_VOLUME_INVALID_HANDLE;

  sprintf (h->dvb_audn, "/dev/dvb/adapter%d/audio0", h->dvb_num);
  if ((h->dvb_audh = open (h->dvb_audn, O_RDWR)) < 0)
    {
      log_print (hlog, LOG_ERR, "open() failed in dvb_volume(), errno = %d\n",
		 errno);
      return RC_DVB_VOLUME_OPEN_AUDIO;
    }

  mix.volume_left = vol;
  mix.volume_right = vol;

  if (ioctl (h->dvb_audh, AUDIO_SET_MIXER, &mix) < 0)
    {
      log_print (hlog, LOG_ERR,
		 "ioctl() failed in dvb_volume(), errno = %d\n", errno);
      return RC_DVB_VOLUME_SET_MIXER;
    }

  close (h->dvb_audh);
  h->dvb_audh = -1;

  return RC_OK;
}

int
dvb_get_pid (void *hdvb, int s, int *apid, int *dpid)
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
    return RC_DVB_GET_PID_SID_NOT_IN_PAT;

  *apid = es_audio;
  *dpid = es_data;

  return RC_OK;
}
