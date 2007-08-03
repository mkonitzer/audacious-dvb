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

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>


void dvb_gui_init (void);
void dvb_about (void);
void dvb_configure (void);
void dvb_getinfo (char *);
void dvb_info_update (char *, char *);

static void dvb_about_destroy (GtkWidget *, gpointer);
static void dvb_configure_destroy (GtkWidget *, gpointer);
static void dvb_info_destroy (GtkWidget *, gpointer);
static gboolean dvb_config_ok (GtkWidget *, GdkEvent *);
static gboolean dvb_config_apply (GtkWidget *, GdkEvent *);
