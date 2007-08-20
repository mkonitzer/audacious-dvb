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

#include "cfg.h"

void
config_init (cfgstruct * config)
{
  config->devno = 0;
  config->loglvl = 0;

  config->rec = FALSE;
  config->rec_fname = g_strdup ("/tmp/audacious-dvb-rec.mp2");
  config->rec_append = FALSE;

  config->isplit = FALSE;
  config->isplit_ival = 0;

  config->vsplit = FALSE;
  config->vsplit_vol = -42.5;
  config->vsplit_dur = 360;
  config->vsplit_minlen = 15;

  config->info_epg = TRUE;
  config->info_mmusic = FALSE;
}


gboolean
config_from_db (cfgstruct * config)
{
  ConfigDb *db;

  config_init (config);

  if ((db = bmp_cfg_db_open ()) == NULL)
    return FALSE;

  bmp_cfg_db_get_int (db, "dvb", "devno", &config->devno);
  bmp_cfg_db_get_int (db, "dvb", "loglevel", &config->loglvl);

  bmp_cfg_db_get_bool (db, "dvb", "rec", &config->rec);
  if (config->rec_fname != NULL)
    {
      g_free (config->rec_fname);
      config->rec_fname = NULL;
    }
  bmp_cfg_db_get_string (db, "dvb", "rec.fname", &config->rec_fname);
  if (config->rec_fname == NULL)
    config->rec_fname = g_strdup ("/tmp/audacious-dvb-rec.mp2");
  bmp_cfg_db_get_bool (db, "dvb", "rec.append", &config->rec_append);

  bmp_cfg_db_get_bool (db, "dvb", "isplit", &config->isplit);
  bmp_cfg_db_get_int (db, "dvb", "isplit.ival", &config->isplit_ival);

  bmp_cfg_db_get_bool (db, "dvb", "vsplit", &config->vsplit);
  if (bmp_cfg_db_get_double (db, "dvb", "vsplit.vol", &config->vsplit_vol))
    config->vsplit_vol /= 100;
  bmp_cfg_db_get_int (db, "dvb", "vsplit.dur", &config->vsplit_dur);
  bmp_cfg_db_get_int (db, "dvb", "vsplit.minlen", &config->vsplit_minlen);

  bmp_cfg_db_get_bool (db, "dvb", "info.mmusic", &config->info_mmusic);
  bmp_cfg_db_get_bool (db, "dvb", "info.epg", &config->info_epg);

  bmp_cfg_db_close (db);
}


gboolean
config_to_db (cfgstruct * config)
{
  ConfigDb *db;

  if ((db = bmp_cfg_db_open ()) == NULL)
    return FALSE;

  bmp_cfg_db_set_int (db, "dvb", "devno", config->devno);
  bmp_cfg_db_set_int (db, "dvb", "loglevel", config->loglvl);

  bmp_cfg_db_set_bool (db, "dvb", "rec", config->rec);
  bmp_cfg_db_set_string (db, "dvb", "rec.fname", config->rec_fname);
  bmp_cfg_db_set_bool (db, "dvb", "rec.append", config->rec_append);

  bmp_cfg_db_set_bool (db, "dvb", "isplit", config->isplit);
  bmp_cfg_db_set_int (db, "dvb", "isplit.ival", config->isplit_ival);

  bmp_cfg_db_set_bool (db, "dvb", "vsplit", config->vsplit);
  bmp_cfg_db_set_double (db, "dvb", "vsplit.vol", config->vsplit_vol * 100);
  bmp_cfg_db_set_int (db, "dvb", "vsplit.dur", config->vsplit_dur);
  bmp_cfg_db_set_int (db, "dvb", "vsplit.minlen", config->vsplit_minlen);

  bmp_cfg_db_set_bool (db, "dvb", "info.mmusic", config->info_mmusic);
  bmp_cfg_db_set_bool (db, "dvb", "info.epg", config->info_epg);

  bmp_cfg_db_close (db);
}
