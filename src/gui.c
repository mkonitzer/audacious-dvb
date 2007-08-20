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

#include <audacious/configdb.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "gui.h"
#include "log.h"
#include "cfg.h"
#include "epg.h"
#include "rtxt.h"
#include "mmusic.h"
#include "config.h"

extern gpointer hlog;
extern cfgstruct *config;

static Widgets widgets = { 0 };


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
      gdk_window_raise (GTK_WIDGET (widgets.configBox)->window);
      return;
    }

  gint i;
  GtkWidget *hbox, *vbox;

  GtkWidget *configBox = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  widgets.configBox = configBox;
  g_signal_connect (G_OBJECT (configBox), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &widgets.configBox);
  gtk_window_set_title (GTK_WINDOW (configBox), "DVB Plugin Configuration");
  gtk_container_border_width (GTK_CONTAINER (configBox), 10);

  GtkWidget *notebook = gtk_notebook_new ();
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (configBox), vbox);

  // Buttons
  hbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing (GTK_BUTTON_BOX (hbox), 5);
  GtkWidget *cancelButton = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  g_signal_connect_swapped (G_OBJECT (cancelButton), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    GTK_OBJECT (widgets.configBox));
  gtk_box_pack_start (GTK_BOX (hbox), cancelButton, TRUE, TRUE, 0);
  GtkWidget *okButton = gtk_button_new_from_stock (GTK_STOCK_OK);
  g_signal_connect (G_OBJECT (okButton), "clicked",
		    G_CALLBACK (dvb_configure_ok), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), okButton, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  // General settings
  GtkWidget *generalFrame = gtk_frame_new ("General Settings");
  gtk_container_border_width (GTK_CONTAINER (generalFrame), 5);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (generalFrame), vbox);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *devLabel = gtk_label_new ("DVB device:");
  gtk_box_pack_start (GTK_BOX (hbox), devLabel, FALSE, FALSE, 0);
  GtkWidget *devnoSpin = gtk_spin_button_new_with_range (0, 9, 1);
  widgets.devnoSpin = devnoSpin;
  gtk_box_pack_start (GTK_BOX (hbox), devnoSpin, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *logLabel = gtk_label_new ("Log level:");
  gtk_box_pack_start (GTK_BOX (hbox), logLabel, FALSE, FALSE, 0);
  GtkWidget *loggingCombo = gtk_combo_box_new_text ();
  widgets.loggingCombo = loggingCombo;
  const gchar *log_name[] = { "No logging (0)", "Alert (1)", "Critical (2)",
    "Error (3)", "Warning (4)", "Notice (5)", "Info (6)", "Debug (7)"
  };
  for (i = 0; i < 8; i++)
    gtk_combo_box_append_text (GTK_COMBO_BOX (loggingCombo), log_name[i]);
  gtk_box_pack_start (GTK_BOX (hbox), loggingCombo, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), generalFrame,
			    gtk_label_new ("General"));

  // Recording settings
  GtkWidget *recordFrame = gtk_frame_new ("Recording Settings");
  gtk_container_border_width (GTK_CONTAINER (recordFrame), 5);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (recordFrame), vbox);

  GtkWidget *recordCheck =
    gtk_check_button_new_with_label ("Record stream while playing");
  widgets.recordCheck = recordCheck;
  gtk_signal_connect (GTK_OBJECT (recordCheck), "clicked",
		      G_CALLBACK (recordClicked), NULL);
  gtk_box_pack_start (GTK_BOX (vbox), recordCheck, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *fnameLabel = gtk_label_new ("Filename:");
  widgets.fnameLabel = fnameLabel;
  gtk_box_pack_start (GTK_BOX (hbox), fnameLabel, FALSE, FALSE, 0);
  GtkWidget *fnameEntry = gtk_entry_new_with_max_length (50);
  widgets.fnameEntry = fnameEntry;
  gtk_box_pack_start (GTK_BOX (hbox), fnameEntry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  GtkWidget *appendCheck =
    gtk_check_button_new_with_label ("Append to existing file");
  widgets.appendCheck = appendCheck;
  gtk_box_pack_start (GTK_BOX (vbox), appendCheck, FALSE, FALSE, 0);

  // Splitting
  GtkWidget *splitFrame = gtk_frame_new ("Splitting");
  widgets.splitFrame = splitFrame;
  gtk_container_border_width (GTK_CONTAINER (splitFrame), 5);
  gtk_container_add (GTK_CONTAINER (vbox), splitFrame);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (splitFrame), vbox);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *isplitCheck =
    gtk_check_button_new_with_label ("Split into intervals of:");
  widgets.isplitCheck = isplitCheck;
  gtk_signal_connect (GTK_OBJECT (isplitCheck), "clicked",
		      G_CALLBACK (isplitClicked), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), isplitCheck, FALSE, FALSE, 0);
  GtkWidget *isplitSpin = gtk_spin_button_new_with_range (0, 9999, 1);
  widgets.isplitSpin = isplitSpin;
  gtk_box_pack_start (GTK_BOX (hbox), isplitSpin, FALSE, FALSE, 0);
  GtkWidget *isplitLabel = gtk_label_new ("s.");
  gtk_box_pack_start (GTK_BOX (hbox), isplitLabel, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *vsplitCheck =
    gtk_check_button_new_with_label ("Split when volume is below:");
  widgets.vsplitCheck = vsplitCheck;
  gtk_signal_connect (GTK_OBJECT (vsplitCheck), "clicked",
		      G_CALLBACK (vsplitClicked), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), vsplitCheck, FALSE, FALSE, 0);
  GtkWidget *vsplitvolSpin = gtk_spin_button_new_with_range (-999, 99, 0.5);
  widgets.vsplitvolSpin = vsplitvolSpin;
  gtk_box_pack_start (GTK_BOX (hbox), vsplitvolSpin, FALSE, FALSE, 0);
  GtkWidget *vsplit1Label = gtk_label_new ("dB");
  widgets.vsplit1Label = vsplit1Label;
  gtk_box_pack_start (GTK_BOX (hbox), vsplit1Label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *vsplit2Label = gtk_label_new ("for at least:");
  widgets.vsplit2Label = vsplit2Label;
  GtkWidget *vsplit2Align = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (vsplit2Align), 0, 0, 20, 0);
  gtk_container_add (GTK_CONTAINER (vsplit2Align), vsplit2Label);
  gtk_box_pack_start (GTK_BOX (hbox), vsplit2Align, FALSE, FALSE, 0);
  GtkWidget *vsplitdurSpin = gtk_spin_button_new_with_range (0, 999, 1);
  widgets.vsplitdurSpin = vsplitdurSpin;
  gtk_box_pack_start (GTK_BOX (hbox), vsplitdurSpin, FALSE, FALSE, 0);
  GtkWidget *vsplit3Label = gtk_label_new ("ms");
  widgets.vsplit3Label = vsplit3Label;
  gtk_box_pack_start (GTK_BOX (hbox), vsplit3Label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 5);
  GtkWidget *vsplit4Label = gtk_label_new ("with a minimum file length of:");
  widgets.vsplit4Label = vsplit4Label;
  GtkWidget *vsplit4Align = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (vsplit4Align), 0, 0, 20, 0);
  gtk_container_add (GTK_CONTAINER (vsplit4Align), vsplit4Label);
  gtk_box_pack_start (GTK_BOX (hbox), vsplit4Align, FALSE, FALSE, 0);
  GtkWidget *vsplitminlenSpin = gtk_spin_button_new_with_range (0, 999, 1);
  widgets.vsplitminlenSpin = vsplitminlenSpin;
  gtk_box_pack_start (GTK_BOX (hbox), vsplitminlenSpin, FALSE, FALSE, 0);
  GtkWidget *vsplit5Label = gtk_label_new ("s.");
  widgets.vsplit5Label = vsplit5Label;
  gtk_box_pack_start (GTK_BOX (hbox), vsplit5Label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), recordFrame,
			    gtk_label_new ("Recording"));

  // Information retrieval
  GtkWidget *infoFrame = gtk_frame_new ("Information Retrieval");
  gtk_container_border_width (GTK_CONTAINER (infoFrame), 5);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (infoFrame), vbox);

  GtkWidget *rtCheck =
    gtk_check_button_new_with_label ("RDS-Radiotext[+]");
  widgets.rtCheck = rtCheck;
  gtk_box_pack_start (GTK_BOX (vbox), rtCheck, FALSE, FALSE, 0);

  GtkWidget *epgCheck =
    gtk_check_button_new_with_label ("Electronic Program Guide (EPG)");
  widgets.epgCheck = epgCheck;
  gtk_box_pack_start (GTK_BOX (vbox), epgCheck, FALSE, FALSE, 0);

  GtkWidget *madCheck =
    gtk_check_button_new_with_label ("MadMusic OpenTV Application");
  widgets.madCheck = madCheck;
  gtk_box_pack_start (GTK_BOX (vbox), madCheck, FALSE, FALSE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), infoFrame,
			    gtk_label_new ("Information"));

  config_to_gui (config);

  gtk_widget_show_all (configBox);
}


