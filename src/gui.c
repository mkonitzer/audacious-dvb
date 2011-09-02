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

static prefWidgets *pref = NULL;

static void config_to_gui (const cfgstruct *);
static void dvb_configure_ok (GtkWidget *, gpointer);
static void channelLogosClicked (GtkWidget *, gpointer);
static void logToFileClicked (GtkWidget *, gpointer);
static void recordClicked (GtkWidget *, gpointer);
static void isplitClicked (GtkWidget *, gpointer);
static void vsplitClicked (GtkWidget *, gpointer);
static void config_from_gui (cfgstruct *);

#define GTK_BUILDER_GET_OBJECT( builder, name, type, data ) \
    data->name = type( gtk_builder_get_object( builder, #name ) )

// Information window
struct _infoboxWidgets
{
  GtkWidget *mainwin;
  GtkButton *closeButton;

  // Service information
  GtkEntry *stationEntry;
  GtkEntry *providerEntry;
  GtkImage *stationImage;

  // Radiotext
  GtkEntry *rttitleEntry;
  GtkEntry *rtartistEntry;
  GtkEntry *rtptyEntry;
  GtkTextView *rtevTextView;

  // EPG
  GtkEntry *epgevnameEntry;
  GtkEntry *epgevdescEntry;
  GtkEntry *epglangEntry;
  GtkEntry *epgatypeEntry;
  GtkTextView *epgevddescTextView;

  // MadMusic
  GtkEntry *mmtitleEntry;
  GtkEntry *mmartistEntry;
  GtkEntry *mmtrnumEntry;
  GtkEntry *mmalbumEntry;

  GtkEntry *dvbtuneEntry;
  GtkEntry *dvbpidEntry;
  GtkEntry *dvbuncEntry;
  GtkEntry *dvbberEntry;

  // DVB
  GtkProgressBar *dvbstrProgressBar;
  GtkProgressBar *dvbsnrProgressBar;
  GtkToggleButton *dvbsignalCheckButton;
  GtkToggleButton *dvbcarrierCheckButton;
  GtkToggleButton *dvbviterbiCheckButton;
  GtkToggleButton *dvbsyncCheckButton;
  GtkToggleButton *dvblockCheckButton;
  GtkToggleButton *dvbtimedoutCheckButton;
};

// Preferences window
struct _prefWidgets
{
  GtkWidget *mainwin;
  GtkButton *cancelButton;
  GtkButton *okButton;

  // DVB card
  GtkSpinButton *devnoSpin;

  // Channel logos
  GtkToggleButton *channelLogosCheck;
  GtkWidget *channelLogosLabel;
  GtkFileChooser *channelLogosChooser;

  // Logging
  GtkComboBox *logLevelCombo;
  GtkToggleButton *logToFileCheck;
  GtkFileChooser *logFileChooser;
  GtkToggleButton *logAppendCheck;

  // Recording
  GtkToggleButton *reconplayCheck;
  GtkToggleButton *reconpauseCheck;
  GtkFileChooser *fnameChooser;
  GtkWidget *fnameLabel;
  GtkToggleButton *appendCheck;
  GtkToggleButton *overwriteCheck;
  GtkWidget *splitFrame;

  // Splitting
  GtkToggleButton *isplitCheck;
  GtkSpinButton *isplitSpin;
  GtkWidget *isplitLabel;
  GtkToggleButton *vsplitCheck;
  GtkSpinButton *vsplitvolSpin;
  GtkWidget *vsplit1Label;
  GtkWidget *vsplit2Label;
  GtkSpinButton *vsplitdurSpin;
  GtkWidget *vsplit3Label;
  GtkWidget *vsplit4Label;
  GtkSpinButton *vsplitminlenSpin;
  GtkWidget *vsplit5Label;

  // Information retrieval
  GtkToggleButton *dvbCheck;
  GtkToggleButton *rtCheck;
  GtkToggleButton *epgCheck;
  GtkToggleButton *madCheck;
};


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
  if (pref == NULL)
    pref = g_malloc0 (sizeof (prefWidgets));

  if (pref->mainwin != NULL)
    {
      gtk_window_present (GTK_WINDOW (pref->mainwin));
      return;
    }

  // Create GtkBuilder interface
  GError* error = NULL;
  GtkBuilder *builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, glwidgets, strlen (glwidgets), &error))
    {
      g_warning ("Couldn't load builder file: %s", error->message);
      g_error_free (error);
    }

  pref->mainwin = GTK_WIDGET (gtk_builder_get_object (builder, "config"));
