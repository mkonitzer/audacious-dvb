/* $Id$ */
/* Methods for configuration file handling

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
#include <audacious/configdb.h>
#include <audacious/plugin.h>

#include "cfg.h"
#include "config.h"

cfgstruct *
config_init (void)
{
  cfgstruct *config;
  config = g_malloc0 (sizeof (cfgstruct));

  // Logging
  config->log_level = LOG_ERR;
  config->log_tofile = FALSE;
  config->log_append = TRUE;

  // Recording
  config->rec_onplay = FALSE;
  config->rec_onpause = FALSE;
  config->rec_fname = g_strdup ("/tmp/audacious-dvb-rec.mp2");
  config->rec_append = TRUE;
  config->rec_overwrite = TRUE;

  // Splitting
  config->isplit = FALSE;
  config->isplit_ival = 3600;
  config->vsplit = FALSE;
  config->vsplit_vol = -42.5;
  config->vsplit_dur = 360;
  config->vsplit_minlen = 15;

  // Information retrieval
  config->info_dvbstat = FALSE;
  config->info_rt = TRUE;
  config->info_epg = TRUE;
  config->info_mmusic = FALSE;

  return config;
}


gboolean
config_from_db (cfgstruct * config)
{
#ifdef HAVE_MCS_HANDLE_T
  mcs_handle_t *db;
#else
  ConfigDb *db;
#endif

  if ((db = aud_cfg_db_open ()) == NULL)
    return FALSE;

  // DVB device
  aud_cfg_db_get_int (db, "dvb", "devno", &config->devno);

  // Logging
  aud_cfg_db_get_int (db, "dvb", "log.level", (gint *) & config->log_level);
  aud_cfg_db_get_bool (db, "dvb", "log.tofile", &config->log_tofile);
  if (config->log_filename != NULL)
    {
      g_free (config->log_filename);
      config->log_filename = NULL;
    }
  aud_cfg_db_get_string (db, "dvb", "log.fname", &config->log_filename);
  if (config->log_filename == NULL)
    config->log_filename = g_strdup ("/tmp/audacious-dvb.log");
  aud_cfg_db_get_bool (db, "dvb", "log.append", &config->log_append);

  // Recording
  aud_cfg_db_get_bool (db, "dvb", "rec.onplay", &config->rec_onplay);
  aud_cfg_db_get_bool (db, "dvb", "rec.onpause", &config->rec_onpause);
  if (config->rec_fname != NULL)
    {
      g_free (config->rec_fname);
      config->rec_fname = NULL;
    }
  aud_cfg_db_get_string (db, "dvb", "rec.fname", &config->rec_fname);
  if (config->rec_fname == NULL)
    config->rec_fname = g_strdup ("/tmp/audacious-dvb-rec.mp2");
  aud_cfg_db_get_bool (db, "dvb", "rec.append", &config->rec_append);
  aud_cfg_db_get_bool (db, "dvb", "rec.overwrite", &config->rec_overwrite);

  // Splitting
  aud_cfg_db_get_bool (db, "dvb", "isplit", &config->isplit);
  aud_cfg_db_get_int (db, "dvb", "isplit.ival", &config->isplit_ival);
  aud_cfg_db_get_bool (db, "dvb", "vsplit", &config->vsplit);
  if (aud_cfg_db_get_double (db, "dvb", "vsplit.vol", &config->vsplit_vol))
    config->vsplit_vol /= 100;
  aud_cfg_db_get_int (db, "dvb", "vsplit.dur", &config->vsplit_dur);
  aud_cfg_db_get_int (db, "dvb", "vsplit.minlen", &config->vsplit_minlen);

  // Information retrieval
  aud_cfg_db_get_bool (db, "dvb", "info.dvbstat", &config->info_dvbstat);
  aud_cfg_db_get_bool (db, "dvb", "info.mmusic", &config->info_mmusic);
  aud_cfg_db_get_bool (db, "dvb", "info.epg", &config->info_epg);
  aud_cfg_db_get_bool (db, "dvb", "info.rt", &config->info_rt);

  aud_cfg_db_close (db);
  return TRUE;
}


gboolean
config_to_db (const cfgstruct * config)
{
#ifdef HAVE_MCS_HANDLE_T
  mcs_handle_t *db;
#else
  ConfigDb *db;
#endif

  if ((db = aud_cfg_db_open ()) == NULL)
    return FALSE;

  // DVB device
  aud_cfg_db_set_int (db, "dvb", "devno", config->devno);

  // Logging
  aud_cfg_db_set_bool (db, "dvb", "log.tofile", config->log_tofile);
  aud_cfg_db_set_int (db, "dvb", "log.level", (gint) config->log_level);
  aud_cfg_db_set_string (db, "dvb", "log.fname", config->log_filename);
  aud_cfg_db_set_bool (db, "dvb", "log.append", config->log_append);

  // Recording
  aud_cfg_db_set_bool (db, "dvb", "rec.onplay", config->rec_onplay);
  aud_cfg_db_set_bool (db, "dvb", "rec.onpause", config->rec_onpause);
  aud_cfg_db_set_string (db, "dvb", "rec.fname", config->rec_fname);
  aud_cfg_db_set_bool (db, "dvb", "rec.append", config->rec_append);
  aud_cfg_db_set_bool (db, "dvb", "rec.overwrite", config->rec_overwrite);

  // Splitting
  aud_cfg_db_set_bool (db, "dvb", "isplit", config->isplit);
  aud_cfg_db_set_int (db, "dvb", "isplit.ival", config->isplit_ival);
  aud_cfg_db_set_bool (db, "dvb", "vsplit", config->vsplit);
  aud_cfg_db_set_double (db, "dvb", "vsplit.vol", config->vsplit_vol * 100);
  aud_cfg_db_set_int (db, "dvb", "vsplit.dur", config->vsplit_dur);
  aud_cfg_db_set_int (db, "dvb", "vsplit.minlen", config->vsplit_minlen);

  // Information retrieval
  aud_cfg_db_set_bool (db, "dvb", "info.dvbstat", config->info_dvbstat);
  aud_cfg_db_set_bool (db, "dvb", "info.mmusic", config->info_mmusic);
  aud_cfg_db_set_bool (db, "dvb", "info.epg", config->info_epg);
  aud_cfg_db_set_bool (db, "dvb", "info.rt", config->info_rt);

  aud_cfg_db_close (db);
  return TRUE;
}


void
config_exit (cfgstruct * cfg)
{
  g_free (cfg);
}
