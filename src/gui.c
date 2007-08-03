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

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include <audacious/configdb.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "config.h"

#include "gui.h"
#include "log.h"


static int config_dialog_open;
static int about_dialog_open;
static int info_dialog_open;

static char si_prov[256], si_name[256];
static GtkWidget *cf_win, *cf_cb1, *cf_cb2, *cf_cb3, *cf_cb4, *cf_ef4,
  *cf_ef5;
static GtkWidget *cf_b1, *cf_b2, *cf_b3, *cf_ef1, *cf_ef2, *cf_ef3, *dialog;
static GtkWidget *cf_cb5, *cf_cb6;
static GtkWidget *if_win, *if_ef1, *if_ef2, *if_tx1;

extern int cf_rec_guard, cf_get_info, cf_get_epg;
extern int playing, cf_rec_sildur, cf_rec_isplit;
extern int cf_record, cf_rec_append, cf_rec_asplit, cf_rec_stime;
extern char cf_rec_file[MAXPATHLEN];
extern void *hlog;
extern float cf_rec_sillvl;

static char *about_title = "About DVB Input Plugin";
static char *about_text1 = "\n\
     DVB Input Plugin 0.5.0 written by Christian Motz     \n\
<douleftis@users.sourceforge.net>\n";
static char *about_text2 = "See README for details on usage.\n";


void
dvb_gui_init (void)
{
  config_dialog_open = 0;
  about_dialog_open = 0;
}


void
dvb_about (void)
{
  GtkWidget *button, *l1, *l2;

  if (about_dialog_open)
    {
      gdk_window_raise (GTK_WIDGET (dialog)->window);
      return;
    }

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), about_title);
  gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
  gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
		      GTK_SIGNAL_FUNC (dvb_about_destroy), NULL);

  l1 = gtk_label_new (about_text1);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), l1, TRUE, TRUE, 0);
  gtk_widget_show (l1);

  l2 = gtk_label_new (about_text2);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), l2, TRUE, TRUE, 0);
  gtk_widget_show (l2);

  button = gtk_button_new_with_label ("Ok");
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (dialog));
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
		      button, FALSE, FALSE, 0);
  gtk_widget_set_usize (button, 90, 22);

  gtk_widget_show (button);
  gtk_widget_show (dialog);

  about_dialog_open = 1;
}


static void
dvb_about_destroy (GtkWidget * w, gpointer d)
{
  about_dialog_open = 0;
  gtk_widget_destroy (GTK_WIDGET (w));
}