#define GET_OBJECT( name, type ) GTK_BUILDER_GET_OBJECT( builder, name, type, pref )
  GET_OBJECT( cancelButton, GTK_BUTTON );
  GET_OBJECT( okButton, GTK_BUTTON );
  
  // DVB card
  GET_OBJECT( devnoSpin, GTK_SPIN_BUTTON );

  // Channel logos
  GET_OBJECT( channelLogosCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( channelLogosLabel, GTK_WIDGET );
  GET_OBJECT( channelLogosChooser, GTK_FILE_CHOOSER );
  
  // Logging
  GET_OBJECT( logLevelCombo, GTK_COMBO_BOX );
  GET_OBJECT( logToFileCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( logFileChooser, GTK_FILE_CHOOSER );
  GET_OBJECT( logAppendCheck, GTK_TOGGLE_BUTTON );
  
  // Recording
  GET_OBJECT( reconplayCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( reconpauseCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( fnameChooser, GTK_FILE_CHOOSER );
  GET_OBJECT( fnameLabel, GTK_WIDGET );
  GET_OBJECT( appendCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( overwriteCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( splitFrame, GTK_WIDGET );

  // Splitting
  GET_OBJECT( isplitCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( isplitSpin, GTK_SPIN_BUTTON );
  GET_OBJECT( isplitLabel, GTK_WIDGET );
  GET_OBJECT( vsplitCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( vsplitvolSpin, GTK_SPIN_BUTTON );
  GET_OBJECT( vsplit1Label, GTK_WIDGET );
  GET_OBJECT( vsplit2Label, GTK_WIDGET );
  GET_OBJECT( vsplitdurSpin, GTK_SPIN_BUTTON );
  GET_OBJECT( vsplit3Label, GTK_WIDGET );
  GET_OBJECT( vsplit4Label, GTK_WIDGET );
  GET_OBJECT( vsplitminlenSpin, GTK_SPIN_BUTTON );
  GET_OBJECT( vsplit5Label, GTK_WIDGET );
  
  // Information retrieval
  GET_OBJECT( dvbCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( rtCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( epgCheck, GTK_TOGGLE_BUTTON );
  GET_OBJECT( madCheck, GTK_TOGGLE_BUTTON );
#undef GET_OBJECT

  g_object_unref (G_OBJECT (builder));

  // Register signal handlers
  g_signal_connect (G_OBJECT (pref->mainwin), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &pref->mainwin);
  g_signal_connect_swapped (G_OBJECT (pref->cancelButton), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    G_OBJECT (pref->mainwin));
  g_signal_connect (G_OBJECT (pref->okButton), "clicked",
		    G_CALLBACK (dvb_configure_ok), NULL);

  g_signal_connect (G_OBJECT (pref->channelLogosCheck), "clicked",
		      G_CALLBACK (channelLogosClicked), NULL);
  g_signal_connect (G_OBJECT (pref->logToFileCheck), "clicked",
		      G_CALLBACK (logToFileClicked), NULL);
  g_signal_connect (G_OBJECT (pref->reconplayCheck), "clicked",
		      G_CALLBACK (recordClicked), NULL);
  g_signal_connect (G_OBJECT (pref->reconpauseCheck), "clicked",
		      G_CALLBACK (recordClicked), NULL);
  g_signal_connect (G_OBJECT (pref->isplitCheck), "clicked",
		      G_CALLBACK (isplitClicked), NULL);
  g_signal_connect (G_OBJECT (pref->vsplitCheck), "clicked",
		      G_CALLBACK (vsplitClicked), NULL);

  config_to_gui (config);

  gtk_widget_show_all (pref->mainwin);
}


static void
dvb_configure_ok (GtkWidget * w, gpointer data)
{
  config_from_gui (config);
  config_to_db (config);

  log_set_level (hlog, config->log_level);

  gtk_widget_destroy (pref->mainwin);
}


static void
channelLogosClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b = gtk_toggle_button_get_active (pref->channelLogosCheck);
  gtk_widget_set_sensitive (pref->channelLogosLabel, b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->channelLogosChooser), b);
}


static void
logToFileClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b = gtk_toggle_button_get_active (pref->logToFileCheck);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->logFileChooser), b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->logAppendCheck), b);
}


static void
recordClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b = gtk_toggle_button_get_active (pref->reconplayCheck) ||
	  gtk_toggle_button_get_active (pref->reconpauseCheck);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->fnameChooser), b);
  gtk_widget_set_sensitive (pref->fnameLabel, b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->appendCheck), b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->overwriteCheck), b);
  gtk_widget_set_sensitive (pref->splitFrame, b);
}


