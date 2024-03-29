/* $Id$ */
/* Read RDS-Radiotext[+] data from MPEG-Frames
   | All methods have shamelessly been stolen from U. Hanke's great
   | extension to the Radio plugin (http://www.egal-vdr.de/plugins/) for
   | the famous Video Disk Recorder (VDR) -- this is GPL at its best! :-)

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
#include "rtxt.h"
#include "log.h"
#include "util.h"

extern gpointer hlog;


static const gchar *pty_string[] = {
  "unknown program type",
  "News",
  "Current affairs",
  "Information",
  "Sport",
  "Education",
  "Drama",
  "Culture",
  "Science",
  "Varied",
  "Pop music",
  "Rock music",
  "M.O.R. music",
  "Light classical",
  "Serious classical",
  "Other music"
};

static const guchar rds_addchar[128] = {
  0xe1, 0xe0, 0xe9, 0xe8, 0xed, 0xec, 0xf3, 0xf2,
  0xfa, 0xf9, 0xd1, 0xc7, 0x8c, 0xdf, 0x8e, 0x8f,
  0xe2, 0xe4, 0xea, 0xeb, 0xee, 0xef, 0xf4, 0xf6,
  0xfb, 0xfc, 0xf1, 0xe7, 0x9c, 0x9d, 0x9e, 0x9f,
  0xaa, 0xa1, 0xa9, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xa3, 0xab, 0xac, 0xad, 0xae, 0xaf,
  0xba, 0xb9, 0xb2, 0xb3, 0xb1, 0xa1, 0xb6, 0xb7,
  0xb5, 0xbf, 0xf7, 0xb0, 0xbc, 0xbd, 0xbe, 0xa7,
  0xc1, 0xc0, 0xc9, 0xc8, 0xcd, 0xcc, 0xd3, 0xd2,
  0xda, 0xd9, 0xca, 0xcb, 0xcc, 0xcd, 0xd0, 0xcf,
  0xc2, 0xc4, 0xca, 0xcb, 0xce, 0xcf, 0xd4, 0xd6,
  0xdb, 0xdc, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
  0xc3, 0xc5, 0xc6, 0xe3, 0xe4, 0xdd, 0xd5, 0xd8,
  0xde, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xf0,
  0xe3, 0xe5, 0xe6, 0xf3, 0xf4, 0xfd, 0xf5, 0xf8,
  0xfe, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

static const gchar *rtp_typename[] = {
  NULL,				/* 0: Class if the RadioText contains no RT+ information */
  "ITEM.TITLE",			/* 1: Title of item; e.g. track title of an album */
  "ITEM.ALBUM",			/* 2: The collection name to which this track belongs */
  "ITEM.TRACKNUMBER",		/* 3: The track number of the item on the album */
  "ITEM.ARTIST",		/* 4: A person or band/collective responsible for the work */
  "ITEM.COMPOSITION",		/* 5: A complete composition */
  "ITEM.MOVEMENT",		/* 6: A movement is a large division of a composition */
  "ITEM.CONDUCTOR",		/* 7: The artist(s) who performed the work. */
  "ITEM.COMPOSER",		/* 8: Name of the original composer/author */
  "ITEM.BAND",			/* 9: Band/orchestra/accompaniment/musician */
  "ITEM.COMMENT",		/* 10: Any comment related to the content */
  "ITEM.GENRE",			/* 11: The main genre of the audio */
  "INFO.NEWS",			/* 12: Message / headline */
  "INFO.NEWS.LOCAL",		/* 13: Local news */
  "INFO.STOCKMARKET",		/* 14: Quote information */
  "INFO.SPORT",			/* 15: Result of a game */
  "INFO.LOTTERY",		/* 16: Raffle / lottery */
  "INFO.HOROSCOPE",		/* 17: Horoscope */
  "INFO.DAILY_DIVERSION",	/* 18: Daily tipp / diversion / joke ... */
  "INFO.HEALTH",		/* 19: Information about health */
  "INFO.EVENT",			/* 20: Info about an event */
  "INFO.SZENE",			/* 21: Information about szene */
  "INFO.CINEMA",		/* 22: Information about movies in cinema */
  "INFO.TV",			/* 23: Information about TV-movies */
  "INFO.DATE_TIME",		/* 24: Information about date and time */
  "INFO.WEATHER",		/* 25: Information about weather */
  "INFO.TRAFFIC",		/* 26: Information about traffic */
  "INFO.ALARM",			/* 27: Alarm information */
  "INFO.ADVERTISEMENT",		/* 28: Info about an advertisement */
  "INFO.URL",			/* 29: Link to url */
  "INFO.OTHER",			/* 30: Other information, not especially specified */
  "STATIONNAME.SHORT",		/* 31: Name describing the radio station (call letters) */
  "STATIONNAME.LONG",		/* 32: Name describing the radio station */
  "PROGRAMME.NOW",		/* 33: EPG info programme now */
  "PROGRAMME.NEXT",		/* 34: EPG info programme next */
  "PROGRAMME.PART",		/* 35: Part of the current radio show */
  "PROGRAMME.HOST",		/* 36: Name of the host of the radio show */
  "PROGRAMME.EDITORIAL_STAFF",	/* 37: Name of the editorial staff */
  "PROGRAMME.FREQUENCY",	/* 38: Information about radio shows */
  "PROGRAMME.HOMEPAGE",		/* 39: Link to radio station homepage */
  "PROGRAMME.SUBCHANNEL",	/* 40: For so-called multicasting applications */
  "PHONE.HOTLINE",		/* 41: The telephone number of the radio station's hotline */
  "PHONE.STUDIO",		/* 42: The telephone number of the radio station's studio */
  "PHONE.OTHER",		/* 43: Name and telephone number */
  "SMS.STUDIO",			/* 44: The sms number of the radio stations studio */
  "SMS.OTHER",			/* 45: Name and sms number */
  "EMAIL.HOTLINE",		/* 46: The email adress of the radio stations hotline */
  "EMAIL.STUDIO",		/* 47: The email adress of the radio stations studio */
  "EMAIL.OTHER",		/* 48: Name and email adress */
  "MMS.OTHER",			/* 49: Name and mms number */
  "CHAT",			/* 50: chat content */
  "CHAT.CENTER",		/* 51: Address, where replies to the chat shall be sent */
  "VOTE.QUESTION",		/* 52: A question (typically binary) */
  "VOTE.CENTER",		/* 53: url or sms number to send the answer to */
  NULL, NULL, NULL, NULL, NULL,	/* 54 .. 58: are reserved for future usage */
  "PLACE",			/* 59: Adds info about a location */
  "APPOINTMENT",		/* 60: Adds info about date and time */
  "IDENTIFIER",			/* 61: For music it is the Int. Standard Recording Code */
  "PURCHASE",			/* 62: Address where item can be purchased */
  "GET_DATA"			/* 63: Retrieves more data about other RT+ information element */
};