void
dvb_configure (void)
{
  char num[32];
  GtkWidget *box1, *box2, *box3;
  GtkWidget *fr, *lbl;

  if (config_dialog_open)
    {
      gdk_window_raise (GTK_WIDGET (cf_win)->window);
      return;
    }

  /* Create a dialog window with boxes. */
  cf_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (cf_win), "DVB Plugin Configuration");
  gtk_window_set_policy (GTK_WINDOW (cf_win), FALSE, FALSE, TRUE);
  gtk_signal_connect (GTK_OBJECT (cf_win), "destroy",
		      GTK_SIGNAL_FUNC (dvb_configure_destroy), NULL);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (cf_win), box1);

  box3 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (box3);
  gtk_box_pack_start (GTK_BOX (box1), box3, TRUE, TRUE, 5);

  /* Create a frame for the option check buttons. */
  fr = gtk_frame_new ("Options");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box3), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box2 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 5);

  cf_cb1 = gtk_check_button_new_with_label ("Record stream while playing");
  gtk_widget_show (cf_cb1);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb1), (int) cf_record);
  gtk_box_pack_start (GTK_BOX (box2), cf_cb1, TRUE, FALSE, 5);

  cf_cb2 = gtk_check_button_new_with_label ("Append to existing file");
  gtk_widget_show (cf_cb2);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb2),
				(int) cf_rec_append);
  gtk_box_pack_start (GTK_BOX (box2), cf_cb2, TRUE, FALSE, 5);

  box1 = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (box1);
  gtk_box_pack_start (GTK_BOX (box2), box1, TRUE, TRUE, 5);

  cf_cb3 = gtk_check_button_new_with_label ("Start a new file every");
  gtk_widget_show (cf_cb3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb3),
				(int) cf_rec_asplit);
  gtk_box_pack_start (GTK_BOX (box1), cf_cb3, FALSE, FALSE, 0);

  cf_ef2 = gtk_entry_new_with_max_length (8);
  sprintf (num, "%d", cf_rec_stime);
  gtk_entry_set_text (GTK_ENTRY (cf_ef2), (gchar *) num);
  gtk_widget_set_usize (cf_ef2, 50, 0);
  gtk_widget_show (cf_ef2);
  gtk_box_pack_start (GTK_BOX (box1), cf_ef2, FALSE, FALSE, 0);

  lbl = gtk_label_new ("seconds.");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  /* Create a frame for the Autosplit option check buttons. */
  fr = gtk_frame_new ("Automatic split");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box3), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box2 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 5);

  cf_cb4 = gtk_check_button_new_with_label ("Activate");
  gtk_widget_show (cf_cb4);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb4),
				(int) cf_rec_isplit);
  gtk_box_pack_start (GTK_BOX (box2), cf_cb4, TRUE, FALSE, 5);

  box1 = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (box1);
  gtk_box_pack_start (GTK_BOX (box2), box1, TRUE, TRUE, 5);

  lbl = gtk_label_new ("Split when below");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  cf_ef4 = gtk_entry_new_with_max_length (8);
  sprintf (num, "%.2f", cf_rec_sillvl);
  gtk_entry_set_text (GTK_ENTRY (cf_ef4), (gchar *) num);
  gtk_widget_set_usize (cf_ef4, 50, 0);
  gtk_widget_show (cf_ef4);
  gtk_box_pack_start (GTK_BOX (box1), cf_ef4, FALSE, FALSE, 0);

  lbl = gtk_label_new ("dB for at least");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  cf_ef3 = gtk_entry_new_with_max_length (4);
  sprintf (num, "%d", cf_rec_sildur);
  gtk_entry_set_text (GTK_ENTRY (cf_ef3), (gchar *) num);
  gtk_widget_set_usize (cf_ef3, 40, 0);
  gtk_widget_show (cf_ef3);
  gtk_box_pack_start (GTK_BOX (box1), cf_ef3, FALSE, FALSE, 0);

  lbl = gtk_label_new ("ms.");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  box1 = gtk_hbox_new (FALSE, 5);
  gtk_widget_show (box1);
  gtk_box_pack_start (GTK_BOX (box2), box1, TRUE, TRUE, 5);

  lbl = gtk_label_new ("Minimum file duration:");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  cf_ef5 = gtk_entry_new_with_max_length (8);
  sprintf (num, "%d", cf_rec_guard);
  gtk_entry_set_text (GTK_ENTRY (cf_ef5), (gchar *) num);
  gtk_widget_set_usize (cf_ef5, 40, 0);
  gtk_widget_show (cf_ef5);
  gtk_box_pack_start (GTK_BOX (box1), cf_ef5, FALSE, FALSE, 0);

  lbl = gtk_label_new ("seconds.");
  gtk_widget_show (lbl);
  gtk_box_pack_start (GTK_BOX (box1), lbl, FALSE, FALSE, 0);

  /* Create a frame for the Data retrieval option check button. */
  fr = gtk_frame_new ("Information retrieval");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box3), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box2 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 5);

  cf_cb5 = gtk_check_button_new_with_label ("MADMusic OpenTV Application");
  gtk_widget_show (cf_cb5);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb5),
				(int) cf_get_info);
  gtk_box_pack_start (GTK_BOX (box2), cf_cb5, TRUE, FALSE, 5);

  cf_cb6 = gtk_check_button_new_with_label ("Electronic Program Guide");
  gtk_widget_show (cf_cb6);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cf_cb6), (int) cf_get_epg);
  gtk_box_pack_start (GTK_BOX (box2), cf_cb6, TRUE, FALSE, 5);

  /* Create a frame for the filename entry field. */
  fr = gtk_frame_new ("Filename");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box3), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box2 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 5);

  cf_ef1 = gtk_entry_new ();
  gtk_widget_show (cf_ef1);
  gtk_entry_set_text (GTK_ENTRY (cf_ef1), (gchar *) cf_rec_file);
  gtk_box_pack_start (GTK_BOX (box2), cf_ef1, TRUE, FALSE, 5);

  /* Create the button. */
  box1 = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (box1), GTK_BUTTONBOX_END);
  gtk_widget_show (box1);
  gtk_box_pack_start (GTK_BOX (box3), box1, TRUE, FALSE, 5);

  cf_b1 = gtk_button_new_with_label ("Ok");
  gtk_widget_show (cf_b1);
  gtk_container_add (GTK_CONTAINER (box1), cf_b1);
  gtk_signal_connect_object (GTK_OBJECT (cf_b1), "clicked",
			     GTK_SIGNAL_FUNC (dvb_config_ok),
			     GTK_OBJECT (cf_win));

  cf_b2 = gtk_button_new_with_label ("Cancel");
  gtk_widget_show (cf_b2);
  gtk_container_add (GTK_CONTAINER (box1), cf_b2);
  gtk_signal_connect_object (GTK_OBJECT (cf_b2), "clicked",
			     GTK_SIGNAL_FUNC (gtk_widget_destroy),
			     GTK_OBJECT (cf_win));

  cf_b3 = gtk_button_new_with_label ("Apply");
  gtk_widget_show (cf_b3);
  gtk_container_add (GTK_CONTAINER (box1), cf_b3);
  gtk_signal_connect_object (GTK_OBJECT (cf_b3), "clicked",
			     GTK_SIGNAL_FUNC (dvb_config_apply),
			     GTK_OBJECT (cf_win));

  /* Show and process. */
  config_dialog_open = 1;
  gtk_widget_show (cf_win);
}