static void
isplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b = gtk_toggle_button_get_active (pref->isplitCheck);
  if (b)
    // Volume and interval splitting are mutually exclusive
    gtk_toggle_button_set_active (pref->vsplitCheck, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->isplitSpin), b);
  gtk_widget_set_sensitive (pref->isplitLabel, b);
}


static void
vsplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b = gtk_toggle_button_get_active (pref->vsplitCheck);
  if (b)
    // Volume and interval splitting are mutually exclusive
    gtk_toggle_button_set_active (pref->isplitCheck, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->vsplitvolSpin), b);
  gtk_widget_set_sensitive (pref->vsplit1Label, b);
  gtk_widget_set_sensitive (pref->vsplit2Label, b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->vsplitdurSpin), b);
  gtk_widget_set_sensitive (pref->vsplit3Label, b);
  gtk_widget_set_sensitive (pref->vsplit4Label, b);
  gtk_widget_set_sensitive (GTK_WIDGET (pref->vsplitminlenSpin), b);
  gtk_widget_set_sensitive (pref->vsplit5Label, b);
}


static void
config_to_gui (const cfgstruct * config)
{
  g_assert (pref != NULL);

  // DVB card
  gtk_spin_button_set_value (pref->devnoSpin, config->devno);

  // Channel logos
  gtk_toggle_button_set_active (pref->channelLogosCheck, config->logos_use);
  gtk_file_chooser_set_filename (pref->channelLogosChooser, config->logos_dir);
  
  // Logging
  gtk_combo_box_set_active (pref->logLevelCombo, config->log_level);
  gtk_toggle_button_set_active (pref->logToFileCheck, config->log_tofile);
  gtk_file_chooser_set_filename (pref->logFileChooser, config->log_filename);
  gtk_toggle_button_set_active (pref->logAppendCheck, config->log_append);

  // Recording
  gtk_toggle_button_set_active (pref->reconpauseCheck, config->rec_onpause);
  gtk_toggle_button_set_active (pref->reconplayCheck, config->rec_onplay);
  gtk_file_chooser_set_filename (pref->fnameChooser, config->rec_fname);
  gtk_toggle_button_set_active (pref->appendCheck, config->rec_append);
  gtk_toggle_button_set_active (pref->overwriteCheck, config->rec_overwrite);

  // Splitting
  gtk_toggle_button_set_active (pref->isplitCheck, config->isplit);
  gtk_spin_button_set_value (pref->isplitSpin, config->isplit_ival);
  gtk_toggle_button_set_active (pref->vsplitCheck, config->vsplit);
  gtk_spin_button_set_value (pref->vsplitvolSpin, config->vsplit_vol);
  gtk_spin_button_set_value (pref->vsplitdurSpin, config->vsplit_dur);
  gtk_spin_button_set_value (pref->vsplitminlenSpin, config->vsplit_minlen);

  // Information retrieval
  gtk_toggle_button_set_active (pref->dvbCheck, config->info_dvbstat);
  gtk_toggle_button_set_active (pref->rtCheck, config->info_rt);
  gtk_toggle_button_set_active (pref->epgCheck, config->info_epg);
  gtk_toggle_button_set_active (pref->madCheck, config->info_mmusic);

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
  g_assert (pref != NULL);

  // DVB card
  config->devno = gtk_spin_button_get_value (pref->devnoSpin);

  // Channel logos
  config->logos_use = gtk_toggle_button_get_active (pref->channelLogosCheck);
  if (config->logos_dir != NULL)
    g_free (config->logos_dir);
  config->logos_dir =
    g_strdup (gtk_file_chooser_get_filename (pref->channelLogosChooser));

  // Logging
  config->log_level = gtk_combo_box_get_active (pref->logLevelCombo);
  config->log_tofile = gtk_toggle_button_get_active (pref->logToFileCheck);
  if (config->log_filename != NULL)
    g_free (config->log_filename);
  config->log_filename =
	  g_strdup (gtk_file_chooser_get_filename (pref->logFileChooser));
  config->log_append = gtk_toggle_button_get_active (pref->logAppendCheck);

  // Recording
  config->rec_onplay = gtk_toggle_button_get_active (pref->reconplayCheck);
  config->rec_onpause = gtk_toggle_button_get_active (pref->reconpauseCheck);
  if (config->rec_fname != NULL)
    g_free (config->rec_fname);
  config->rec_fname =
    g_strdup (gtk_file_chooser_get_filename (pref->fnameChooser));
  config->rec_append = gtk_toggle_button_get_active (pref->appendCheck);
  config->rec_overwrite = gtk_toggle_button_get_active (pref->overwriteCheck);

  // Splitting
  config->isplit = gtk_toggle_button_get_active (pref->isplitCheck);
  config->isplit_ival = gtk_spin_button_get_value (pref->isplitSpin);
  config->vsplit = gtk_toggle_button_get_active (pref->vsplitCheck);
  config->vsplit_vol = gtk_spin_button_get_value (pref->vsplitvolSpin);
  config->vsplit_dur = gtk_spin_button_get_value (pref->vsplitdurSpin);
  config->vsplit_minlen = gtk_spin_button_get_value (pref->vsplitminlenSpin);

  // Information retrieval
  config->info_dvbstat = gtk_toggle_button_get_active (pref->dvbCheck);
  config->info_rt = gtk_toggle_button_get_active (pref->rtCheck);
  config->info_epg = gtk_toggle_button_get_active (pref->epgCheck);
  config->info_mmusic = gtk_toggle_button_get_active (pref->madCheck);
}