static void
dvb_configure_ok (GtkWidget * w, gpointer data)
{
  config_from_gui (config);
  config_to_db (config);

  log_set_level (hlog, config->loglvl);

  gtk_widget_destroy (GTK_WIDGET (widgets.configBox));
}


static void
recordClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  b = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.recordCheck));
  gtk_widget_set_sensitive (widgets.fnameLabel, b);
  gtk_widget_set_sensitive (widgets.fnameEntry, b);
  gtk_widget_set_sensitive (widgets.appendCheck, b);
  gtk_widget_set_sensitive (widgets.splitFrame, b);
}


static void
isplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  b = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.isplitCheck));
  gtk_widget_set_sensitive (widgets.isplitSpin, b);
}


static void
vsplitClicked (GtkWidget * w, gpointer user_data)
{
  gboolean b;
  b = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.vsplitCheck));
  gtk_widget_set_sensitive (widgets.vsplitvolSpin, b);
  gtk_widget_set_sensitive (widgets.vsplit1Label, b);
  gtk_widget_set_sensitive (widgets.vsplit2Label, b);
  gtk_widget_set_sensitive (widgets.vsplitdurSpin, b);
  gtk_widget_set_sensitive (widgets.vsplit3Label, b);
  gtk_widget_set_sensitive (widgets.vsplit4Label, b);
  gtk_widget_set_sensitive (widgets.vsplitminlenSpin, b);
  gtk_widget_set_sensitive (widgets.vsplit5Label, b);
}

