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
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glade/glade.h>

#include "gui.h"
#include "log.h"
#include "cfg.h"
#include "dvb.h"
#include "epg.h"
#include "glwidgets.h"
#include "rtxt.h"
#include "mmusic.h"
#include "config.h"
#include "util.h"

extern gpointer hlog;
extern cfgstruct *config;

static Widgets widgets = { NULL };

static void config_to_gui (const cfgstruct *);
static void dvb_configure_ok (GtkWidget *, gpointer);
static void channelLogosClicked (GtkWidget *, gpointer);
static void logToFileClicked (GtkWidget *, gpointer);
static void recordClicked (GtkWidget *, gpointer);
static void isplitClicked (GtkWidget *, gpointer);
static void vsplitClicked (GtkWidget *, gpointer);
static void config_from_gui (cfgstruct *);


void
dvb_about (void)
{
  const gchar *authors[] = {
    "Marius Konitzer <m.konitzer@gmx.de>",
    "Christian Motz <douleftis@users.sourceforge.net>",
    NULL
  };

  gtk_show_about_dialog (NULL,
			 "name", "audacious-dvb",
			 "comments", "DVB Input Plugin for Audacious",
			 "version", VERSION,
			 "copyright",
			 "Copyright (C) 2007  Marius Konitzer\n\n"
			 "Based on xmms-dvb:\n"
			 "Copyright (C) 2003, 2004  Christian Motz",
			 "authors", authors, "license",
			 "audacious-dvb is free software; you can redistribute "
			 "it and/or modify\nit under the terms of the GNU General "
			 "Public License as published by\nthe Free Software "
			 "Foundation; either version 2 of the License, or\n(at "
			 "your option) any later version.\n\naudacious-dvb is "
			 "distributed in the hope that it will be useful, but\n"
			 "WITHOUT ANY WARRANTY; without even the implied warranty "
			 "of\nMERCHANTABILITY or FITNESS FOR A PARTICULAR "
			 "PURPOSE. See the GNU\nGeneral Public License for more "
			 "details.\n\nYou should have received a copy of the GNU "
			 "General Public License\nalong with audacious-dvb; if "
			 "not, write to the Free Software Foundation,\nInc., 51 "
			 "Franklin St, Fifth Floor, Boston, MA  02110-1301  USA",
			 NULL);
}


void
dvb_configure (void)
{
  if (widgets.configBox)
    {
      gtk_window_present (GTK_WINDOW (widgets.configBox));
      return;
    }

  // Create configuration box
  widgets.configXml =
    glade_xml_new_from_buffer (glwidgets, strlen (glwidgets), "config", NULL);
  glade_xml_signal_autoconnect (widgets.configXml);
  widgets.configBox = glade_xml_get_widget (widgets.configXml, "config");

  // Register signal handlers
  g_signal_connect (G_OBJECT (widgets.configBox), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &widgets.configBox);
  g_signal_connect_swapped (G_OBJECT
			    (glade_xml_get_widget
			     (widgets.configXml, "cancelButton")), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    GTK_OBJECT (widgets.configBox));
  g_signal_connect (G_OBJECT (glade_xml_get_widget
			      (widgets.configXml, "okButton")), "clicked",
		    G_CALLBACK (dvb_configure_ok), NULL);

  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "channelLogosCheck")), "clicked",
		      G_CALLBACK (channelLogosClicked), NULL);
  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "logToFileCheck")), "clicked",
		      G_CALLBACK (logToFileClicked), NULL);
  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "reconplayCheck")), "clicked",
		      G_CALLBACK (recordClicked), NULL);
  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "reconpauseCheck")), "clicked",
		      G_CALLBACK (recordClicked), NULL);
  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "isplitCheck")), "clicked",
		      G_CALLBACK (isplitClicked), NULL);
  gtk_signal_connect (GTK_OBJECT
		      (glade_xml_get_widget
		       (widgets.configXml, "vsplitCheck")), "clicked",
		      G_CALLBACK (vsplitClicked), NULL);

  config_to_gui (config);

  gtk_widget_show_all (widgets.configBox);
}


static void
dvb_configure_ok (GtkWidget * w, gpointer data)
{
  config_from_gui (config);
  config_to_db (config);

  log_set_level (hlog, config->log_level);

  gtk_widget_destroy (GTK_WIDGET (widgets.configBox));
}


static void
channelLogosClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
#if AUD_PLUGIN_API >= 16
  b =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "channelLogosCheck")));