static void
dvb_configure_destroy (GtkWidget * w, gpointer d)
{
  config_dialog_open = 0;
}


static gboolean
dvb_config_ok (GtkWidget * w, GdkEvent * ev)
{
  dvb_config_apply (w, ev);
  gtk_widget_destroy (GTK_WIDGET (w));
  return TRUE;
}


static gboolean
dvb_config_apply (GtkWidget * w, GdkEvent * ev)
{
  int idb;
  ConfigDb *cfgdb;

  if (GTK_TOGGLE_BUTTON (cf_cb1)->active)
    {
      cf_record = 1;
    }
  else
    {
      if (cf_record && playing)
	{
	  dvb_close_record ();
	}
      cf_record = 0;
    }

  if (GTK_TOGGLE_BUTTON (cf_cb2)->active)
    cf_rec_append = 1;
  else
    cf_rec_append = 0;

  if (GTK_TOGGLE_BUTTON (cf_cb3)->active)
    cf_rec_asplit = 1;
  else
    cf_rec_asplit = 0;

  if (GTK_TOGGLE_BUTTON (cf_cb4)->active)
    cf_rec_isplit = 1;
  else
    cf_rec_isplit = 0;

  if (GTK_TOGGLE_BUTTON (cf_cb5)->active)
    cf_get_info = 1;
  else
    cf_get_info = 0;

  if (GTK_TOGGLE_BUTTON (cf_cb6)->active)
    cf_get_epg = 1;
  else
    cf_get_epg = 0;

  cf_rec_stime = atoi (gtk_entry_get_text (GTK_ENTRY (cf_ef2)));
  cf_rec_sildur = atoi (gtk_entry_get_text (GTK_ENTRY (cf_ef3)));
  sscanf (gtk_entry_get_text (GTK_ENTRY (cf_ef4)), "%f", &cf_rec_sillvl);
  cf_rec_guard = atoi (gtk_entry_get_text (GTK_ENTRY (cf_ef5)));

  /* Trust me, you DON'T want to know ... */
  if (cf_rec_sillvl < 0)
    cf_rec_sillvl -= .005;
  else
    cf_rec_sillvl += .005;

  idb = (int) (cf_rec_sillvl * 100);
  cf_rec_sillvl = (float) idb;
  cf_rec_sillvl /= 100;

  log_print (hlog, LOG_DEBUG, "%f dB (%d), %d ms, %d s", cf_rec_sillvl, idb,
	     cf_rec_sildur, cf_rec_guard);

  if (strcmp (cf_rec_file, gtk_entry_get_text (GTK_ENTRY (cf_ef1))) != 0)
    {
      if (cf_record && playing)
	dvb_close_record ();

      strcpy (cf_rec_file, gtk_entry_get_text (GTK_ENTRY (cf_ef1)));
    }

  if ((cfgdb = bmp_cfg_db_open ()) != NULL)
    {
      bmp_cfg_db_set_bool (cfgdb, "DVB", "Record", cf_record);
      bmp_cfg_db_set_bool (cfgdb, "DVB", "Append", cf_rec_append);
      bmp_cfg_db_set_bool (cfgdb, "DVB", "Autosplit", cf_rec_asplit);
      bmp_cfg_db_set_bool (cfgdb, "DVB", "Split", cf_rec_isplit);
      bmp_cfg_db_set_int (cfgdb, "DVB", "Interval", cf_rec_stime);
      bmp_cfg_db_set_int (cfgdb, "DVB", "Duration", cf_rec_sildur);
      bmp_cfg_db_set_int (cfgdb, "DVB", "Level", idb);
      bmp_cfg_db_set_int (cfgdb, "DVB", "Guard", cf_rec_guard);
      bmp_cfg_db_set_bool (cfgdb, "DVB", "Info", cf_get_info);
      bmp_cfg_db_set_bool (cfgdb, "DVB", "EPG", cf_get_epg);
      bmp_cfg_db_set_string (cfgdb, "DVB", "File", cf_rec_file);
      bmp_cfg_db_close (cfgdb);
    }

  return TRUE;
}


