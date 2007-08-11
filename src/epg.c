/* $Id$ */
/* Methods for retrieving EPG information over DVB

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
#include <string.h>

#include "epg.h"
#include "log.h"
#include "dvb.h"


extern gint playing, epg_running, si_update;
extern gpointer hlog, hdvb;

gchar epg_desc[4096];

static GMutex *gmt_epg = NULL;


gpointer
dvb_epg (gpointer arg)
{
  gint sid, rc, len, sct;
  guchar s[4096];

  log_print (hlog, LOG_INFO, "dvb_epg() thread started");

  if (gmt_epg == NULL)
    gmt_epg = g_mutex_new ();
  g_mutex_lock (gmt_epg);

  sid = (int) arg;
  log_print (hlog, LOG_DEBUG, "EPG SID: %d (0x%04x)", sid, sid);

  sct = 0;

  dvb_parse_eit (NULL, 0);

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

	  rc = dvb_parse_eit (s, len);

	  if (s[6] != s[7])
	    sct++;
	  else
	    sct = 0;
	}
    }

  log_print (hlog, LOG_INFO, "dvb_epg() thread stopping");

  g_mutex_unlock (gmt_epg);
  gmt_epg = NULL;
  g_thread_exit (0);
}


gint
dvb_parse_eit (guchar * sect, gint len)
{
  gint dll, rc, rst;
  guchar *p, *q;
  static guchar un[4096];

  if (len == 0)
    {
      memset (un, 0x00, sizeof (un));
      memset (epg_desc, 0x00, sizeof (epg_desc));
      return RC_OK;
    }

  p = &sect[14];

  while (p < &sect[len - 4])
    {
      rst = p[10] >> 5;
      dll = ((p[10] << 8) | p[11]) & 0xfff;

      if (rst == 4)
	{
	  if (memcmp (un, p, dll + 12) != 0)
	    {
	      memset (epg_desc, 0x00, sizeof (epg_desc));
	      memcpy (un, p, dll + 12);
	      log_print (hlog, LOG_INFO,
			 "EIT: %02x%02x %02x%02x %02x%02x%02x %02x%02x%02x",
			 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8],
			 p[9]);

	      log_print (hlog, LOG_DEBUG, "EIT: %d", dll);
	      p += 12;
	      rc = dvb_eit_desc (p, dll);
	    }
	  else
	    p += 12;
	}
      else
	p += 12;

      p += dll;
    }

  return RC_OK;
}


gint
dvb_eit_desc (guchar * d, gint l)
{
  gint dt, dl, i, j, cdn, ldn, loi;
  gchar ll[1024], hex[16], lg[4], name[256], text[256];
  guchar *p, *q;

  p = d;

  while (p < &d[l])
    {
      dt = *p++;
      dl = *p++;
      q = p;

      switch (dt)
	{
	case 0x4d:
	  memcpy (lg, q, 3);
	  lg[3] = '\0';
	  q += 3;
	  j = *q++;
	  if (j > 0)
	    {
	      for (i = 0; i < j; i++)
		name[i] = *q++;
	    }
	  name[j] = '\0';

	  j = *q++;
	  if (j > 0)
	    {
	      for (i = 0; i < j; i++)
		text[i] = *q++;
	    }
	  text[j] = '\0';

	  dvb_clean_string (name);
	  dvb_clean_string (text);

	  strcpy (epg_desc, name);
	  if (strlen (text) > 0)
	    {
	      strcat (epg_desc, " - ");
	      strcat (epg_desc, text);
	    }
	  si_update++;

	  log_print (hlog, LOG_INFO, "EIT: %s,\"%s\",\"%s\"", lg, name, text);
	  break;

	case 0x4e:
	  cdn = q[0] >> 4;
	  ldn = q[0] & 0xf;
	  q++;
	  memcpy (lg, q, 3);
	  lg[3] = '\0';
	  q += 3;
	  loi = *q++;
	  if (loi > 0)
	    {
	      ;
	    }
	  q += loi;

	  j = *q++;
	  if (j > 0)
	    {
	      for (i = 0; i < j; i++)
		text[i] = *q++;
	    }
	  text[j] = '\0';

	  dvb_clean_string (text);

	  if (strlen (epg_desc) > 0)
	    strcat (epg_desc, " - ");
	  strcat (epg_desc, text);
	  si_update++;

	  log_print (hlog, LOG_INFO, "EIT: %d/%d,%s,\"%s\"", cdn, ldn, lg,
		     text);
	  break;

	default:
	  g_sprintf (ll, "%02x", dt);
	  if (dl > 0)
	    {
	      strcat (ll, ":");
	      for (i = 0; i < dl; i++)
		{
		  g_sprintf (hex, "%02x", p[i]);
		  strcat (ll, hex);
		}
	    }
	  log_print (hlog, LOG_INFO, "EIT: %s", ll);
	  break;
	}

      p += dl;
    }

  return RC_OK;
}


void
dvb_clean_string (gchar * s)
{
  gint i, l;
  gchar *ws;

  if ((ws = g_malloc (1 + strlen (s))) != NULL)
    {
      l = 0;
      for (i = 0; i < strlen (s); i++)
	{
	  if ((s[i] & 0x7f) >= 32)
	    ws[l++] = s[i];
	  else
	    ws[l++] = ' ';
	}
      ws[l] = '\0';
      strcpy (s, ws);
      g_free (ws);
    }
}
