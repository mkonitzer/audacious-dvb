/* $Id$ */
/* Methods for retrieving EPG information over DVB

   Copyright (C) 2007  Marius Konitzer
   Copyright (C) 2003, 2004  Christian Motz
   This file is part of audacious-dvb.

   webchanges is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   webchanges is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with webchanges; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "epg.h"
#include "log.h"
#include "dvb.h"


extern int  playing, epg_running, si_update;
extern void *hlog, *hdvb;

char  epg_desc[4096];

static pthread_mutex_t  epg_mutex = PTHREAD_MUTEX_INITIALIZER;


/*******************************************************************************
** Function: dvb_epg()
**
** Description: 
**
*******************************************************************************/

void *dvb_epg(void *arg)
{
  int           sid, rc, len, sct;
  unsigned char s[4096];

  log_print(hlog, LOG_INFO, "dvb_epg() thread started");

  pthread_mutex_lock(&epg_mutex);

  sid = (int)arg;
  log_print(hlog, LOG_DEBUG, "EPG SID: %d (0x%04x)", sid, sid);

  sct = 0;

  dvb_parse_eit(NULL, 0);

  while (playing) {
    if ((rc = dvb_section(hdvb, 0x0012, 0x4e, sid, sct, s, 1000)) != RC_OK) {
      if (rc != RC_DVB_SECTION_SELECT_TIMEOUT) {
        break;
      }
    } else {
      len = 3 + (((s[1] << 8) | s[2]) & 0xfff);

      log_print(hlog, LOG_DEBUG, "EPG section lenght = %d", len);

      rc = dvb_parse_eit(s, len);

      if (s[6] != s[7]) {
        sct++;
      } else {
        sct = 0;
      }
    }    
  }

  log_print(hlog, LOG_INFO, "dvb_epg() thread stopping");

  pthread_mutex_unlock(&epg_mutex);
  pthread_exit(NULL);
}


/*******************************************************************************
** Function: dvb_parse_eit()
**
** Description: 
**
*******************************************************************************/

int dvb_parse_eit(unsigned char *sect, int len)
{
  int                   dll, rc, rst;
  unsigned char         *p, *q;
  static unsigned char  un[4096];

  if (len == 0) {
    memset(un, 0x00, sizeof(un));
    memset(epg_desc, 0x00, sizeof(epg_desc));
    return (RC_OK);
  }

  p = &sect[14];

  while (p < &sect[len - 4]) {
    rst = p[10] >> 5;
    dll = ((p[10] << 8) | p[11]) & 0xfff;

    if (rst == 4) {
      if (memcmp(un, p, dll + 12) != 0) {
        memset(epg_desc, 0x00, sizeof(epg_desc));
        memcpy(un, p, dll + 12);
        log_print(hlog, LOG_INFO, "EIT: %02x%02x %02x%02x %02x%02x%02x %02x%02x%02x",
                  p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);

        log_print(hlog, LOG_DEBUG, "EIT: %d", dll);
        p += 12;
        rc = dvb_eit_desc(p, dll);
      } else {
        p += 12;
      }
    } else {
      p += 12;
    }      

    p += dll;
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_eit_desc()
**
** Description: 
**
*******************************************************************************/

int dvb_eit_desc(unsigned char *d, int l)
{
  int           dt, dl, i, j, cdn, ldn, loi;
  char          ll[1024], hex[16], lg[4], name[256], text[256];
  unsigned char *p, *q;

  p = d;

  while (p < &d[l]) {
    dt = *p++;
    dl = *p++;

    q = p;

    switch (dt) {
      case 0x4d:
        memcpy(lg, q, 3);
        lg[3] = '\0';
        q += 3;
        j = *q++;
        if (j > 0) {
          for (i = 0; i < j; i++) {
            name[i] = *q++;
          }
        }
        name[j] = '\0';
        
        j = *q++;
        if (j > 0) {
          for (i = 0; i < j; i++) {
            text[i] = *q++;
          }
        }
        text[j] = '\0';

        dvb_clean_string(name);
        dvb_clean_string(text);

        strcpy(epg_desc, name);
        if (strlen(text) > 0) {
          strcat(epg_desc, " - ");
          strcat(epg_desc, text);
        }
        si_update++;

        log_print(hlog, LOG_INFO, "EIT: %s,\"%s\",\"%s\"", lg, name, text);
        break;

      case 0x4e:
        cdn = q[0] >> 4;
        ldn = q[0] & 0xf;
        q++;
        memcpy(lg, q, 3);
        lg[3] = '\0';
        q += 3;
        loi = *q++;
        if (loi > 0) {
          ;
        }
        q += loi;

        j = *q++;
        if (j > 0) {
          for (i = 0; i < j; i++) {
            text[i] = *q++;
          }
        }
        text[j] = '\0';

        dvb_clean_string(text);

        if (strlen(epg_desc) > 0) {
          strcat(epg_desc, " - ");
        }
        strcat(epg_desc, text);
        si_update++;

        log_print(hlog, LOG_INFO, "EIT: %d/%d,%s,\"%s\"", cdn, ldn, lg, text);
        break;

      default:
        sprintf(ll, "%02x", dt);
        if (dl > 0) {
          strcat(ll, ":");
          for (i = 0; i < dl; i++) {
            sprintf(hex, "%02x", p[i]);
            strcat(ll, hex);
          }
        }
        log_print(hlog, LOG_INFO, "EIT: %s", ll);
        break;
    }

    p += dl;
  }

  return (RC_OK);
}


/*******************************************************************************
** Function: dvb_clean_string()
**
** Description: This removes anything the providers throw into their
**              SI data that might annoy us: codeset selection,
**              highlighting and the like. All we use at this point
**              is the printable stuff, even though it is technically
**              incorrect to do so.
**
*******************************************************************************/

void dvb_clean_string(char *s)
{
  int   i, l;
  char  *ws;

  if ((ws = malloc(1 + strlen(s))) != NULL) {
    l = 0;
    for (i = 0; i < strlen(s); i++) {
      if ((s[i] & 0x7f) >= 32) {
        ws[l++] = s[i];
      } else {
        ws[l++] = ' ';
      }
    }
    ws[l] = '\0';
    strcpy(s, ws);
    free(ws);
  }

  return;
}


