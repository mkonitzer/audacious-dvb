/* $Id$ */
/* Gives logging capabilities to audacious-dvb (via glib)

   Copyright (C) 2007  Marius Konitzer
   Copyright (C) 2003, 2004  Christian Motz
   This file is part of audacious-dvb.

   webchanges is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   webchanges is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with webchanges; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA  */

#ifndef lint
static char sccsid[] = "@(#)$Id$";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include <sys/param.h>

#include "log.h"


typedef struct _HLOG
{
  char hl_pfx[256];
  int hl_level;
} HLOG;


int
log_open (void **hlog, char *pfx, int lvl)
{
  HLOG *hl;

  if ((hl = malloc (sizeof (HLOG))) == NULL)
    {
      *hlog = NULL;
      return RC_LOG_OPEN_MALLOC_FAILED;
    }

  if (strlen (pfx) >= sizeof (hl->hl_pfx))
    {
      *hlog = NULL;
      return RC_LOG_OPEN_PREFIX_TOO_LONG;
    }
  strcpy (hl->hl_pfx, pfx);

  hl->hl_level = lvl;

  *hlog = (void *) hl;

  return RC_OK;
}


int
log_close (void *hlog)
{
  HLOG *hl;

  if ((hl = (HLOG *) hlog) == NULL)
    return RC_LOG_CLOSE_HANDLE_INVALID;

  free (hl);

  return RC_OK;
}


int
log_print (void *hlog, int lvl, char *fmt, ...)
{
  HLOG *hl;
  time_t t;
  va_list args;
  struct tm *tm;
  char msgbuf[256];

  hl = (HLOG *) hlog;

  if (hl == NULL)
    return RC_LOG_PRINT_HANDLE_INVALID;

  if (hl->hl_level >= lvl)
    {
      va_start (args, fmt);
      vsnprintf (msgbuf, 255, fmt, args);
      va_end (args);
      g_message ("[%s] %s", hl->hl_pfx, msgbuf);
    }

  return RC_OK;
}