#else
  b = FALSE;
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "channelLogosCheck"), FALSE);

#endif
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "channelLogosLabel"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "channelLogosChooser"), b);
}


static void
logToFileClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  b =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "logToFileCheck")));
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "logFileEntry"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "logAppendCheck"), b);
}


static void
recordClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  b =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "reconplayCheck"))) ||
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "reconpauseCheck")));
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "fnameEntry"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "fnameLabel"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "appendCheck"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "overwriteCheck"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "splitFrame"), b);
}


static void
isplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  if ((b =
       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				     (glade_xml_get_widget
				      (widgets.configXml, "isplitCheck")))))
    {
      // Volume and interval splitting are mutually exclusive
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (glade_xml_get_widget
				     (widgets.configXml, "vsplitCheck")),
				    FALSE);
    }
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "isplitSpin"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "isplitLabel"), b);
}


static void
vsplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  if ((b =
       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				     (glade_xml_get_widget
				      (widgets.configXml, "vsplitCheck")))))
    {
      // Volume and interval splitting are mutually exclusive
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (glade_xml_get_widget
				     (widgets.configXml, "isplitCheck")),
				    FALSE);
    }
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplitvolSpin"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplit1Label"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplit2Label"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplitdurSpin"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplit3Label"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplit4Label"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplitminlenSpin"), b);
  gtk_widget_set_sensitive (glade_xml_get_widget
			    (widgets.configXml, "vsplit5Label"), b);
}


static void
config_to_gui (const cfgstruct * config)
{
  // DVB card
  gtk_spin_button_set_value (GTK_SPIN_BUTTON
			     (glade_xml_get_widget
			      (widgets.configXml, "devnoSpin")),
			     config->devno);

  // Channel logos
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "channelLogosCheck")),
				config->logos_use);
  gtk_file_chooser_set_filename (GTK_FILE_CHOOSER
		      (glade_xml_get_widget
		       (widgets.configXml, "channelLogosChooser")),
		      config->logos_dir);
  

  // Logging
  gtk_combo_box_set_active (GTK_COMBO_BOX
			    (glade_xml_get_widget
			     (widgets.configXml, "logLevelCombo")),
			    config->log_level);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "logToFileCheck")),
				config->log_tofile);
  gtk_entry_set_text (GTK_ENTRY
		      (glade_xml_get_widget
		       (widgets.configXml, "logFileEntry")),
		      config->log_filename);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "logAppendCheck")),
				config->log_append);

  // Recording
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "reconpauseCheck")),
				config->rec_onpause);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "reconplayCheck")),
				config->rec_onplay);
  gtk_entry_set_text_safe (GTK_ENTRY
			   (glade_xml_get_widget
			    (widgets.configXml, "fnameEntry")),
			   config->rec_fname);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "appendCheck")),
				config->rec_append);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "overwriteCheck")),
				config->rec_overwrite);

  // Splitting
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "isplitCheck")),
				config->isplit);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON
			     (glade_xml_get_widget
			      (widgets.configXml, "isplitSpin")),
			     config->isplit_ival);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "vsplitCheck")),
				config->vsplit);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON
			     (glade_xml_get_widget
			      (widgets.configXml, "vsplitvolSpin")),
			     config->vsplit_vol);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON
			     (glade_xml_get_widget
			      (widgets.configXml, "vsplitdurSpin")),
			     config->vsplit_dur);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON
			     (glade_xml_get_widget
			      (widgets.configXml, "vsplitminlenSpin")),
			     config->vsplit_minlen);

  // Information retrieval
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "dvbCheck")),
				config->info_dvbstat);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "rtCheck")),
				config->info_rt);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "epgCheck")),
				config->info_epg);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				(glade_xml_get_widget
				 (widgets.configXml, "madCheck")),
				config->info_mmusic);

  // Make dialog elements (in)active
  channelLogosClicked (NULL, NULL);
  logToFileClicked (NULL, NULL);
  isplitClicked (NULL, NULL);
  vsplitClicked (NULL, NULL);
  recordClicked (NULL, NULL);
}


