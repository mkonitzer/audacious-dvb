/* $Id$ */
/* Read RDS-Radiotext[+] data from MPEG-Frames
   | All methods have shamelessly been stolen from U. Hanke's great
   | extension to the radio plugin (http://www.egal-vdr.de/plugins/) for
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

#ifndef __AUDACIOUS_DVB_RADIOTEXT_H__
#define __AUDACIOUS_DVB_RADIOTEXT_H__

void radiotext_read_frame (const unsigned char *data, int len);

#endif // __AUDACIOUS_DVB_RADIOTEXT_H__
