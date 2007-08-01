/*******************************************************************************
**
** Filename:      dvb.c
**
** Function List: dvb_open()
**                dvb_close()
**                dvb_tune_qpsk()
**                dvb_status()
**                dvb_filter()
**                dvb_packet()
**                dvb_unfilter()
**                dvb_section()
**                dvb_apid()
**                dvb_apkt()
**                dvb_dpid()
**                dvb_dpkt()
**                dvb_volume()
**
** Function:      This module contains code that interfaces to a DVB
**                PCI adapter using the driver available from
**
**                              http://www.linuxtv.org
**
**                It provides a bit of abstraction so there is no need
**                to twiddle around with the device itself. The layer
**                this code presents is far from perfect, but it seems
**                to do the job in most cases.
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
** 030602 -- CMO: Module created.
**
** 030622 -- CMO: Added support for logging.
**
** 030701 -- CMO: Moved in code to use the adapter's audio path so
**                that scrambled channels can be handled as well.
**
** 030704 -- CMO: Added an interface function to set the audio decoder
**                volume. This is used for muting the audio path, as
**                XMMS does its own output of raw PCM data.
**
** 040107 -- CMO: Fixed copyright statement to reflect the GPL status
**                of the code.
**
*******************************************************************************/

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/param.h>

#include <linux/dvb/frontend.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/dmx.h>

#include "dvb.h"
#include "log.h"

extern void *hlog;


typedef struct _HDVB {
  int                           dvb_num;
  char                          dvb_fedn[MAXPATHLEN];
  int                           dvb_fedh;
  int                           dvb_excl;
  char                          dvb_dmxdn[MAXPATHLEN];
  int                           dvb_dmxdh;
  char                          dvb_audn[MAXPATHLEN];
  int                           dvb_audh;
  int                           dvb_admx;
  int                           dvb_ddmx;
  int                           dvb_pid;
  struct dmx_pes_filter_params  dvb_dmx;
} HDVB;


/*******************************************************************************
** Function: dvb_open()
**
** Description: 
**
*******************************************************************************/

