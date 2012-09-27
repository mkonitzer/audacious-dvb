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
#include <glib/gprintf.h>
#include <string.h>

#include "epg.h"
#include "log.h"
#include "dvb.h"
#include "util.h"


extern gpointer hlog;


epgstruct *
epg_init (void)
{
  return g_malloc0 (sizeof (epgstruct));
}


static gchar *
stream_type (gint cnt, gint cty)
{
  gchar *ret = "unknown";
  if (cnt == 0x02)
    {
      switch (cty)
	{
	case 0x01:
	  ret = "Single mono";
	  break;
	case 0x02:
	  ret = "Dual mono";
	  break;
	case 0x03:
	  ret = "Stereo (2 channel)";
	  break;
	case 0x04:
	  ret = "Multi-lingual, multi-channel";
	  break;
	case 0x05:
	  ret = "Surround sound";
	  break;
	case 0x40:
	  ret = "Visually impaired";
	  break;
	case 0x41:
	  ret = "Hard of hearing";
	  break;
	}
    }
  return g_strdup (ret);
}


static void
dvb_eit_desc (epgstruct * epg, const guchar * d, gint l)
{
  guint cnt, cty, dt, dl;
  gint i, j, cdn, ldn, loi;
  gchar ll[1024], hex[16], lang[4];
  gchar *name = NULL, *text = NULL, *newtext = NULL, *exttext = NULL, *stype =
    NULL;
  const guchar *p = d, *q = NULL;
  guint pil_mday, pil_mon, pil_hour, pil_min;

  while (p < &d[l])
    {
      dt = *p++;
      dl = *p++;
      q = p;

      switch (dt)
	{
	case 0x4d:		// short_event_descriptor
	  if (dl < 5)
	    {
	      log_print (hlog, LOG_DEBUG, "EIT[short_event_descriptor]: "
			 "wrong description length: %u (expected >=5)", dl);
	      break;
	    }

	  memcpy (lang, q, 3);	// ISO_639_language_code
	  lang[3] = '\0';
	  q += 3;

	  j = *q++;		// event_name_length
	  if (j > 0)		// event_name_char
	    {
	      name = g_strndup ((gchar *) q, j);
	      q += j;
	    }

	  j = *q++;		// text_length
	  if (j > 0)		// text_char
	    text = g_strndup ((gchar *) q, j);

	  if (is_updated (name, &epg->short_ev_name, DVB_STRING_DVBSI))
	    epg->refresh = TRUE;

	  if (is_updated (text, &epg->short_ev_text, DVB_STRING_DVBSI))
	    epg->refresh = TRUE;

	  if (epg->refresh)
	    log_print (hlog, LOG_NOTICE, "EIT[short_event_descriptor]: "
		       "%s,\"%s\",\"%s\"", lang, name, text);

	  if (name != NULL)
	    {
	      g_free (name);
	      name = NULL;
	    }
	  if (text != NULL)
	    {
	      g_free (text);
	      text = NULL;
	    }
	  break;

	case 0x4e:		// extended_event_descriptor
	  if (dl < 6)
	    {
	      log_print (hlog, LOG_DEBUG, "EIT[extended_event_descriptor]: "
			 "wrong description length: %u (expected >=6)", dl);
	      break;
	    }

	  cdn = q[0] >> 4;	// descriptor_number
	  ldn = q[0] & 0xf;	// last_descriptor_number
	  q++;

	  memcpy (lang, q, 3);	// ISO_639_language_code
	  lang[3] = '\0';
	  q += 3;

	  loi = *q++;		// length_of_items
	  q += loi;

	  j = *q++;		// text_length
	  if (j > 0)
	    {			// text_char
	      gchar *tmp = exttext;
	      newtext = g_strndup ((gchar *) q, j);
	      if (exttext == NULL)
		exttext = g_strdup (newtext);
	      else
		// we may assume, that all events have the same encoding
		exttext = g_strconcat (exttext, &newtext[1], NULL);
	      if (tmp != NULL)
		{
		  g_free (tmp);
		  tmp = NULL;
		}
	    }

	  log_print (hlog, LOG_NOTICE, "EIT[extended_event_descriptor]: "
		     "%d/%d,%s,\"%s\"", cdn, ldn, lang, newtext);

	  if (newtext != NULL)
	    {
	      g_free (newtext);
	      newtext = NULL;
	    }

	  // Free exttext if we just processed the last ext. packet
	  if (cdn == ldn)
	    {
	      if (is_updated (exttext, &epg->ext_ev_text, DVB_STRING_DVBSI))
		epg->refresh = TRUE;

	      if (exttext != NULL)
		{
		  g_free (exttext);
		  exttext = NULL;
		}
	    }
	  break;

	case 0x50:		// component_descriptor
	  if (dl < 6)
	    {
	      log_print (hlog, LOG_DEBUG, "EIT[component_descriptor]: "
			 "wrong description length: %u (expected >=6)", dl);
	      break;
	    }

	  cnt = q[0] & 0xf;	// stream_content
	  cty = q[1];		// component_type
	  stype = stream_type (cnt, cty);
	  q += 3;

	  memcpy (lang, q, 3);	// ISO_639_language_code
	  lang[3] = '\0';

	  if (is_updated (stype, &epg->stream_type, DVB_STRING_DVBSI))
	    epg->refresh = TRUE;
	  if (is_updated (lang, &epg->lang, DVB_STRING_DVBSI))
	    epg->refresh = TRUE;
	  if (epg->refresh)
	    log_print (hlog, LOG_NOTICE, "EIT[component_descriptor]: "
		       "%s, %s (%lu,%lu)", lang, stype, cnt, cty);

	  if (stype != NULL)
	    {
	      g_free (stype);
	      stype = NULL;
	    }
	  break;

	case 0x69:		// PDC_descriptor
	  if (dl != 3)
	    {
	      log_print (hlog, LOG_DEBUG, "EIT[PDC_descriptor]: "
			 "wrong description length: %u (expected 3)", dl);
	      break;
	    }

	  pil_mday = ((q[0] << 1) & 0x1e) | (q[1] >> 7);
	  pil_mon = ((q[1] >> 3) & 0xf);
	  pil_hour = ((q[1] << 2) & 0x1c) | (q[2] >> 6);
	  pil_min = (q[2] & 0x1f);

	  if (epg->pil_mday != pil_mday || epg->pil_mon != pil_mon
	      || epg->pil_hour != pil_hour || epg->pil_min != pil_min)
	    {
	      epg->pil_mday = pil_mday;
	      epg->pil_mon = pil_mon;
	      epg->pil_hour = pil_hour;
	      epg->pil_min = pil_min;
	      epg->refresh = TRUE;
	    }

	  if (epg->refresh)
	    log_print (hlog, LOG_NOTICE, "EIT[PDC_descriptor]: "
		       "%02u.%02u. %02u:%02u", pil_mday, pil_mon, pil_hour,
		       pil_min);
	  break;

	default:
	  ll[0] = '\0';
	  if (dl > 0)
	    {
	      for (i = 0; i < dl; i++)
		{
		  g_sprintf (hex, "%02x", p[i]);
		  g_strlcat (ll, hex, sizeof (ll));
		}
	    }
	  log_print (hlog, LOG_DEBUG, "EIT[dt]: %s", ll);
	  break;
	}

      p += dl;
    }

  if (exttext != NULL)
    {
      g_free (exttext);
      exttext = NULL;
    }
}


gint
epg_read_data (epgstruct * epg, const guchar * sect, gint len)
{
  gint dll, rst;
  const guchar *p;
  static guchar un[4096];

  if (len == 0)
    {
      memset (un, 0x00, sizeof (un));
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
	      memcpy (un, p, dll + 12);
	      log_print (hlog, LOG_DEBUG,
			 "EIT: %02x%02x %02x%02x %02x%02x%02x %02x%02x%02x",
			 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8],
			 p[9]);

	      log_print (hlog, LOG_DEBUG, "EIT: %d", dll);
	      p += 12;
	      dvb_eit_desc (epg, p, dll);
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


void
epg_exit (epgstruct * epg)
{
  if (epg != NULL)
    g_free (epg);
}
