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

#include "config.h"

#include <glib.h>
#ifdef HAVE_AUDACIOUS_CONFIGDB_H
#include <audacious/configdb.h>
#endif
#include <audacious/misc.h>
#include <audacious/plugin.h>

#include "cfg.h"

#ifdef HAVE_AUDACIOUS_CONFIGDB_H
#define cfg_get_int(sect,val,cfg,var)		(aud_cfg_db_get_int (db, sect, val, & cfg->var))
#define cfg_get_bool(sect,val,cfg,var)		(aud_cfg_db_get_bool (db, sect, val, & cfg->var))
#define cfg_get_string(sect,val,cfg,var)	(aud_cfg_db_get_string (db, sect, val, & cfg->var))
#define cfg_get_double(sect,val,cfg,var)	(aud_cfg_db_get_double (db, sect, val, & cfg->var))
#define cfg_set_int(sect,val,cfg,var)		(aud_cfg_db_set_int (db, sect, val, cfg->var))
#define cfg_set_bool(sect,val,cfg,var)		(aud_cfg_db_set_bool (db, sect, val, cfg->var))
#define cfg_set_string(sect,val,cfg,var)	(aud_cfg_db_set_string (db, sect, val, cfg->var))
#define cfg_set_double(sect,val,cfg,var)	(aud_cfg_db_set_double (db, sect, val, cfg->var))
#else
#define cfg_get_int(sect,val,cfg,var)		(cfg->var = aud_get_int (sect, val))
#define cfg_get_bool(sect,val,cfg,var)		(cfg->var = aud_get_bool (sect, val))
#define cfg_get_string(sect,val,cfg,var)	(cfg->var = aud_get_string (sect, val))
#define cfg_get_double(sect,val,cfg,var)	(cfg->var = aud_get_double (sect, val))
#define cfg_set_int(sect,val,cfg,var)		(aud_set_int (sect, val, cfg->var))
#define cfg_set_bool(sect,val,cfg,var)		(aud_set_bool (sect, val, cfg->var))
#define cfg_set_string(sect,val,cfg,var)	(aud_set_string (sect, val, cfg->var))
#define cfg_set_double(sect,val,cfg,var)	(aud_set_double (sect, val, cfg->var))
#endif

cfgstruct *
config_init (void)
{
  cfgstruct *config;
  config = g_malloc0 (sizeof (cfgstruct));

  // Channel logos
  config->logos_use = FALSE;
  
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
#ifdef HAVE_AUDACIOUS_CONFIGDB_H
  mcs_handle_t *db;
  if ((db = aud_cfg_db_open ()) == NULL)
    return FALSE;
#endif

  // DVB device
  cfg_get_int ("dvb", "devno", config, devno);

  // Channel logos
  cfg_get_bool ("dvb", "logos.use", config, logos_use);
  if (config->logos_dir != NULL)
    {
      g_free (config->logos_dir);
      config->logos_dir = NULL;
    }
  cfg_get_string ("dvb", "logos.dir", config, logos_dir);
  if (config->logos_dir == NULL)
    config->logos_use = FALSE;

  // Logging
#ifdef HAVE_AUDACIOUS_CONFIGDB_H
  aud_cfg_db_get_int (db, "dvb", "log.level", (gint *) & config->log_level);
#else
  config->log_level = aud_get_int ("dvb", "log.level");
#endif
  cfg_get_bool ("dvb", "log.tofile", config, log_tofile);
  if (config->log_filename != NULL)
    {
      g_free (config->log_filename);
      config->log_filename = NULL;
    }
  cfg_get_string ("dvb", "log.fname", config, log_filename);
  if (config->log_filename == NULL)
    config->log_filename = g_strdup ("/tmp/audacious-dvb.log");
  cfg_get_bool ("dvb", "log.append", config, log_append);

  // Recording
  cfg_get_bool ("dvb", "rec.onplay", config, rec_onplay);
  cfg_get_bool ("dvb", "rec.onpause", config, rec_onpause);
  if (config->rec_fname != NULL)
    {
      g_free (config->rec_fname);
      config->rec_fname = NULL;
    }
  cfg_get_string ("dvb", "rec.fname", config, rec_fname);
  if (config->rec_fname == NULL)
    config->rec_fname = g_strdup ("/tmp/audacious-dvb-rec.mp2");
  cfg_get_bool ("dvb", "rec.append", config, rec_append);
  cfg_get_bool ("dvb", "rec.overwrite", config, rec_overwrite);

  // Splitting
  cfg_get_bool ("dvb", "isplit", config, isplit);
  cfg_get_int ("dvb", "isplit.ival", config, isplit_ival);
  cfg_get_bool ("dvb", "vsplit", config, vsplit);
  cfg_get_double ("dvb", "vsplit.vol", config, vsplit_vol);
  config->vsplit_vol /= 100;
  cfg_get_int ("dvb", "vsplit.dur", config, vsplit_dur);
  cfg_get_int ("dvb", "vsplit.minlen", config, vsplit_minlen);

  // Information retrieval
  cfg_get_bool ("dvb", "info.dvbstat", config, info_dvbstat);
  cfg_get_bool ("dvb", "info.mmusic", config, info_mmusic);
  cfg_get_bool ("dvb", "info.epg", config, info_epg);
  cfg_get_bool ("dvb", "info.rt", config, info_rt);

#ifdef HAVE_AUDACIOUS_CONFIGDB_H
  aud_cfg_db_close (db);
#endif
  return TRUE;
}


