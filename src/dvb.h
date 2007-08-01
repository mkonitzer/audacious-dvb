/*******************************************************************************
**
** Filename:      dvb.h
**
** Function:      This include file contains the return code
**                definitions and prototype declarations for the
**                functions providing access to the DVB adapter.
**
** Copyright:     (C) COPYRIGHT CHRISTIAN MOTZ 2003, 2004
**
**                This program is free software; you can redistribute
**                it and/or modify it under the terms of the GNU
**                General Public License as published by the Free
**                Software Foundation; either version 2, or (at your
**                option) any later version.
**
** Version:       $Id: dvb.h,v 1.3 2004/04/07 14:46:11 douleftis Exp $
**
** Change Activity:
**
** 030602 -- CMO: Module created.
**
** 030622 -- CMO: Added a few return codes.
**
** 030704 -- CMO: Added RCs and prototypes for audio handling
**                functions.
**
** 040107 -- CMO: Fixed copyright statement to reflect the GPL status
**                of the code.
**
*******************************************************************************/

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


int dvb_open(int, void **);
int dvb_close(void *);
int dvb_tune_qpsk(void *, int, int, char, int, int);
int dvb_status(void *);
int dvb_filter(void *, int);
int dvb_packet(void *, unsigned char *, int);
int dvb_unfilter(void *);
int dvb_section(void *, int, int, int, int, unsigned char *, int);
int dvb_apid(void *, int);
int dvb_apkt(void *, unsigned char *, int, int, int *);
int dvb_dpid(void *, int);
int dvb_dpkt(void *, unsigned char *, int, int, int *);
int dvb_volume(void *, int);