void
infobox_show (infoboxWidgets * infobox, const statstruct * station,
	      const rtstruct * rt, const epgstruct * epg,
	      const mmstruct * mmusic)
{
  if (infobox == NULL)
    return;

  if (infobox->mainwin != NULL)
    {
      gtk_window_present (GTK_WINDOW (infobox->mainwin));
      return;
    }

  // Create GtkBuilder interface
  GError* error = NULL;
  GtkBuilder * builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, glwidgets, strlen (glwidgets), &error))
    {
      g_warning ("Couldn't load builder file: %s", error->message);
      g_error_free (error);
      g_free (infobox);
      return;
    }

  infobox->mainwin = GTK_WIDGET (gtk_builder_get_object (builder, "fileinfo"));
#define GET_OBJECT( name, type ) GTK_BUILDER_GET_OBJECT( builder, name, type, infobox )
  GET_OBJECT( closeButton, GTK_BUTTON );

  // Service information
  GET_OBJECT( providerEntry, GTK_ENTRY );
  GET_OBJECT( stationEntry, GTK_ENTRY );
  GET_OBJECT( stationImage, GTK_IMAGE );

  // Radiotext
  GET_OBJECT( rttitleEntry, GTK_ENTRY );
  GET_OBJECT( rtartistEntry, GTK_ENTRY );
  GET_OBJECT( rtptyEntry, GTK_ENTRY );
  GET_OBJECT( rtevTextView, GTK_TEXT_VIEW );
  
  // EPG
  GET_OBJECT( epglangEntry, GTK_ENTRY );
  GET_OBJECT( epgatypeEntry, GTK_ENTRY );
  GET_OBJECT( epgevnameEntry, GTK_ENTRY );
  GET_OBJECT( epgevdescEntry, GTK_ENTRY );
  GET_OBJECT( epgevddescTextView, GTK_TEXT_VIEW );

  // MadMusic
  GET_OBJECT( mmtitleEntry, GTK_ENTRY );
  GET_OBJECT( mmartistEntry, GTK_ENTRY );
  GET_OBJECT( mmalbumEntry, GTK_ENTRY );
  GET_OBJECT( mmtrnumEntry, GTK_ENTRY );

  // DVB
  GET_OBJECT( dvbtuneEntry, GTK_ENTRY );
  GET_OBJECT( dvbpidEntry, GTK_ENTRY );
  GET_OBJECT( dvbstrProgressBar, GTK_PROGRESS_BAR );
  GET_OBJECT( dvbsnrProgressBar, GTK_PROGRESS_BAR );
  GET_OBJECT( dvbuncEntry, GTK_ENTRY );
  GET_OBJECT( dvbberEntry, GTK_ENTRY );
  GET_OBJECT( dvbsignalCheckButton, GTK_TOGGLE_BUTTON );
  GET_OBJECT( dvbcarrierCheckButton, GTK_TOGGLE_BUTTON );
  GET_OBJECT( dvbviterbiCheckButton, GTK_TOGGLE_BUTTON );
  GET_OBJECT( dvbsyncCheckButton, GTK_TOGGLE_BUTTON );
  GET_OBJECT( dvblockCheckButton, GTK_TOGGLE_BUTTON );
  GET_OBJECT( dvbtimedoutCheckButton, GTK_TOGGLE_BUTTON );
