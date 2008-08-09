/* $Id$ */
/* Structures and methods to record to a file and split at given events

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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "record.h"
#include "util.h"
#include "log.h"

extern gpointer hlog;


recstruct *
record_init (void)
{
  return g_malloc0 (sizeof (recstruct));
}


gboolean
record_open (recstruct * rec, const gchar * filename, gboolean append,
	     gboolean overwrite)
{
  const gchar *dot = NULL;
  if (rec == NULL)
    return FALSE;

  // Try to split filename into prefix and suffix
  dot = g_strrstr (filename, ".");
  if (dot == NULL)
    return FALSE;

  rec->fn_prefix = g_strndup (filename, dot - filename);
  rec->fn_suffix = g_strdup (dot);

  if (overwrite)
    rec->filename = g_strdup (filename);
  else
    {
      if (g_file_test (filename, G_FILE_TEST_EXISTS))
	{
	  gchar *tmp = NULL;
	  do
	    {
	      if (tmp != NULL)
		g_free (tmp);
	      tmp =
		g_strdup_printf ("%s-%u%s", rec->fn_prefix, ++rec->fn_idx,
				 rec->fn_suffix);
	    }
	  while (g_file_test (tmp, G_FILE_TEST_EXISTS)
		 && rec->fn_idx < RECORD_INDEX_MAX);
	  if (rec->fn_idx >= RECORD_INDEX_MAX)
	    {
	      g_free (tmp);
	      log_print (hlog, LOG_INFO,
			 "Could not get an unused record file.");
	      return FALSE;
	    }
	  rec->filename = tmp;
	}
      else
	rec->filename = g_strdup (filename);
    }

  // Append to/open new/open next audio file
  if (append)
    {
      log_print (hlog, LOG_INFO, "Opening record '%s' for append",
		 rec->filename);
      rec->file = g_fopen (rec->filename, "ab");
    }
  else
    {
      log_print (hlog, LOG_INFO, "Opening record '%s'", rec->filename);
      rec->file = g_fopen (rec->filename, "wb");
    }

  if (rec->file == NULL)
    {
      log_print (hlog, LOG_INFO, "Could not open record '%s'.",
		 rec->filename);
      return FALSE;
    }

  return TRUE;
}


gboolean
record_next (recstruct * rec, gboolean append, gboolean overwrite)
{
  gchar *tmp = NULL;
  if (rec == NULL || rec->fn_prefix == NULL || rec->fn_suffix == NULL)
    return FALSE;

  tmp = g_strdup_printf ("%s-%u%s", rec->fn_prefix, ++rec->fn_idx,
			 rec->fn_suffix);

  if (overwrite)
    rec->filename = tmp;
  else
    {
      while (g_file_test (tmp, G_FILE_TEST_EXISTS)
	     && rec->fn_idx < RECORD_INDEX_MAX)
	{
	  g_free (tmp);
	  tmp =
	    g_strdup_printf ("%s-%u%s", rec->fn_prefix, ++rec->fn_idx,
			     rec->fn_suffix);
	}
      if (rec->fn_idx >= RECORD_INDEX_MAX)
	{
	  g_free (tmp);
	  log_print (hlog, LOG_INFO, "Could not get an unused record file.");
	  return FALSE;
	}
      rec->filename = tmp;
    }

  // Append to/open new/open next audio file
  if (append)
    {
      log_print (hlog, LOG_INFO, "Opening record '%s' for append",
		 rec->filename);
      rec->file = g_fopen (rec->filename, "ab");
    }
  else
    {
      log_print (hlog, LOG_INFO, "Opening record '%s'", rec->filename);
      rec->file = g_fopen (rec->filename, "wb");
    }

  if (rec->file == NULL)
    {
      log_print (hlog, LOG_INFO, "Could not open record '%s'.",
		 rec->filename);
      return FALSE;
    }

  return TRUE;
}


gint
record_write (recstruct * rec, guchar * buf, size_t len)
{
  if (rec == NULL)
    return -1;
  return fwrite (buf, sizeof (guchar), len, rec->file);
}


void
record_close (recstruct * rec)
{
  if (rec == NULL)
    return;

  log_print (hlog, LOG_INFO, "Closing record '%s'", rec->filename);
  fclose (rec->file);
  rec->file = NULL;
}


void
record_exit (recstruct * rec)
{
  if (rec == NULL)
    return;
  if (rec->file)
    record_close (rec);
  if (rec->fn_prefix)
    g_free (rec->fn_prefix);
  if (rec->fn_suffix)
    g_free (rec->fn_suffix);
  if (rec->filename)
    g_free (rec->filename);
  g_free (rec);
}
