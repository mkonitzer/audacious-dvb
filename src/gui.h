/* $Id$ */
/* Everything dealing with the graphical user interface

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

#ifndef __AUDACIOUS_DVB_GUI_H__
#define __AUDACIOUS_DVB_GUI_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glade/glade.h>

#include "cfg.h"
#include "dvb.h"
#include "epg.h"
#include "rtxt.h"
#include "util.h"
#include "mmusic.h"

typedef struct _Widgets
{
  // Configuration Window
  GtkWidget *configBox;
  GtkWidget *loggingCombo;
  GtkWidget *devnoSpin;
  GtkWidget *recordCheck;
  GtkWidget *fnameEntry;
  GtkWidget *fnameLabel;
  GtkWidget *appendCheck;
  GtkWidget *splitFrame;
  GtkWidget *isplitCheck;
  GtkWidget *isplitSpin;
  GtkWidget *vsplitCheck;
  GtkWidget *vsplitvolSpin;
  GtkWidget *vsplit1Label;
  GtkWidget *vsplit2Label;
  GtkWidget *vsplitdurSpin;
  GtkWidget *vsplit3Label;
  GtkWidget *vsplit4Label;
  GtkWidget *vsplitminlenSpin;
  GtkWidget *vsplit5Label;
  GtkWidget *dvbCheck;
  GtkWidget *rtCheck;
  GtkWidget *epgCheck;
  GtkWidget *madCheck;

  // Information Window
  GladeXML *infoXml;
  GtkWidget *infoBox;
} Widgets;

void dvb_about (void);
void dvb_configure (void);
void dvb_show_infobox (statstruct *, rtstruct *, epgstruct *, mmstruct *);
void dvb_hide_infobox ();

void infobox_update_service (statstruct *);
void infobox_update_radiotext (rtstruct *);
void infobox_update_epg (epgstruct *);
void infobox_update_mmusic (mmstruct *);
void infobox_update_dvb (dvbstatstruct *);

#endif // __AUDACIOUS_DVB_GUI_H__