#undef GET_OBJECT

  g_object_unref (G_OBJECT (builder));

  // Register signal handlers
  g_signal_connect (G_OBJECT (infobox->mainwin), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &infobox->mainwin);
  g_signal_connect_swapped (G_OBJECT (infobox->closeButton), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    G_OBJECT (infobox->mainwin));

  // Fill in stream information
  infobox_update_service (infobox, station);
  infobox_update_radiotext (infobox, rt);
  infobox_update_epg (infobox, epg);
  infobox_update_mmusic (infobox, mmusic);

  gtk_widget_show_all (infobox->mainwin);
}


void
infobox_redraw (infoboxWidgets * infobox)
{
  if (infobox == NULL)
    return;
  gtk_widget_queue_draw (infobox->mainwin);
}


gboolean
infobox_is_visible (infoboxWidgets * infobox)
{
  return (infobox != NULL && infobox->mainwin != NULL);
}


void
infobox_update_service (infoboxWidgets * infobox, const statstruct * st)
{
  if (infobox == NULL || infobox->mainwin == NULL)
    return;
  
  if (st == NULL)
    {
      gtk_entry_set_text_safe (infobox->providerEntry, "");
      gtk_entry_set_text_safe (infobox->stationEntry, "");
      gtk_image_set_from_stock (infobox->stationImage, "gtk-info",
				GTK_ICON_SIZE_BUTTON);
      return;
    }

  gtk_entry_set_text_safe (infobox->providerEntry, st->prov_name);
  gtk_entry_set_text_safe (infobox->stationEntry, st->svc_name);
  if (st->svc_imagefn != NULL)
    {
      GtkRequisition req;
      gtk_widget_size_request (GTK_WIDGET (infobox->stationImage), &req);
      GdkPixbuf * pb = gdk_pixbuf_new_from_file_at_scale (st->svc_imagefn,
				req.width, req.height, TRUE, NULL);
      gtk_image_set_from_pixbuf (infobox->stationImage, pb);
      g_object_unref (pb);
    }
  else
    gtk_image_set_from_stock (infobox->stationImage, "gtk-info",
			      GTK_ICON_SIZE_BUTTON);
}