static gushort
crc16_ccitt (guchar * daten, gint len, gint skipfirst)
{
  // CRC16-CCITT: x^16 + x^12 + x^5 + 1
  // with start 0xffff and result inverse
  register gushort crc = 0xffff;

  if (skipfirst)
    daten++;

  while (len--)
    {
      crc = (crc >> 8) | (crc << 8);
      crc ^= *daten++;
      crc ^= (crc & 0xff) >> 4;
      crc ^= (crc << 8) << 4;
      crc ^= ((crc & 0xff) << 4) << 1;
    }

  return ~(crc);
}


rtstruct *
radiotext_init (void)
{
  rtstruct *rt = g_malloc0 (sizeof (rtstruct));
  rt->runtoggle = 0xff;
  return rt;
}


static void
radiotext_events_insert (rtstruct * rt, gchar * newtext)
{
  // Shift Radiotext events
  int i;
  if (rt->event[RT_EVNTS - 1] != NULL)
    g_free (rt->event[RT_EVNTS - 1]);
  for (i = RT_EVNTS - 1; i > 0; --i)
    rt->event[i] = rt->event[i - 1];
  rt->event[0] = g_strdup (newtext);
  g_assert (rt->event[RT_EVNTS] == NULL);
}


gchar *
radiotext_events_to_text (const rtstruct * rt)
{
  // GLib API should allow (const gchar * const *) here
  // (see http://bugzilla.gnome.org/show_bug.cgi?id=547199)
  return (rt != NULL ? g_strjoinv ("\n", rt->event) : NULL);
}