static void
config_from_gui (cfgstruct * config)
{
  // DVB card
  config->devno =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON
			       (glade_xml_get_widget
				(widgets.configXml, "devnoSpin")));

  // Channel logos
  config->logos_use =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "channelLogosCheck")));
  if (config->logos_dir != NULL)
    g_free (config->logos_dir);
  config->logos_dir =
    g_strdup (gtk_file_chooser_get_filename
	      (GTK_FILE_CHOOSER
	       (glade_xml_get_widget (widgets.configXml, "channelLogosChooser"))));
  

  // Logging
  config->log_level =
    gtk_combo_box_get_active (GTK_COMBO_BOX
			      (glade_xml_get_widget
			       (widgets.configXml, "logLevelCombo")));
  config->log_tofile =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "logToFileCheck")));
  if (config->log_filename != NULL)
    g_free (config->log_filename);
  config->log_filename =
    g_strdup (gtk_entry_get_text
	      (GTK_ENTRY
	       (glade_xml_get_widget (widgets.configXml, "logFileEntry"))));
  config->log_append =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "logAppendCheck")));

  // Recording
  config->rec_onplay =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "reconplayCheck")));
  config->rec_onpause =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "reconpauseCheck")));
  if (config->rec_fname != NULL)
    g_free (config->rec_fname);
  config->rec_fname =
    g_strdup (gtk_entry_get_text
	      (GTK_ENTRY
	       (glade_xml_get_widget (widgets.configXml, "fnameEntry"))));
  config->rec_append =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "appendCheck")));
  config->rec_overwrite =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "overwriteCheck")));

  // Splitting
  config->isplit =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "isplitCheck")));
  config->isplit_ival =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON
			       (glade_xml_get_widget
				(widgets.configXml, "isplitSpin")));
  config->vsplit =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "vsplitCheck")));
  config->vsplit_vol =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON
			       (glade_xml_get_widget
				(widgets.configXml, "vsplitvolSpin")));
  config->vsplit_dur =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON
			       (glade_xml_get_widget
				(widgets.configXml, "vsplitdurSpin")));
  config->vsplit_minlen =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON
			       (glade_xml_get_widget
				(widgets.configXml, "vsplitminlenSpin")));

  // Information retrieval
  config->info_dvbstat =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "dvbCheck")));
  config->info_rt =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "rtCheck")));
  config->info_epg =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "epgCheck")));
  config->info_mmusic =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
				  (glade_xml_get_widget
				   (widgets.configXml, "madCheck")));
}


void
infobox_show (const statstruct * station, const rtstruct * rt,
	      const epgstruct * epg, const mmstruct * mmusic)
{
  if (widgets.infoBox)
    {
      gtk_window_present (GTK_WINDOW (widgets.infoBox));
      return;
    }

  // Create info box
  widgets.infoXml =
    glade_xml_new_from_buffer (glwidgets, strlen (glwidgets), "fileinfo",
			       NULL);
  glade_xml_signal_autoconnect (widgets.infoXml);
  widgets.infoBox = glade_xml_get_widget (widgets.infoXml, "fileinfo");

  // Register signal handlers
  g_signal_connect (G_OBJECT (widgets.infoBox), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &widgets.infoBox);
  g_signal_connect_swapped (G_OBJECT
			    (glade_xml_get_widget
			     (widgets.infoXml, "closeButton")), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    GTK_OBJECT (widgets.infoBox));

  // Fill in stream information
  infobox_update_service (station);
  infobox_update_radiotext (rt);
  infobox_update_epg (epg);
  infobox_update_mmusic (mmusic);

  gtk_widget_show_all (widgets.infoBox);
}


void
infobox_hide (void)
{
  if (widgets.infoBox == NULL)
    return;

  gtk_widget_destroy (widgets.infoBox);
  widgets.infoBox = NULL;
  widgets.infoXml = NULL;
}


void
infobox_redraw (void)
{
  if (widgets.infoBox == NULL)
    return;

  gtk_widget_queue_draw (widgets.infoBox);
}


gboolean
infobox_is_visible (void)
{
  return (widgets.infoBox != NULL);
}