void
infobox_update_radiotext (infoboxWidgets * infobox, const rtstruct * rt)
{
  if (infobox == NULL || infobox->mainwin == NULL)
    return;

  GtkTextBuffer *rtevTextBuffer =
	  gtk_text_view_get_buffer (infobox->rtevTextView);
  if (rt != NULL)
    {
      gtk_entry_set_text_safe (infobox->rttitleEntry, rt->title);
      gtk_entry_set_text_safe (infobox->rtartistEntry, rt->artist);
      gtk_entry_set_text_safe (infobox->rtptyEntry, rt->pty);
      gchar *events = radiotext_events_to_text (rt);
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
      gtk_entry_set_text_safe (infobox->rttitleEntry, "");
      gtk_entry_set_text_safe (infobox->rtartistEntry, "");
      gtk_entry_set_text_safe (infobox->rtptyEntry, "");
      gtk_text_buffer_set_text (rtevTextBuffer, "", -1);
    }
}


void
infobox_update_epg (infoboxWidgets * infobox, const epgstruct * epg)
{
  if (infobox == NULL || infobox->mainwin == NULL)
    return;

  GtkTextBuffer *epgevddescTextBuffer =
	  gtk_text_view_get_buffer (infobox->epgevddescTextView);
  if (epg != NULL)
    {
      gchar *tmp;
      gtk_entry_set_text_safe (infobox->epglangEntry, epg->lang);
      gtk_entry_set_text_safe (infobox->epgatypeEntry, epg->stream_type);
      if (epg->pil_mday != 0)
	tmp = g_strdup_printf ("%s (Start: %02u:%02u)", epg->short_ev_name,
			       epg->pil_hour, epg->pil_min);
      else
	tmp = g_strdup (epg->short_ev_name);
      gtk_entry_set_text_safe (infobox->epgevnameEntry, tmp);
      g_free (tmp);
      gtk_entry_set_text_safe (infobox->epgevdescEntry, epg->short_ev_text);
      if (epg->ext_ev_text != NULL)
	gtk_text_buffer_set_text (epgevddescTextBuffer, epg->ext_ev_text, -1);
    }
  else
    {
      gtk_entry_set_text_safe (infobox->epglangEntry, "");
      gtk_entry_set_text_safe (infobox->epgatypeEntry, "");
      gtk_entry_set_text_safe (infobox->epgevnameEntry, "");
      gtk_entry_set_text_safe (infobox->epgevdescEntry, "");
      gtk_text_buffer_set_text (epgevddescTextBuffer, "", -1);
    }
}


void
infobox_update_mmusic (infoboxWidgets * infobox, const mmstruct * mmusic)
{
  if (infobox == NULL || infobox->mainwin == NULL)
    return;

  if (mmusic != NULL)
    {
      gtk_entry_set_text_safe (infobox->mmtitleEntry, mmusic->title);
      gtk_entry_set_text_safe (infobox->mmartistEntry, mmusic->artist);
      gtk_entry_set_text_safe (infobox->mmalbumEntry, mmusic->album);
      gtk_entry_printf (infobox->mmtrnumEntry, "%d", mmusic->trnum);
    }
  else
    {
      gtk_entry_set_text_safe (infobox->mmtitleEntry, "");
      gtk_entry_set_text_safe (infobox->mmartistEntry, "");
      gtk_entry_set_text_safe (infobox->mmalbumEntry, "");
      gtk_entry_set_text_safe (infobox->mmtrnumEntry, "");
    }
}