static void
radiotext_decode (rtstruct * rt)
{
  guchar *mtext = NULL;
  gint leninfo;
  if (rt == NULL)
    return;

  /*
   * byte 1+2 = ADD (10bit SiteAdress + 6bit EncoderAdress)
   * byte 3   = SQC (Sequence Counter 0x00 = not used)
   * byte 4 = MFL (Message Field Length)
   * byte 5 = MEC (Message Element Code)
   */
  mtext = rt->mtext;
  leninfo = mtext[4];
  if (rt->index >= leninfo + 7)
    {
      if (mtext[5] == 0x0a)	// RT
	{
	  /* byte 6+7 = DSN+PSN (DataSetNumber+ProgramServiceNumber, 
	   *                           ignore here, always 0x00 ?)
	   * byte 8   = MEL (MessageElementLength, max. 64+1 byte @ RT)
	   * byte 9   = RT-Status bits
	   *   bit 0   = AB-Flagcontrol
	   *   bit 1-4 = Transmission-Number
	   *   bit 5+6 = Buffer-Config (ignored, always 0x01?)
	   */
	  gint i, ii;
	  gchar *temptext = NULL;
	  if (mtext[8] == 0 || mtext[8] > RT_MEL || mtext[8] > leninfo - 4)
	    {
	      log_print
		(hlog, LOG_DEBUG,
		 "RT error: Length not correct (MFL= %d, MEL= %d)",
		 mtext[4], mtext[8]);
	      return;
	    }

	  // Decode Radiotext event
	  temptext = g_malloc0 (mtext[8]);
	  for (i = 1, ii = 0; i < mtext[8]; i++)
	    {
	      if (mtext[9 + i] <= 0xfe)
		temptext[ii++] =
		  (mtext[9 + i] >=
		   0x80) ? rds_addchar[mtext[9 + i] - 0x80] : mtext[9 + i];
	    }
	  if (rt->plustext != NULL)
	    g_free (rt->plustext);
	  rt->plustext = g_strndup (temptext, RT_MEL - 1);
	  log_print (hlog, LOG_NOTICE, "Radiotext: %s", rt->plustext);

	  // Update event list if we have a new Radiotext event
	  g_free (temptext);
	  temptext = str_beautify (rt->plustext, 0, DVB_STRING_RADIOTEXT);
	  if (rt->event[0] == NULL || strcmp (temptext, rt->event[0]) != 0)
	    {
	      // Beautify radiotext string and add it to event list
	      radiotext_events_insert (rt, temptext);
	      rt->refresh = TRUE;
	    }
	  if (temptext != NULL)
	    {
	      g_free (temptext);
	      temptext = NULL;
	    }
	}
      else if (mtext[5] == 0x46)	// RTplus
	{
	  /* RTplus tags V2.1 (only if RT)
	   * byte 6   = MEL, only 8 byte for 2 tags
	   * byte 7+8 = ApplicationID, always 0x4bd7
	   * byte 9   = Applicationgroup Typecode / PTY ?
	   * bit 10#4 = Item Togglebit
	   * bit 10#3 = Item Runningbit
	   * Tag1:
	   *   bit 10#2..11#5 = Contenttype
	   *   bit 11#4..12#7 = Startmarker
	   *   bit 12#6..12#1 = Length
	   * Tag2:
	   *   bit 12#0..13#3 = Contenttype
	   *   bit 13#2..14#5 = Startmarker
	   *   bit 14#4..14#0 = Length
	   */
	  gint i;
	  guint rtp_type[2], rtp_start[2], rtp_len[2];
	  if (mtext[6] > leninfo - 2 || mtext[6] != 8)
	    {
	      log_print (hlog, LOG_DEBUG,
			 "RTplus error: Length not correct (MEL= %d, len= %d)",
			 mtext[6], leninfo);
	      return;
	    }

	  // Extract RTplus info
	  rtp_type[0] = (0x38 & mtext[10] << 3) | mtext[11] >> 5;
	  rtp_start[0] = (0x3e & mtext[11] << 1) | mtext[12] >> 7;
	  rtp_len[0] = 0x3f & mtext[12] >> 1;
	  rtp_type[1] = (0x20 & mtext[12] << 5) | mtext[13] >> 3;
	  rtp_start[1] = (0x38 & mtext[13] << 3) | mtext[14] >> 5;
	  rtp_len[1] = 0x1f & mtext[14];
	  log_print (hlog, LOG_INFO,
		     "RTplus: Toggle/Run = %d/%d (was %d/%d), "
		     "tag#1 = %d/%d/%d, tag#2 = %d/%d/%d (type/start/length)",
		     (mtext[10] & 0x10) > 0, (mtext[10] & 0x08) > 0,
		     (rt->runtoggle & 0x10) > 0, (rt->runtoggle & 0x08) > 0,
		     rtp_type[0], rtp_start[0], rtp_len[0], rtp_type[1],
		     rtp_start[1], rtp_len[1]);

	  // Only update when toggle-/running-bits swap
	  if ((mtext[10] & 0x18) != rt->runtoggle)
	    {
	      rt->runtoggle = (mtext[10] & 0x18);
	      // Reset title/artist fields
	      if (rt->title != NULL)
		{
		  g_free (rt->title);
		  rt->title = NULL;
		}
	      if (rt->artist != NULL)
		{
		  g_free (rt->artist);
		  rt->artist = NULL;
		}
	      rt->refresh = TRUE;
	    }

	  // Refresh and display RTplus info
	  for (i = 0; i < 2; i++)
	    {
	      if (rt->plustext != NULL &&
		  rtp_start[i] + rtp_len[i] <= strlen (rt->plustext))
		{
		  gchar *temptext =
		    g_strndup (rt->plustext + rtp_start[i], rtp_len[i] + 1);

		  // Print RTplus info
		  if (rtp_type[i] >= 0 &&
		      rtp_type[i] < sizeof (rtp_typename) &&
		      rtp_typename[rtp_type[i]] != NULL)
		    {
		      log_print (hlog, LOG_INFO,
				 "RTplus[%d]: %s (type %s)", i,
				 temptext, rtp_typename[rtp_type[i]]);
		    }
		  else
		    {
		      log_print (hlog, LOG_INFO,
				 "RTplus[%d]: %s (unknown type %u)", i,
				 temptext, rtp_type[i]);
		    }

		  // Set title or artist if given
		  switch (rtp_type[i])
		    {
		    case 1:	// Item_Title
		      if (is_updated
			  (temptext, &rt->title, DVB_STRING_RADIOTEXT))
			rt->refresh = TRUE;
		      break;
		    case 4:	// Item_Artist
		      if (is_updated
			  (temptext, &rt->artist, DVB_STRING_RADIOTEXT))
			rt->refresh = TRUE;
		      break;
		    }
		  if (temptext != NULL)
		    {
		      g_free (temptext);
		      temptext = NULL;
		    }
		}
	    }
	}
    }
  else
    log_print (hlog, LOG_DEBUG,
	       "RDS error: Length not correct (MFL= %d, len= %d)",
	       mtext[4], rt->index);
}