void
infobox_update_service (const statstruct * st)
{
  GtkWidget *provEntry, *statEntry;
  if (widgets.infoBox == NULL)
    return;

  provEntry = glade_xml_get_widget (widgets.infoXml, "providerEntry");
  statEntry = glade_xml_get_widget (widgets.infoXml, "stationEntry");
  if (st != NULL)
    {
      gtk_entry_set_text_safe (GTK_ENTRY (provEntry), st->prov_name);
      gtk_entry_set_text_safe (GTK_ENTRY (statEntry), st->svc_name);
    }
  else
    {
      gtk_entry_set_text_safe (GTK_ENTRY (provEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (statEntry), "");
    }
}


void
infobox_update_radiotext (const rtstruct * rt)
{
  gchar *events = NULL;
  GtkTextBuffer *rtevTextBuffer;
  GtkWidget *rtptitleEntry, *rtpartistEntry, *rtpptyEntry, *rtevTextView;
  if (widgets.infoBox == NULL)
    return;

  rtptitleEntry = glade_xml_get_widget (widgets.infoXml, "rttitleEntry");
  rtpartistEntry = glade_xml_get_widget (widgets.infoXml, "rtartistEntry");
  rtpptyEntry = glade_xml_get_widget (widgets.infoXml, "rtptyEntry");
  rtevTextView = glade_xml_get_widget (widgets.infoXml, "rtevTextView");
  rtevTextBuffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (rtevTextView));
  if (rt != NULL)
    {
      gtk_entry_set_text_safe (GTK_ENTRY (rtptitleEntry), rt->title);
      gtk_entry_set_text_safe (GTK_ENTRY (rtpartistEntry), rt->artist);
      gtk_entry_set_text_safe (GTK_ENTRY (rtpptyEntry), rt->pty);
      events = radiotext_events_to_text (rt);
      if (events != NULL)
	{
	  gtk_text_buffer_set_text (rtevTextBuffer, events, -1);
	  g_free (events);
	}
      else
	gtk_text_buffer_set_text (rtevTextBuffer, "", -1);
    }
  else
    {
      gtk_entry_set_text_safe (GTK_ENTRY (rtptitleEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (rtpartistEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (rtpptyEntry), "");
      gtk_text_buffer_set_text (rtevTextBuffer, "", -1);
    }
}


void
infobox_update_epg (const epgstruct * epg)
{
  GtkTextBuffer *epgevddescTextBuffer;
  GtkWidget *epglangEntry, *epgatypeEntry,
    *epgevnameEntry, *epgevdescEntry, *epgevddescTextView;
  if (widgets.infoBox == NULL)
    return;

  epglangEntry = glade_xml_get_widget (widgets.infoXml, "epglangEntry");
  epgatypeEntry = glade_xml_get_widget (widgets.infoXml, "epgatypeEntry");
  epgevnameEntry = glade_xml_get_widget (widgets.infoXml, "epgevnameEntry");
  epgevdescEntry = glade_xml_get_widget (widgets.infoXml, "epgevdescEntry");
  epgevddescTextView =
    glade_xml_get_widget (widgets.infoXml, "epgevddescTextView");
  epgevddescTextBuffer =
    gtk_text_view_get_buffer (GTK_TEXT_VIEW (epgevddescTextView));
  if (epg != NULL)
    {
      gchar *tmp;
      gtk_entry_set_text_safe (GTK_ENTRY (epglangEntry), epg->lang);
      gtk_entry_set_text_safe (GTK_ENTRY (epgatypeEntry), epg->stream_type);
      if (epg->pil_mday != 0)
	tmp = g_strdup_printf ("%s (Start: %02u:%02u)", epg->short_ev_name,
			       epg->pil_hour, epg->pil_min);
      else
	tmp = g_strdup (epg->short_ev_name);
      gtk_entry_set_text_safe (GTK_ENTRY (epgevnameEntry), tmp);
      g_free (tmp);
      gtk_entry_set_text_safe (GTK_ENTRY (epgevdescEntry),
			       epg->short_ev_text);
      if (epg->ext_ev_text != NULL)
	gtk_text_buffer_set_text (epgevddescTextBuffer, epg->ext_ev_text, -1);
    }
  else
    {
      gtk_entry_set_text_safe (GTK_ENTRY (epglangEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (epgatypeEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (epgevnameEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (epgevdescEntry), "");
      gtk_text_buffer_set_text (epgevddescTextBuffer, "", -1);
    }
}


void
infobox_update_mmusic (const mmstruct * mmusic)
{
  GtkWidget *mmtitleEntry, *mmartistEntry, *mmalbumEntry, *mmtrnumEntry;
  if (widgets.infoBox == NULL)
    return;

  mmtitleEntry = glade_xml_get_widget (widgets.infoXml, "mmtitleEntry");
  mmartistEntry = glade_xml_get_widget (widgets.infoXml, "mmartistEntry");
  mmalbumEntry = glade_xml_get_widget (widgets.infoXml, "mmalbumEntry");
  mmtrnumEntry = glade_xml_get_widget (widgets.infoXml, "mmtrnumEntry");
  if (mmusic != NULL)
    {
      gtk_entry_set_text_safe (GTK_ENTRY (mmtitleEntry), mmusic->title);
      gtk_entry_set_text_safe (GTK_ENTRY (mmartistEntry), mmusic->artist);
      gtk_entry_set_text_safe (GTK_ENTRY (mmalbumEntry), mmusic->album);
      gtk_entry_printf (mmtrnumEntry, "%d", mmusic->trnum);
    }
  else
    {
      gtk_entry_set_text_safe (GTK_ENTRY (mmtitleEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (mmartistEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (mmalbumEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (mmtrnumEntry), "");
    }
}

void
infobox_update_dvb (HDVB * hdvb, const dvbstatstruct * dvb,
		    const tunestruct * tune)
{
  GtkWidget *dvbtuneEntry, *dvbpidEntry, *dvbstrProgressBar,
    *dvbsnrProgressBar, *dvbuncEntry, *dvbberEntry, *dvbsignalCheckButton,
    *dvbcarrierCheckButton, *dvbviterbiCheckButton, *dvbsyncCheckButton,
    *dvblockCheckButton, *dvbtimedoutCheckButton;
  if (widgets.infoBox == NULL)
    return;

  dvbtuneEntry = glade_xml_get_widget (widgets.infoXml, "dvbtuneEntry");
  dvbpidEntry = glade_xml_get_widget (widgets.infoXml, "dvbpidEntry");
  dvbstrProgressBar =
    glade_xml_get_widget (widgets.infoXml, "dvbstrProgressBar");
  dvbsnrProgressBar =
    glade_xml_get_widget (widgets.infoXml, "dvbsnrProgressBar");
  dvbuncEntry = glade_xml_get_widget (widgets.infoXml, "dvbuncEntry");
  dvbberEntry = glade_xml_get_widget (widgets.infoXml, "dvbberEntry");
  dvbsignalCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvbsignalCheckButton");
  dvbcarrierCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvbcarrierCheckButton");
  dvbviterbiCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvbviterbiCheckButton");
  dvbsyncCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvbsyncCheckButton");
  dvblockCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvblockCheckButton");
  dvbtimedoutCheckButton =
    glade_xml_get_widget (widgets.infoXml, "dvbtimedoutCheckButton");
  if (hdvb != NULL && tune != NULL)
    {
      gchar *text;
      text = dvb_tune_to_text (hdvb, tune);
      gtk_entry_set_text_safe (GTK_ENTRY (dvbtuneEntry), text);
      if (text)
	g_free (text);
      gtk_entry_printf (dvbpidEntry, "%d (%d, %d)", tune->sid, tune->apid,
			tune->dpid);
    }
  else
    {
      gtk_entry_set_text_safe (GTK_ENTRY (dvbtuneEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (dvbpidEntry), "");
    }
  if (dvb != NULL)
    {
      gchar *text;
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dvbstrProgressBar),
				     ((double) dvb->str) / 0xffff);
      text = g_strdup_printf ("%.1lf%%", ((double) dvb->str) * 100 / 0xffff);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dvbstrProgressBar), text);
      g_free (text);
      text = g_strdup_printf ("%.1lf%%", ((double) dvb->snr) * 100 / 0xffff);
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dvbsnrProgressBar),
				     ((double) dvb->snr) / 0xffff);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dvbsnrProgressBar), text);
      g_free (text);
      gtk_entry_printf (dvbuncEntry, "%08x", dvb->unc);
      gtk_entry_printf (dvbberEntry, "%08x", dvb->ber);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbsignalCheckButton), dvb->signal);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbcarrierCheckButton), dvb->carrier);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbviterbiCheckButton), dvb->viterbi);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbsyncCheckButton), dvb->sync);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvblockCheckButton), dvb->lock);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbtimedoutCheckButton), dvb->timedout);
    }
  else
    {
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dvbstrProgressBar), 0);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dvbstrProgressBar), "");
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (dvbsnrProgressBar), 0);
      gtk_progress_bar_set_text (GTK_PROGRESS_BAR (dvbsnrProgressBar), "");
      gtk_entry_set_text_safe (GTK_ENTRY (dvbuncEntry), "");
      gtk_entry_set_text_safe (GTK_ENTRY (dvbberEntry), "");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbsignalCheckButton), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbcarrierCheckButton), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbviterbiCheckButton), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbsyncCheckButton), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvblockCheckButton), FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				    (dvbtimedoutCheckButton), FALSE);
    }
}