void
infobox_update_dvb (infoboxWidgets * infobox, HDVB * hdvb,
		    const dvbstatstruct * dvb, const tunestruct * tune)
{
  if (infobox == NULL || infobox->mainwin == NULL)
    return;

  if (hdvb != NULL && tune != NULL)
    {
      gchar *text;
      text = dvb_tune_to_text (hdvb, tune);
      gtk_entry_set_text_safe (infobox->dvbtuneEntry, text);
      if (text)
	g_free (text);
      gtk_entry_printf (infobox->dvbpidEntry, "%d (%d, %d)", tune->sid,
			tune->apid, tune->dpid);
    }
  else
    {
      gtk_entry_set_text_safe (infobox->dvbtuneEntry, "");
      gtk_entry_set_text_safe (infobox->dvbpidEntry, "");
    }
  if (dvb != NULL)
    {
      gchar *text;
      gtk_progress_bar_set_fraction (infobox->dvbstrProgressBar,
				     ((double) dvb->str) / 0xffff);
      text = g_strdup_printf ("%.1lf%%", ((double) dvb->str) * 100 / 0xffff);
      gtk_progress_bar_set_text (infobox->dvbstrProgressBar, text);
      g_free (text);
      text = g_strdup_printf ("%.1lf%%", ((double) dvb->snr) * 100 / 0xffff);
      gtk_progress_bar_set_fraction (infobox->dvbsnrProgressBar,
				     ((double) dvb->snr) / 0xffff);
      gtk_progress_bar_set_text (infobox->dvbsnrProgressBar, text);
      g_free (text);
      gtk_entry_printf (infobox->dvbuncEntry, "%08x", dvb->unc);
      gtk_entry_printf (infobox->dvbberEntry, "%08x", dvb->ber);
      gtk_toggle_button_set_active (infobox->dvbsignalCheckButton, dvb->signal);
      gtk_toggle_button_set_active (infobox->dvbcarrierCheckButton, dvb->carrier);
      gtk_toggle_button_set_active (infobox->dvbviterbiCheckButton, dvb->viterbi);
      gtk_toggle_button_set_active (infobox->dvbsyncCheckButton, dvb->sync);
      gtk_toggle_button_set_active (infobox->dvblockCheckButton, dvb->lock);
      gtk_toggle_button_set_active (infobox->dvbtimedoutCheckButton, dvb->timedout);
    }
  else
    {
      gtk_progress_bar_set_fraction (infobox->dvbstrProgressBar, 0);
      gtk_progress_bar_set_text (infobox->dvbstrProgressBar, "");
      gtk_progress_bar_set_fraction (infobox->dvbsnrProgressBar, 0);
      gtk_progress_bar_set_text (infobox->dvbsnrProgressBar, "");
      gtk_entry_set_text_safe (infobox->dvbuncEntry, "");
      gtk_entry_set_text_safe (infobox->dvbberEntry, "");
      gtk_toggle_button_set_active (infobox->dvbsignalCheckButton, FALSE);
      gtk_toggle_button_set_active (infobox->dvbcarrierCheckButton, FALSE);
      gtk_toggle_button_set_active (infobox->dvbviterbiCheckButton, FALSE);
      gtk_toggle_button_set_active (infobox->dvbsyncCheckButton, FALSE);
      gtk_toggle_button_set_active (infobox->dvblockCheckButton, FALSE);
      gtk_toggle_button_set_active (infobox->dvbtimedoutCheckButton, FALSE);
    }
}


infoboxWidgets *
infobox_init (void)
{
  return g_malloc0 (sizeof (infoboxWidgets));
}


void
infobox_exit (infoboxWidgets * infobox)
{
  if (infobox != NULL)
    g_free (infobox);
}