gboolean
config_to_db (const cfgstruct * config)
{
#ifdef HAVE_AUDACIOUS_CONFIGDB_H
  mcs_handle_t *db;
  if ((db = aud_cfg_db_open ()) == NULL)
    return FALSE;
#endif

  // DVB device
  cfg_set_int ("dvb", "devno", config, devno);

  // Channel logos
  cfg_set_bool ("dvb", "logos.use", config, logos_use);
  cfg_set_string ("dvb", "logos.dir", config, logos_dir);

  // Logging
  cfg_set_bool ("dvb", "log.tofile", config, log_tofile);
  cfg_set_int ("dvb", "log.level", config, log_level);
  cfg_set_string ("dvb", "log.fname", config, log_filename);
  cfg_set_bool ("dvb", "log.append", config, log_append);

  // Recording
  cfg_set_bool ("dvb", "rec.onplay", config, rec_onplay);
  cfg_set_bool ("dvb", "rec.onpause", config, rec_onpause);
  cfg_set_string ("dvb", "rec.fname", config, rec_fname);
  cfg_set_bool ("dvb", "rec.append", config, rec_append);
  cfg_set_bool ("dvb", "rec.overwrite", config, rec_overwrite);

  // Splitting
  cfg_set_bool ("dvb", "isplit", config, isplit);
  cfg_set_int ("dvb", "isplit.ival", config, isplit_ival);
  cfg_set_bool ("dvb", "vsplit", config, vsplit);
  cfg_set_double ("dvb", "vsplit.vol", config, vsplit_vol * 100);
  cfg_set_int ("dvb", "vsplit.dur", config, vsplit_dur);
  cfg_set_int ("dvb", "vsplit.minlen", config, vsplit_minlen);

  // Information retrieval
  cfg_set_bool ("dvb", "info.dvbstat", config, info_dvbstat);
  cfg_set_bool ("dvb", "info.mmusic", config, info_mmusic);
  cfg_set_bool ("dvb", "info.epg", config, info_epg);
  cfg_set_bool ("dvb", "info.rt", config, info_rt);

#ifdef HAVE_AUDACIOUS_CONFIGDB_H
  aud_cfg_db_close (db);
#endif
  return TRUE;
}


void
config_exit (cfgstruct * cfg)
{
  g_free (cfg);
}
