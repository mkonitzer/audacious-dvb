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
#include "util.h"


extern gpointer hlog;



epgstruct *
epg_init (void)
{
  return g_malloc0 (sizeof (epgstruct));
}


static gchar*
stream_type (gint cnt, gint cty)
{
  gchar *ret = "unknown";
  if ( cnt == 0x02 )
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
  return g_strdup(ret);
}


static gint
dvb_eit_desc (epgstruct * epg, const guchar * d, gint l)
{
  gint dt, dl, i, j, cdn, ldn, loi, cnt, cty;
  gchar ll[1024], hex[16], lg[4], *name, *text, *newtext, *exttext, *stype;
  const guchar *p, *q;

  p = d;

  while (p < &d[l])
    {
      dt = *p++;
      dl = *p++;
      q = p;

      switch (dt)
	{
	case 0x4d:		// short_event_descriptor
	  memcpy (lg, q, 3);	// ISO_639_language_code
	  lg[3] = '\0';
	  q += 3;

	  j = *q++;		// event_name_length
	  if (j > 0)		// event_name_char
	    name = str_beautify (q, j, FALSE);

	  j = *q++;		// text_length
	  if (j > 0)		// text_char
	    text = str_beautify (q, j, FALSE);

	  if (is_updated (name, &epg->short_ev_name, FALSE))
	    epg->refresh = TRUE;

	  if (is_updated (text, &epg->short_ev_text, FALSE))
	    epg->refresh = TRUE;

	  if (epg->refresh)
	    log_print (hlog, LOG_INFO, "EIT: %s,\"%s\",\"%s\"", lg, name,
		       text);

	  g_free (name);
	  g_free (text);
	  name = text = NULL;
	  break;

	case 0x4e:		// extended_event_descriptor
	  cdn = q[0] >> 4;	// descriptor_number
	  ldn = q[0] & 0xf;	// last_descriptor_number
	  q++;

	  memcpy (lg, q, 3);	// ISO_639_language_code
	  lg[3] = '\0';
	  q += 3;

	  loi = *q++;		// length_of_items
	  q += loi;

	  j = *q++;		// text_length
	  if (j > 0)
	    {			// text_char
	      gchar *tmp = exttext;
	      newtext = str_beautify (q, j, FALSE);

	      exttext =
		g_strconcat ((exttext != NULL ? exttext : ""), newtext, NULL);
	      g_free (tmp);
	    }

	  log_print (hlog, LOG_INFO, "EIT: %d/%d,%s,\"%s\"", cdn, ldn, lg,
		     newtext);

	  g_free (newtext);
	  newtext = NULL;

	  // Free exttext if we just processed the last ext. packet
	  if (cdn == ldn)
	    {
	      if (is_updated (exttext, &epg->ext_ev_text, FALSE))
		epg->refresh = TRUE;

	      g_free (exttext);
	      exttext = NULL;
	    }
	  break;
	  
	case 0x50:		// component_descriptor
	    cnt = q[0] & 0xf;	// stream_content
	    cty = q[1];		// component_type
	    stype = stream_type (cnt, cty);
	    q+=3;
	  
	    memcpy (lg, q, 3);	// ISO_639_language_code
	    lg[3] = '\0';
	  
	    if (is_updated (stype, &epg->stream_type, FALSE))
	      epg->refresh = TRUE;
	    if (is_updated (lg, &epg->lang, FALSE))
	      epg->refresh = TRUE;
	  
	    g_free(stype);
	    stype = NULL;
	  
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

  g_free (exttext);
  exttext = NULL;

  return RC_OK;
}

gint
epg_read_data (epgstruct * epg, const guchar * sect, gint len)
{
  gint dll, rc, rst;
  const guchar *p, *q;
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
	      log_print (hlog, LOG_INFO,
			 "EIT: %02x%02x %02x%02x %02x%02x%02x %02x%02x%02x",
			 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8],
			 p[9]);

	      log_print (hlog, LOG_DEBUG, "EIT: %d", dll);
	      p += 12;
	      rc = dvb_eit_desc (epg, p, dll);
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
  g_free (epg);
}