int dvb_open(int dev, void **hdvb)
{
  HDVB                      *h;
//  struct dvb_frontend_info  dfi;

  if ((h = malloc(sizeof(HDVB))) == NULL) {
    return (RC_DVB_OPEN_MALLOC_FAILED);
  }

  memset(h, 0x00, sizeof(HDVB));

  h->dvb_num = dev;
  sprintf(h->dvb_fedn, "/dev/dvb/adapter%d/frontend0", h->dvb_num);
  h->dvb_excl = 1;

  if ((h->dvb_fedh = open(h->dvb_fedn, O_RDWR)) < 0) {
    if (errno == EBUSY) {
      h->dvb_excl = 0;
    } else {
      free(h);
      *hdvb = NULL;
      return (RC_DVB_OPEN_FRONTEND_FAILED);
    }
  }
/*
  if (ioctl(h->dvb_fedh, FE_GET_INFO, &dfi) >= 0) {
    printf("%s ", dfi.name);
    switch (dfi.type) {
      case FE_QAM:
        printf("(QAM)");
        break;

      case FE_OFDM:
        printf("(OFDM)");
        break;

      case FE_QPSK:
        printf("(QPSK)");
        break;

      default:
        printf("(Unknown)");
        break;
    }

    printf(" %d-%d MHz, %d (%d) ", dfi.frequency_min / 1000,
           dfi.frequency_max / 1000,
           dfi.frequency_stepsize, dfi.frequency_tolerance);

    printf(" %d-%d (%d)\n", dfi.symbol_rate_min / 1000,
           dfi.symbol_rate_max / 1000, dfi.symbol_rate_tolerance);
  }
*/
  *hdvb = h;

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_close()
**
** Description: 
**
*******************************************************************************/

int dvb_close(void *hdvb)
{
  HDVB  *h;

  h = (HDVB *)hdvb;

  if (h->dvb_excl) {
    ioctl(h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
  }

  if (h->dvb_admx > 0) {
    close(h->dvb_admx);
  }

  if (h->dvb_ddmx > 0) {
    close(h->dvb_ddmx);
  }

  if (h->dvb_fedh > 0) {
    close(h->dvb_fedh);
  }

  free(h);

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_tune_qpsk()
**
** Description: 
**
*******************************************************************************/

int dvb_tune_qpsk(void *hdvb, int lnb, int qrg, char pol, int sr, int npow)
{
  int                             zf, tone, volt, err; // , i;
  HDVB                            *h;
//  fe_status_t                     fest;
  struct dvb_diseqc_master_cmd    cmd;
  struct dvb_frontend_parameters  fep;

  h = (HDVB *)hdvb;

  if (qrg > 10000000) {
    if (qrg < 11600000) {
      tone = 0;
      zf = qrg - 9750000;
    } else {
      tone = 1;
      zf = qrg - 10600000;
    }
  } else {
    zf = qrg;
    tone = 0;
  }

  if (!h->dvb_excl) {
    return (RC_OK);
  }

  if (ioctl(h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF) < 0) {
    return (RC_DVB_TUNE_QPSK_TONE_RESET);
  }

  if (npow) {
    if (toupper(pol) == 'H') {
      volt = 2;
    } else {
      volt = 0;
    }

//    err = ioctl(h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
    err = ioctl(h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_13);
  } else {
    if (toupper(pol) == 'H') {
      volt = 2;
      err = ioctl(h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_18);
    } else {
      volt = 0;
      err = ioctl(h->dvb_fedh, FE_SET_VOLTAGE, SEC_VOLTAGE_13);
    }
  }

  if (err < 0) {
    return (RC_DVB_TUNE_QPSK_PWR_FAILED);
  }

  if (npow == 0) {
    cmd.msg[0] = 0xe0;
    cmd.msg[1] = 0x10;
    cmd.msg[2] = 0x38;
    cmd.msg[3] = 0xf0 | ((lnb << 2) & 0x0f) | tone | volt;
    cmd.msg_len = 4;

    usleep(15000);

    if (ioctl(h->dvb_fedh, FE_DISEQC_SEND_MASTER_CMD, &cmd) < 0) {
      return (RC_DVB_TUNE_QPSK_DISEQC_FAILED);
    }

    usleep(15000);

    if (tone) {
      err = ioctl(h->dvb_fedh, FE_SET_TONE, SEC_TONE_ON);
    } else {
      err = ioctl(h->dvb_fedh, FE_SET_TONE, SEC_TONE_OFF);
    }

    if (err < 0) {
      return (RC_DVB_TUNE_QPSK_TONE_SET);
    }
  }

//  printf("%d\n", zf);

  fep.frequency           = zf;
  fep.inversion           = INVERSION_AUTO;
  fep.u.qpsk.fec_inner    = FEC_AUTO;
  fep.u.qpsk.symbol_rate  = sr;

  if (ioctl(h->dvb_fedh, FE_SET_FRONTEND, &fep) < 0) {
    return (RC_DVB_TUNE_QPSK_FE_SET_FAILED);
  }

/*  for (i = 0; i < 150; i++) {
    usleep(100000);

    if (ioctl(h->dvb_fedh, FE_READ_STATUS, &fest) < 0) {
      return (-1);
    }

    if (fest & FE_HAS_LOCK) {
      return (RC_OK);
    }
  }

  return (RC_DVB_TUNE_QPSK_LOCK_TIMEOUT); */

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_status()
**
** Description: 
**
*******************************************************************************/

int dvb_status(void *hdvb)
{
  int           qual;
  HDVB          *h;
  long          sstr, snr;
  fe_status_t   fest;
  unsigned long ber;

  h = (HDVB *)hdvb;

  sstr = snr = ber = 0;

  if (ioctl(h->dvb_fedh, FE_READ_STATUS, &fest) < 0) {
    return (-1);
  }

  if (ioctl(h->dvb_fedh, FE_READ_BER, &ber) < 0) {
    return (-1);
  }

  if (ioctl(h->dvb_fedh, FE_READ_SIGNAL_STRENGTH, &sstr) < 0) {
    return (-1);
  }

  if (ioctl(h->dvb_fedh, FE_READ_SNR, &snr) < 0) {
    return (-1);
  }

  qual = 2097152 - ber;
  if (qual < 0) {
    qual = 0;
  }

  qual /= 20971;

  if (fest & FE_HAS_SIGNAL) {
    printf("S");
  } else {
    printf(" ");
  }

  if (fest & FE_HAS_CARRIER) {
    printf("C");
  } else {
    printf(" ");
  }

  if (fest & FE_HAS_VITERBI) {
    printf("V");
  } else {
    printf(" ");
  }

  if (fest & FE_HAS_SYNC) {
    printf("Y");
  } else {
    printf(" ");
  }

  if (fest & FE_HAS_LOCK) {
    printf("L");
  } else {
    printf(" ");
  }

/*  printf(" %d %d (%d) %d (%d%%/%d%%)\n", sstr, snr, (snr -sstr), ber / 10, qual,
         (1048576 - (ber / 10)) / 10485); */
  
  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_filter()
**
** Description: 
**
*******************************************************************************/

int dvb_filter(void *hdvb, int pid)
{
  HDVB  *h;

  h = (HDVB *)hdvb;

  sprintf(h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_dmxdh = open(h->dvb_dmxdn, O_RDWR)) < 0) {
    log_print(hlog, LOG_ERR, "open() failed in dvb_filter(), errno = %d\n", errno);
    return (RC_DVB_FILTER_OPEN_DEMUX);
  }

  if (ioctl(h->dvb_dmxdh, DMX_SET_BUFFER_SIZE, 256 * 1024) < 0) {
    close(h->dvb_dmxdh);
    return (RC_DVB_FILTER_SET_BUFFER_SIZE);
  }

  h->dvb_dmx.pid      = pid;
  h->dvb_dmx.input    = DMX_IN_FRONTEND;
  h->dvb_dmx.output   = DMX_OUT_TAP;
  h->dvb_dmx.pes_type = DMX_PES_OTHER;
  h->dvb_dmx.flags    = DMX_IMMEDIATE_START;

  if (ioctl(h->dvb_dmxdh, DMX_SET_PES_FILTER, &h->dvb_dmx) < 0) {
    log_print(hlog, LOG_ERR, "DMX_SET_PES_FILTER failed in dvb_filter(), errno = %d\n", errno);
    close(h->dvb_dmxdh);
    return (RC_DVB_FILTER_SET_FAILED);
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_packet()
**
** Description: 
**
*******************************************************************************/

int dvb_packet(void *hdvb, unsigned char *pkt, int t)
{
  int             r, s;
  HDVB            *h;
  fd_set          rfd;
  struct timeval  tv;

  h = (HDVB *)hdvb;

  tv.tv_sec  = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO(&rfd);
  FD_SET(h->dvb_dmxdh, &rfd);

  if ((s = select(h->dvb_dmxdh + 1, &rfd, NULL, NULL, &tv)) <= 0) {
    if (s == 0) {
      return (RC_DVB_PACKET_SELECT_TIMEOUT);
    } else {
      return (RC_DVB_PACKET_SELECT_FAILED);
    }
  }

  if ((r = read(h->dvb_dmxdh, pkt, 184)) <= 0) {
    return (RC_DVB_PACKET_READ_FAILED);
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_unfilter()
**
** Description: 
**
*******************************************************************************/

int dvb_unfilter(void *hdvb)
{
  HDVB  *h;

  h = (HDVB *)hdvb;

  if (ioctl(h->dvb_dmxdh, DMX_STOP) < 0) {
    return (RC_OK);
  }

  close(h->dvb_dmxdh);

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_section()
**
** Description: 
**
*******************************************************************************/

int dvb_section(void *hdvb, int pid, int sect, int sid, int sct, unsigned char *s, int t)
{
  int                           sel, r, fd;
  HDVB                          *h;
  fd_set                        fds;
  struct timeval                tv;
  struct dmx_sct_filter_params  fp;

  h = (HDVB *)hdvb;

  sprintf(h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((fd = open(h->dvb_dmxdn, O_RDWR)) < 0) {
    log_print(hlog, LOG_ERR, "open failed in dvb_section(), errno = %d\n", errno);
    return (RC_DVB_SECTION_OPEN_DEMUX);
  }

  fp.pid = pid;
  memset(&fp.filter.filter, 0x00, DMX_FILTER_SIZE);
  memset(&fp.filter.mask, 0x00, DMX_FILTER_SIZE);
  memset(&fp.filter.mode, 0x00, DMX_FILTER_SIZE);
  fp.timeout = 0;
  fp.flags = DMX_IMMEDIATE_START | DMX_ONESHOT | DMX_CHECK_CRC;

  if (sect != -1) {
    fp.filter.filter[0] = sect & 0xff;
    fp.filter.mask[0]   = 0xff;

    if (sect == 2) {
      fp.filter.filter[1] = (sid >> 8) & 0xff;
      fp.filter.filter[2] = sid & 0xff;
      fp.filter.mask[1]   = 0xff;
      fp.filter.mask[2]   = 0xff;
    }

    if (sect == 0x42) {
      fp.filter.filter[4] = sct & 0xff;
      fp.filter.mask[4]   = 0xff;
    }

    if (sect == 0x4e) {
      fp.filter.filter[1] = (sid >> 8) & 0xff;
      fp.filter.filter[2] = sid & 0xff;
      fp.filter.mask[1]   = 0xff;
      fp.filter.mask[2]   = 0xff;
      fp.filter.filter[4] = sct & 0xff;
      fp.filter.mask[4]   = 0xff;
    }
  }

  if (ioctl(fd, DMX_SET_FILTER, &fp) < 0) {
    close(fd);
    return (RC_DVB_SECTION_DMX_SET_FILTER);
  }

  tv.tv_sec  = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  sel = select(fd + 1, &fds, NULL, NULL, &tv);

  if (sel > 0) {
    if ((r = read(fd, s, 4096)) < 0) {
      close(fd);
      return (RC_DVB_SECTION_READ_FAILED);
    }

    close(fd);
    return (RC_OK);
  } else {
    close(fd);

    if (sel < 0) {
      log_print(hlog, LOG_WARNING,
                "select() failed in dvb_section(), errno = %d", errno);
      log_print(hlog, LOG_DEBUG, "%04x %04x %02x %d", pid, sect, sct, t);
      return (RC_DVB_SECTION_SELECT_FAILED);
    } else {
      log_print(hlog, LOG_DEBUG, "select() timed out in dvb_section()");
      log_print(hlog, LOG_DEBUG, "%04x %04x %02x %d", pid, sect, sct, t);
      return (RC_DVB_SECTION_SELECT_TIMEOUT);
    }
  }

  close(fd);

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_apid()
**
** Description: 
**
*******************************************************************************/

int dvb_apid(void *hdvb, int pid)
{
  HDVB                          *h;
  struct dmx_pes_filter_params  fp;

  h = (HDVB *)hdvb;

  sprintf(h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_admx = open(h->dvb_dmxdn, O_RDWR)) < 0) {
    log_print(hlog, LOG_ERR, "open() failed in dvb_apid(), errno = %d\n", errno);
    return (RC_DVB_APID_OPEN_DEMUX);
  }

  if (ioctl(h->dvb_admx, DMX_SET_BUFFER_SIZE, 256 * 1024) < 0) {
    close(h->dvb_admx);
    return (RC_DVB_APID_SET_BUFFER_SIZE);
  }

  fp.pid      = pid;
  fp.input    = DMX_IN_FRONTEND;
  fp.output   = DMX_OUT_TAP;
  fp.pes_type = DMX_PES_AUDIO;
  fp.flags    = DMX_IMMEDIATE_START;

  if (ioctl(h->dvb_admx, DMX_SET_PES_FILTER, &fp) < 0) {
    return (RC_DVB_APID_SETFILTER_FAILED);
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_apkt()
**
** Description: 
**
*******************************************************************************/

int dvb_apkt(void *hdvb, unsigned char *pkt, int len, int t, int *rcvd)
{
  int             r, sel;
  HDVB            *h;
  fd_set          rfd;
  struct timeval  tv;

  h = (HDVB *)hdvb;

  tv.tv_sec  = t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO(&rfd);
  FD_SET(h->dvb_admx, &rfd);

  if ((sel = select(h->dvb_admx + 1, &rfd, NULL, NULL, &tv)) <= 0) {
    if (sel < 0) {
      return (RC_DVB_APKT_SELECT_FAILED);
    }
    return (RC_DVB_APKT_SELECT_TIMEOUT);
  }

  r = read(h->dvb_admx, pkt, len);

  if (r <= 0) {
    log_print(hlog, LOG_ERR, "read() failed, errno = %d.\n", errno);
    return (RC_DVB_APKT_READ_FAILED);
  }

  *rcvd = r;

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_dpid()
**
** Description: 
**
*******************************************************************************/

int dvb_dpid(void *hdvb, int pid)
{
  HDVB                          *h;
  struct dmx_sct_filter_params  fp;
  struct dmx_pes_filter_params  pfp;

  h = (HDVB *)hdvb;

  sprintf(h->dvb_dmxdn, "/dev/dvb/adapter%d/demux0", h->dvb_num);
  if ((h->dvb_ddmx = open(h->dvb_dmxdn, O_RDWR)) < 0) {
    log_print(hlog, LOG_ERR, "open() failed in dvb_dpid(), errno = %d\n", errno);
    return (RC_DVB_DPID_OPEN_DEMUX);
  }

  if (ioctl(h->dvb_ddmx, DMX_SET_BUFFER_SIZE, 131072) < 0) {
    close(h->dvb_ddmx);
    return (RC_DVB_DPID_SET_BUFFER_SIZE);
  }

  pfp.pid      = pid;
  pfp.input    = DMX_IN_FRONTEND;
  pfp.output   = DMX_OUT_TAP;
  pfp.pes_type = DMX_PES_AUDIO1;
  pfp.flags    = DMX_IMMEDIATE_START;

  if (ioctl(h->dvb_ddmx, DMX_SET_PES_FILTER, &pfp) < 0) {
    return (RC_DVB_DPID_SETFILTER_FAILED);
  }

  fp.pid = pid;
  memset(&fp.filter.filter, 0x00, DMX_FILTER_SIZE);
  memset(&fp.filter.mask, 0x00, DMX_FILTER_SIZE);
  memset(&fp.filter.mode, 0x00, DMX_FILTER_SIZE);
  fp.timeout = 0;
  fp.flags = DMX_IMMEDIATE_START | DMX_CHECK_CRC;

  fp.filter.filter[0] = 0x80;
  fp.filter.mask[0]   = 0xff;

  fp.filter.filter[1] = 0x00;
  fp.filter.mask[1]   = 0xff;

  fp.filter.filter[2] = 0x02;
  fp.filter.mask[2]   = 0xff;

  if (ioctl(h->dvb_ddmx, DMX_SET_FILTER, &fp) < 0) {
    return (RC_DVB_DPID_SETFILTER_FAILED);
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_dpkt()
**
** Description: 
**
*******************************************************************************/

int dvb_dpkt(void *hdvb, unsigned char *s, int len, int t, int *rcvd)
{
  int                       r, sel;
  HDVB                      *h;
  fd_set                    fds;
  struct timeval            tv;

  h = (HDVB *)hdvb;

  tv.tv_sec =  t / 1000;
  tv.tv_usec = 1000 * (t % 1000);

  FD_ZERO(&fds);
  FD_SET(h->dvb_ddmx, &fds);

  sel = select(h->dvb_ddmx + 1, &fds, NULL, NULL, &tv);

  if (sel > 0) {
    if ((r = read(h->dvb_ddmx, s, len)) < 0) {
      log_print(hlog, LOG_ERR, "Fuck, data read failed, errno = %d\n", errno);
      return (RC_DVB_DPKT_READ_FAILED);
    }

    *rcvd = r;
    return (RC_OK);
  }

  if (sel == 0) {
    return (RC_DVB_DPKT_SELECT_TIMEOUT);
  }

//  fprintf(stderr, "Warning, unable to fetch %d from %d\n", sect, pid);

  return (RC_DVB_DPKT_SELECT_FAILED);
}


/*******************************************************************************
** Function: dvb_volume()
**
** Description: 
**
*******************************************************************************/

int dvb_volume(void *hdvb, int vol)
{
  HDVB          *h;
  audio_mixer_t mix;
  
  h = (HDVB *)hdvb;

  if (h == NULL) {
    return (RC_DVB_VOLUME_INVALID_HANDLE);
  }

  sprintf(h->dvb_audn, "/dev/dvb/adapter%d/audio0", h->dvb_num);
  if ((h->dvb_audh = open(h->dvb_audn, O_RDWR)) < 0) {
    log_print(hlog, LOG_ERR, "open() failed in dvb_volume(), errno = %d\n", errno);
    return (RC_DVB_VOLUME_OPEN_AUDIO);
  }

  mix.volume_left   = vol;
  mix.volume_right  = vol;

  if (ioctl(h->dvb_audh, AUDIO_SET_MIXER, &mix) < 0) {
    log_print(hlog, LOG_ERR, "ioctl() failed in dvb_volume(), errno = %d\n", errno);
    return (RC_DVB_VOLUME_SET_MIXER);
  }

  close(h->dvb_audh);
  h->dvb_audh = -1;

  return (RC_OK);
}