static void
config_to_gui (cfgstruct * config)
{
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets.devnoSpin),
			     config->devno);
  gtk_combo_box_set_active (GTK_COMBO_BOX (widgets.loggingCombo),
			    config->loglvl);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.recordCheck),
				config->rec);
  gtk_entry_set_text (GTK_ENTRY (widgets.fnameEntry), config->rec_fname);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.appendCheck),
				config->rec_append);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.isplitCheck),
				config->isplit);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets.isplitSpin),
			     config->isplit_ival);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.vsplitCheck),
				config->vsplit);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets.vsplitvolSpin),
			     config->vsplit_vol);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets.vsplitdurSpin),
			     config->vsplit_dur);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widgets.vsplitminlenSpin),
			     config->vsplit_minlen);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.rtCheck),
				config->info_rt);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.epgCheck),
				config->info_epg);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widgets.madCheck),
				config->info_mmusic);

  isplitClicked (NULL, NULL);
  vsplitClicked (NULL, NULL);
  recordClicked (NULL, NULL);
}

static void
config_from_gui (cfgstruct * config)
{
  config->devno =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON (widgets.devnoSpin));
  config->loglvl =
    gtk_combo_box_get_active (GTK_COMBO_BOX (widgets.loggingCombo));

  config->rec =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.recordCheck));
  if (config->rec_fname != NULL)
    g_free (config->rec_fname);
  config->rec_fname =
    g_strdup (gtk_entry_get_text (GTK_ENTRY (widgets.fnameEntry)));
  config->rec_append =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.appendCheck));

  config->isplit =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.isplitCheck));
  config->isplit_ival =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON (widgets.isplitSpin));

  config->vsplit =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.vsplitCheck));
  config->vsplit_vol =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON (widgets.vsplitvolSpin));
  config->vsplit_dur =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON (widgets.vsplitdurSpin));
  config->vsplit_minlen =
    gtk_spin_button_get_value (GTK_SPIN_BUTTON (widgets.vsplitminlenSpin));

  config->info_rt =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.rtCheck));
  config->info_epg =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.epgCheck));
  config->info_mmusic =
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widgets.madCheck));
}

