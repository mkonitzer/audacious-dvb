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

#ifndef RC_OK
#define RC_OK                           0
#endif

#define RC_DVB_OPEN_MALLOC_FAILED       2010
#define RC_DVB_OPEN_FRONTEND_FAILED     2011

#define RC_DVB_TUNE_QPSK_TONE_RESET     2020
#define RC_DVB_TUNE_QPSK_PWR_FAILED     2021
#define RC_DVB_TUNE_QPSK_DISEQC_FAILED  2022
#define RC_DVB_TUNE_QPSK_TONE_SET       2023
#define RC_DVB_TUNE_QPSK_FE_SET_FAILED  2024
#define RC_DVB_TUNE_QPSK_READ_STATUS    2025
#define RC_DVB_TUNE_QPSK_LOCK_TIMEOUT   2026

#define RC_DVB_FILTER_OPEN_DEMUX        2030
#define RC_DVB_FILTER_SET_BUFFER_SIZE   2031
#define RC_DVB_FILTER_SET_FAILED        2032

#define RC_DVB_PACKET_SELECT_TIMEOUT    2040
#define RC_DVB_PACKET_SELECT_FAILED     2041
#define RC_DVB_PACKET_READ_FAILED       2042

#define RC_DVB_SECTION_OPEN_DEMUX       2060
#define RC_DVB_SECTION_DMX_SET_FILTER   2061
#define RC_DVB_SECTION_READ_FAILED      2062
#define RC_DVB_SECTION_SELECT_FAILED    2063
#define RC_DVB_SECTION_SELECT_TIMEOUT   2064

#define RC_DVB_APID_OPEN_DEMUX          2070
#define RC_DVB_APID_SET_BUFFER_SIZE     2071
#define RC_DVB_APID_SETFILTER_FAILED    2072

#define RC_DVB_APKT_SELECT_FAILED       2080
#define RC_DVB_APKT_SELECT_TIMEOUT      2081
#define RC_DVB_APKT_READ_FAILED         2082

#define RC_DVB_DPID_OPEN_DEMUX          2090
#define RC_DVB_DPID_SET_BUFFER_SIZE     2091
#define RC_DVB_DPID_SETFILTER_FAILED    2092

#define RC_DVB_DPKT_SELECT_FAILED       2100
#define RC_DVB_DPKT_SELECT_TIMEOUT      2101
#define RC_DVB_DPKT_READ_FAILED         2102

#define RC_DVB_VOLUME_INVALID_HANDLE    2110
#define RC_DVB_VOLUME_OPEN_AUDIO        2111
#define RC_DVB_VOLUME_SET_MIXER         2112

#define RC_DVB_GET_PID_SID_NOT_IN_PAT   2120

int dvb_open (int, void **);
int dvb_close (void *);
int dvb_tune_qpsk (void *, int, int, char, int, int);
int dvb_status (void *);
int dvb_filter (void *, int);
int dvb_packet (void *, unsigned char *, int);
int dvb_unfilter (void *);
int dvb_section (void *, int, int, int, int, unsigned char *, int);
int dvb_apid (void *, int);
int dvb_apkt (void *, unsigned char *, int, int, int *);
int dvb_dpid (void *, int);
int dvb_dpkt (void *, unsigned char *, int, int, int *);
int dvb_volume (void *, int);
int dvb_get_pid (void *, int, int *, int *);

#endif // __AUDACIOUS_DVB_DVB_H__
