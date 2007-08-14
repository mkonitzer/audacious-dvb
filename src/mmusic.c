/* $Id$ */
/* Read Mad Music info, a greek Nova subscription service (Hotbird 13° East)

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
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mmusic.h"
#include "log.h"

extern gpointer hlog;


mmstruct *
madmusic_init (void)
{
  return g_malloc0(sizeof(mmstruct));
}


static void
fix_bill_shit (guchar * s)
{
  while (*s)
    {
      switch (*s)
	{
	case 0xa1:
	  *s = 0xb5;
	  break;
	case 0xa2:
	  *s = 0xb6;
	  break;
	}
      s++;
    }
}


static void
replace_crlf (guchar * s)
{
  while (*s)
    {
      if ((*s == '\n') || (*s == '\r'))
	*s = ' ';
      s++;
    }
}


void
madmusic_decode (mmstruct *mm, guchar * buf, gint len)
{
  gint field, ftna, toai;
  gchar toan[32];
  guchar *p, *q, *r, *rr, wbuf[8192];

  if (len == mm->mad_len && memcmp (mm->mad_buf, buf, len) == 0)
    return;

  memcpy (mm->mad_buf, buf, len);
  mm->mad_len = len;
  time (&mm->mad_time);

  toai = 0;
  toan[toai] = '\0';

  mm->trnum = 0;
  memset (mm->artist, 0x00, sizeof (mm->artist));
  memset (mm->title, 0x00, sizeof (mm->title));
  memset (mm->album, 0x00, sizeof (mm->album));

  memset (wbuf, 0x00, sizeof (wbuf));
  memcpy (wbuf, buf, len);
  fix_bill_shit (wbuf);
  p = wbuf;
  field = 0;

  ftna = 0;

  while (1)
    {
      q = index (p, '|');
      if (q == NULL)
	break;

      *q = '\0';
      q++;

      switch (field)
	{
	case 3:
	  strcpy (mm->artist, p);
	  replace_crlf (mm->artist);
	  break;
	case 4:
	  strcpy (mm->title, p);
	  replace_crlf (mm->title);
	  ftna = 1;
	  break;
	case 5:
	  strcpy (mm->album, p);
	  replace_crlf (mm->album);
	  break;
	}

      if (ftna)
	{
	  if ((r = strstr (p, mm->title)) != NULL)
	    {
	      rr = r - 4;
	      if (rr < p)
		rr = p;

	      toai = 0;
	      toan[toai] = '\0';
	      while (rr < r)
		{
		  if ((*rr >= '0') && (*rr <= '9'))
		    toan[toai++] = *rr;
		  rr++;
		}
	      toan[toai] = '\0';
	      mm->trnum = atoi (toan);
	    }
	}

      p = q;
      field++;

      if (p >= (wbuf + len))
	break;

      if (*p == '^')
	ftna = 0;
    }

  log_print (hlog, LOG_INFO, "MadMusic Album : %s", mm->album);
  log_print (hlog, LOG_INFO, "MadMusic Track : %d", mm->trnum);
  log_print (hlog, LOG_INFO, "MadMusic Artist: %s", mm->artist);
  log_print (hlog, LOG_INFO, "MadMusic Title : %s", mm->title);

  /*if (strlen (artist) > 0)
     g_sprintf (service_name, "%s - %s", artist, title);
     else
     g_sprintf (service_name, "%s", title);
     si_update++; */
}


void
madmusic_exit (mmstruct *mm)
{
/*  if (strlen (title) > 0)
    {
      if ((mad_time > 0) && (t_start <= mad_time))
	log_print (hlog, LOG_INFO, "%d,%s,%s,%s", trnum, album, artist,
		   title);
      else
	log_print (hlog, LOG_INFO, "Track info %d:%02d too old",
		   (t_start - mad_time) / 60, (t_start - mad_time) % 60);
    }*/
  g_free(mm);
}