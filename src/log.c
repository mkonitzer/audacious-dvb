/*******************************************************************************
**
** Filename:      log.c
**
** Function List: log_open()
**                log_close()
**                log_print()
**
** Function:      This is ...
**
** Copyright:     (C) COPYRIGHT CHRISTIAN MOTZ 2003, 2004
**
**                This program is free software; you can redistribute
**                it and/or modify it under the terms of the GNU
**                General Public License as published by the Free
**                Software Foundation; either version 2, or (at your
**                option) any later version.
**
** Version:       $Id$
**
** Change Activity:
**
** 010902 -- CMO: Module created
**
** 030622 -- CMO: Added some sanity checks and return codes.
**
** 040107 -- CMO: Fixed copyright statement to reflect the GPL status
**                of the code.
**
*******************************************************************************/

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


typedef struct _HLOG {
  char  hl_pfx[256];
  int   hl_level;
} HLOG;


/*******************************************************************************
** Function: log_open()
**
** Description: 
**
*******************************************************************************/

int log_open(void **hlog, char *pfx, int lvl)
{
  HLOG  *hl;

  if ((hl = malloc(sizeof(HLOG))) == NULL) {
    *hlog = NULL;
    return (RC_LOG_OPEN_MALLOC_FAILED);
  }

  if (strlen(pfx) >= sizeof(hl->hl_pfx)) {
    *hlog = NULL;
    return (RC_LOG_OPEN_PREFIX_TOO_LONG);
  }
  strcpy(hl->hl_pfx, pfx);

  hl->hl_level = lvl;

  *hlog = (void *)hl;

  return (RC_OK);
}


/*******************************************************************************
** Function: log_close()
**
** Description: 
**
*******************************************************************************/

int log_close(void *hlog)
{
  HLOG  *hl;

  hl = (HLOG *)hlog;

  if (hl == NULL) {
    return (RC_LOG_CLOSE_HANDLE_INVALID);
  }

  free(hl);

  return (RC_OK);
}


/*******************************************************************************
** Function: log_print()
**
** Description: 
**
*******************************************************************************/

int log_print(void *hlog, int lvl, char *fmt, ...)
{
  HLOG      *hl;
  time_t    t;
  va_list   args;
  struct tm *tm;
  char	    msgbuf[256];

  hl = (HLOG *)hlog;

  if (hl == NULL) {
    return (RC_LOG_PRINT_HANDLE_INVALID);
  }

  if (hl->hl_level >= lvl) {
    va_start(args, fmt);
    vsnprintf(msgbuf, 255, fmt, args);
    va_end(args);
    g_message("[%s] %s", hl->hl_pfx, msgbuf);
  }

  return (RC_OK);
}
