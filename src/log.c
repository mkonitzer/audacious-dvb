/* $Id$ */
/* Gives logging capabilities to audacious-dvb (via glib)

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
#include <glib/gstdio.h>
#include <string.h>

#include "log.h"
#include "util.h"


typedef struct _HLOG
{
  enum lvltype level;
  // in glib-mode
  gchar *prefix;
  // in file-mode
  FILE *file;
  gchar *filename;
} HLOG;


gint
log_glib_open (gpointer * hlog, const gchar * pfx, enum lvltype lvl)
{
  HLOG *hl;

  if (pfx == NULL || &hlog == NULL)
    return RC_NPE;

  // Allocate memory for HLOG structure
  if ((hl = g_malloc0 (sizeof (HLOG))) == NULL)
    {
      *hlog = NULL;
      return RC_LOG_ERROR;
    }

  // Fill in (default) values
  hl->prefix = g_strdup (pfx);
  hl->level = lvl;
  *hlog = (void *) hl;

  return RC_OK;
}


gint
log_file_open (gpointer * hlog, const gchar * filename, gboolean append,
	       enum lvltype lvl)
{
  HLOG *hl;

  if (filename == NULL || &hlog == NULL)
    return RC_NPE;

  // Allocate memory for HLOG structure
  if ((hl = g_malloc0 (sizeof (HLOG))) == NULL)
    {
      *hlog = NULL;
      return RC_LOG_ERROR;
    }

  // Try to open logfile
  hl->file = g_fopen (filename, (append ? "a" : "w"));
  if (hl->file == NULL)
    {
      g_free (hl);
      *hlog = NULL;
      return RC_LOG_ERROR;
    }

  // Fill in (default) values
  hl->filename = g_strdup (filename);
  hl->level = lvl;
  *hlog = (void *) hl;

  return RC_OK;
}


gint
log_close (gpointer hlog)
{
  HLOG *hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_NPE;

  // Free HLOG structure
  if (hl->file != NULL)
    {
      g_fprintf (hl->file, "\n");
      fclose (hl->file);
    }
  if (hl->filename != NULL)
    g_free (hl->filename);
  if (hl->prefix != NULL)
    g_free (hl->prefix);
  g_free (hl);

  return RC_OK;
}


gint
log_print (const gpointer hlog, enum lvltype lvl, const gchar * fmt, ...)
{
  HLOG *hl;
  hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_NPE;

  // Only print message that has at least current level
  if (hl->level >= lvl)
    {
      gchar *msg;
      va_list args;
      va_start (args, fmt);
      msg = g_strdup_vprintf (fmt, args);

      if (hl->file == NULL)
	g_message ("[%s] %s", hl->prefix, msg);
      else
	{
	  GTimeVal now;
	  gchar *nowstr = NULL;
	  g_get_current_time (&now);
	  nowstr = g_time_val_to_iso8601 (&now);
	  g_fprintf (hl->file, "%s %s\n", nowstr, msg);
	  g_free (nowstr);
	  fflush (hl->file);
	}
      g_free (msg);
      va_end (args);
    }

  return RC_OK;
}

gint
log_set_level (gpointer hlog, enum lvltype lvl)
{
  HLOG *hl = (HLOG *) hlog;
  if (hl == NULL)
    return RC_NPE;

  // Update current with given level
  hl->level = lvl;
  return RC_OK;
}