void
dvb_infobox (statstruct *station, rtstruct *rt, epgstruct *epg, mmstruct *mmusic)
{
  GtkWidget *hbox, *vbox, *mainvbox, *label;

  if (widgets.infoBox)
    {
      gdk_window_raise (GTK_WIDGET (widgets.infoBox)->window);
      return;
    }

  GtkWidget *infoBox = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  widgets.infoBox = infoBox;
  g_signal_connect (G_OBJECT (infoBox), "destroy",
		    G_CALLBACK (gtk_widget_destroyed), &widgets.infoBox);
  gtk_window_set_title (GTK_WINDOW (infoBox), "DVB Stream Information");
  
  mainvbox = gtk_vbox_new (FALSE, 10);
  gtk_container_add (GTK_CONTAINER (infoBox), mainvbox);

  // Service information
  GtkWidget *serviceFrame = gtk_frame_new ("Service Information");
  gtk_box_pack_start (GTK_BOX (mainvbox), serviceFrame, TRUE, TRUE, 0);
  gtk_container_border_width (GTK_CONTAINER (serviceFrame), 5);
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_border_width (GTK_CONTAINER (vbox), 5);
  gtk_container_add (GTK_CONTAINER (serviceFrame), vbox);
    {
      GtkWidget *table;
      table = gtk_table_new (2, 2, FALSE);
      gtk_box_pack_start (GTK_BOX (vbox), table, TRUE, TRUE, 0);
      // Station
      label = gtk_label_new ("Station:");
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 2, 2);
      GtkWidget *statEntry = gtk_entry_new ();
      widgets.statEntry = statEntry;
      gtk_editable_set_editable (GTK_EDITABLE (statEntry), FALSE);
      gtk_widget_show (statEntry);
      gtk_table_attach_defaults (GTK_TABLE (table), statEntry, 1, 2, 0, 1);
      // Provider
      label = gtk_label_new ("Provider:");
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_EXPAND, 2, 2);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      GtkWidget *provEntry = gtk_entry_new ();
      widgets.provEntry = provEntry;
      gtk_editable_set_editable (GTK_EDITABLE (provEntry), FALSE);
      gtk_widget_show (provEntry);
      gtk_table_attach_defaults (GTK_TABLE (table), provEntry, 1, 2, 1, 2);
    }
  
  // Notebook (Radiotext/EPG/MadMusic/DVB)
  GtkWidget *notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (mainvbox), notebook, TRUE, TRUE, 0);
    {
      // Radiotext information
      GtkWidget *rtFrame = gtk_frame_new ("Radiotext information");
      gtk_container_border_width (GTK_CONTAINER (rtFrame), 5);
      vbox = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (rtFrame), vbox);
      
      GtkWidget *table;
      table = gtk_table_new (4, 2, FALSE);
      gtk_container_add (GTK_CONTAINER (vbox), table);
      // Title
      label = gtk_label_new ("Title:");
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 2, 2);
      GtkWidget *rtptitleEntry = gtk_entry_new ();
      widgets.rtptitleEntry = rtptitleEntry;
      gtk_editable_set_editable (GTK_EDITABLE (rtptitleEntry), FALSE);
      gtk_widget_show (rtptitleEntry);

      gtk_table_attach_defaults (GTK_TABLE (table), rtptitleEntry, 1, 2, 0, 1);
      // Artist
      label = gtk_label_new ("Artist:");
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_EXPAND, 2, 2);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      GtkWidget *rtpartistEntry = gtk_entry_new ();
      widgets.rtpartistEntry = rtpartistEntry;
      gtk_editable_set_editable (GTK_EDITABLE (rtpartistEntry), FALSE);
      gtk_widget_show (rtpartistEntry);
      gtk_table_attach_defaults (GTK_TABLE (table), rtpartistEntry, 1, 2, 1, 2);
      // Program Type
      label = gtk_label_new ("Program type:");
      gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_EXPAND, 2, 2);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      GtkWidget *rtpptyEntry = gtk_entry_new ();
      widgets.rtpptyEntry = rtpptyEntry;
      gtk_editable_set_editable (GTK_EDITABLE (rtpptyEntry), FALSE);
      gtk_widget_show (rtpptyEntry);
      gtk_table_attach_defaults (GTK_TABLE (table), rtpptyEntry, 1, 2, 2, 3);
      // Radiotext events
      GtkWidget *scrtextview = NULL;
      scrtextview = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrtextview),
				      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrtextview), 
					   GTK_SHADOW_ETCHED_IN);
      GtkWidget *textview;
      textview = gtk_text_view_new ();
      /*gtk_table_attach (GTK_TABLE (table), scrtextview, 0, 2, 3, 4,
			GTK_FILL | GTK_SHRINK, GTK_FILL | GTK_SHRINK, 2, 2);*/
      gtk_table_attach_defaults (GTK_TABLE (table), scrtextview, 0, 2, 3, 4);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD_CHAR);
      gtk_container_add (GTK_CONTAINER (scrtextview), textview);
      
      GtkTextBuffer *textbuf;
      textbuf = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
      gtk_text_buffer_set_text (textbuf, "", -1);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), rtFrame,
				gtk_label_new ("Radiotext"));

      // EPG information
      GtkWidget *epgFrame = gtk_frame_new ("EPG information");
      gtk_container_border_width (GTK_CONTAINER (epgFrame), 5);
      vbox = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (epgFrame), vbox);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), epgFrame,
				gtk_label_new ("EPG"));

      // MadMusic information
      GtkWidget *mmFrame = gtk_frame_new ("MadMusic information");
      gtk_container_border_width (GTK_CONTAINER (mmFrame), 5);
      vbox = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (mmFrame), vbox);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), mmFrame,
				gtk_label_new ("MadMusic"));

      // DVB information
      GtkWidget *dvbFrame = gtk_frame_new ("DVB information");
      gtk_container_border_width (GTK_CONTAINER (dvbFrame), 5);
      vbox = gtk_vbox_new (FALSE, 10);
      gtk_container_border_width (GTK_CONTAINER (vbox), 5);
      gtk_container_add (GTK_CONTAINER (dvbFrame), vbox);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), dvbFrame,
				gtk_label_new ("DVB"));

    }

  // Fill in stream information
  infobox_update_service (station);
  infobox_update_radiotext (rt);
  infobox_update_epg (epg);
  infobox_update_mmusic (mmusic);
  
  gtk_widget_show_all (infoBox);
}