void
radiotext_read_data (rtstruct * rt, const guchar * data, gint len)
{
  gint i, val;
  const gint mframel = 263;	// max. 255(MSG)+4(ADD/SQC/MFL)+2(CRC)+2(Start/Stop) of RDS-data

  gint offset = len;
  gint rdsl = data[offset - 2];	// RDS DataFieldLength
  // RDS DataSync = 0xfd @ end
  if (data[offset - 1] != 0xfd || rdsl <= 0)
    return;

  // Reverse data from end to start     
  for (i = offset - 3, val; i > offset - 3 - rdsl; i--)
    {
      val = data[i];

      // Start of RDS-data
      if (val == 0xfe)
	{
	  rt->index = -1;
	  rt->rt_start = 1;
	  rt->rt_bstuff = 0;
	}

      // "Middle" of RDS-data
      if (rt->rt_start)
	{
	  // Bytestuffing reverse: 0xfd00->0xfd, 0xfd01->0xfe, 0xfd02->0xff
	  if (rt->rt_bstuff)
	    {
	      switch (val)
		{
		case 0x00:
		  rt->mtext[rt->index] = 0xfd;
		  break;
		case 0x01:
		  rt->mtext[rt->index] = 0xfe;
		  break;
		case 0x02:
		  rt->mtext[rt->index] = 0xff;
		  break;
		default:
		  // Should never be!
		  rt->mtext[++rt->index] = val;
		  g_assert (FALSE);
		}
	      rt->rt_bstuff = 0;
	    }
	  else
	    rt->mtext[++rt->index] = val;

	  // Check for stuffing
	  if (val == 0xfd && rt->index > 0)
	    rt->rt_bstuff = 1;

	  // Early check for used MEC
	  if (rt->index == 5)
	    {
	      switch (val)
		{
		case 0x07:	// PTY
		case 0x0a:	// Radiotext
		case 0x46:	// ODA-Data
		  rt->mec = val;
		  break;
		default:
		  rt->rt_start = 0;
		  log_print (hlog, LOG_DEBUG,
			     "mec %02x unknown, ignored", val);
		}
	    }

	  // max. rdslength, garbage ?
	  if (rt->index >= mframel)
	    {
	      log_print (hlog, LOG_DEBUG, "RDS error: too long, garbage");
	      rt->rt_start = 0;
	    }
	}

      // End of RDS-data
      if (rt->rt_start && val == 0xff)
	{
	  rt->rt_start = 0;

	  //  min. rdslength, garbage ?
	  if (rt->index >= 9)
	    {
	      // crc16-check
	      unsigned short crc16 =
		crc16_ccitt (rt->mtext, rt->index - 3, 1);
	      if (crc16 ==
		  (rt->mtext[rt->index - 2] << 8) + rt->mtext[rt->index - 1])
		{
		  switch (rt->mec)
		    {
		    case 0x07:	// PTY
		      log_print (hlog, LOG_DEBUG, "mec %d: PTY", rt->mec);
		      if (rt->mtext[8] <= 15)
			{
			  log_print (hlog, LOG_NOTICE,
				     "RDS-PTY set to '%s'",
				     pty_string[rt->mtext[8]]);
			  if (rt->pty != NULL)
			    g_free (rt->pty);
			  rt->pty = g_strdup (pty_string[rt->mtext[8]]);
			  rt->refresh = TRUE;
			}
		      else
			log_print (hlog, LOG_DEBUG,
				   "RDS-PTY has unknown value '%d'",
				   rt->mtext[8]);
		      break;
		    case 0x0a:	// Radiotext
		      log_print (hlog, LOG_DEBUG, "mec %02x: Radiotext",
				 rt->mec);
		      radiotext_decode (rt);
		      break;
		    case 0x46:	// ODA-Data
		      log_print (hlog, LOG_DEBUG, "mec %02x: ODA-Data",
				 rt->mec);
		      if ((rt->mtext[7] << 8) + rt->mtext[8] == 0x4bd7)
			{
			  log_print (hlog, LOG_DEBUG, "mec %02x: RT+",
				     rt->mec);
			  radiotext_decode (rt);
			}
		      break;
		    default:
		      log_print (hlog, LOG_DEBUG,
				 "mec %02x: unknown, ignored", rt->mec);
		      break;
		    }
		}
	      else
		log_print (hlog, LOG_INFO,
			   "RDS error: CRC: calc = %04x != transmit = %02x%02x",
			   crc16, rt->mtext[rt->index - 2],
			   rt->mtext[rt->index - 1]);
	    }
	  else
	    log_print (hlog, LOG_DEBUG, "RDS error: too short, garbage");
	}
    }
}


void
radiotext_exit (rtstruct * rt)
{
  g_free (rt);
}