void
dvb_getinfo (char *s)
{
  GtkWidget *fr, *lbl, *tbl;
  GtkWidget *box1, *box2, *box3;

  if (info_dialog_open)
    {
      gdk_window_raise (GTK_WIDGET (if_win)->window);
      return;
    }

  if_win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (if_win), "Service Information");
  gtk_signal_connect (GTK_OBJECT (if_win), "destroy",
		      GTK_SIGNAL_FUNC (dvb_info_destroy), NULL);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (if_win), box1);

  box2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 5);

  fr = gtk_frame_new ("Service");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box2), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box3 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box3);
  gtk_box_pack_start (GTK_BOX (box1), box3, TRUE, TRUE, 5);

  tbl = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (tbl);
  gtk_box_pack_start (GTK_BOX (box3), tbl, TRUE, TRUE, 5);

  lbl = gtk_label_new ("Provider:");
  gtk_widget_show (lbl);
  gtk_table_attach (GTK_TABLE (tbl), lbl, 0, 1, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_label_set_justify (GTK_LABEL (lbl), GTK_JUSTIFY_LEFT);

  if_ef1 = gtk_entry_new ();
  gtk_widget_show (if_ef1);
  gtk_table_attach (GTK_TABLE (tbl), if_ef1, 1, 2, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_entry_set_text (GTK_ENTRY (if_ef1), si_prov);

  lbl = gtk_label_new ("Name:");
  gtk_widget_show (lbl);
  gtk_table_attach (GTK_TABLE (tbl), lbl, 0, 1, 1, 2,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_label_set_justify (GTK_LABEL (lbl), GTK_JUSTIFY_LEFT);

  if_ef2 = gtk_entry_new ();
  gtk_widget_show (if_ef2);
  gtk_table_attach (GTK_TABLE (tbl), if_ef2, 1, 2, 1, 2,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_entry_set_text (GTK_ENTRY (if_ef2), si_name);

  fr = gtk_frame_new ("Track");
  gtk_widget_show (fr);
  gtk_box_pack_start (GTK_BOX (box2), fr, TRUE, TRUE, 5);

  box1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box1);
  gtk_container_add (GTK_CONTAINER (fr), box1);

  box3 = gtk_vbox_new (FALSE, -5);
  gtk_widget_show (box3);
  gtk_box_pack_start (GTK_BOX (box1), box3, TRUE, TRUE, 5);

  tbl = gtk_table_new (2, 2, FALSE);
  gtk_widget_show (tbl);
  gtk_box_pack_start (GTK_BOX (box3), tbl, TRUE, TRUE, 5);

  lbl = gtk_label_new ("Title:");
  gtk_widget_show (lbl);
  gtk_table_attach (GTK_TABLE (tbl), lbl, 0, 1, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_label_set_justify (GTK_LABEL (lbl), GTK_JUSTIFY_LEFT);

  if_tx1 = gtk_text_view_new ();
  gtk_widget_show (if_tx1);
  gtk_table_attach (GTK_TABLE (tbl), if_tx1, 1, 2, 0, 1,
		    GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 2, 2);
  gtk_widget_set_usize (if_tx1, 300, 24);

  info_dialog_open = 1;
  gtk_widget_show (if_win);
}


static void
dvb_info_destroy (GtkWidget * w, gpointer d)
{
  info_dialog_open = 0;
}


void
dvb_info_update (char *prov, char *name)
{
  strcpy (si_prov, prov);
  strcpy (si_name, name);

  if (info_dialog_open)
    {
      gtk_entry_set_text (GTK_ENTRY (if_ef1), si_prov);
      gtk_entry_set_text (GTK_ENTRY (if_ef2), si_name);
    }
}