void
infobox_update_service (statstruct *st)
{
  if (widgets.infoBox)
    {
      if (st != NULL)
	{
	  gtk_entry_set_text (GTK_ENTRY (widgets.provEntry), st->prov_name);
	  gtk_entry_set_text (GTK_ENTRY (widgets.statEntry), st->svc_name);
	}
      else
	{
	  gtk_entry_set_text (GTK_ENTRY (widgets.provEntry), "");
	  gtk_entry_set_text (GTK_ENTRY (widgets.statEntry), "");
	}   
    }
}


void
infobox_update_radiotext (rtstruct *rt)
{
  if (widgets.infoBox)
    {
      if (rt != NULL)
	{
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtptitleEntry), rt->title);
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtpartistEntry), rt->artist);
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtpptyEntry), rt->pty);
	}
      else
	{
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtptitleEntry), "");
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtpartistEntry), "");
	  gtk_entry_set_text (GTK_ENTRY (widgets.rtpptyEntry), "");
	}
    }
}


void
infobox_update_epg (epgstruct *epg)
{
  if (widgets.infoBox)
    {
      if (epg != NULL)
	{
	}
      else
	{
	}
    }
}


void
infobox_update_mmusic (mmstruct *mmusic)
{
  if (widgets.infoBox)
    {
      if (mmusic != NULL)
	{
	}
      else
	{
	}
    }
}
