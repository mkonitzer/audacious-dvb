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
** Version:       $Id: log.c,v 1.2 2004/04/07 14:46:11 douleftis Exp $
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
static char sccsid[] = "@(#)$Id: log.c,v 1.2 2004/04/07 14:46:11 douleftis Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>

#include "log.h"


typedef struct _HLOG {
  char  hl_fn[MAXPATHLEN];
  char  hl_pfx[256];
  int   hl_level;
} HLOG;

static char *month[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


/*******************************************************************************
** Function: log_open()
**
** Description: 
**
*******************************************************************************/

int log_open(char *fn, void **hlog, char *pfx, int lvl)
{
  HLOG  *hl;

  if ((hl = malloc(sizeof(HLOG))) == NULL) {
    *hlog = NULL;
    return (RC_LOG_OPEN_MALLOC_FAILED);
  }

  if (strlen(fn) >= sizeof(hl->hl_fn)) {
    *hlog = NULL;
    return (RC_LOG_OPEN_FILENAME_TOO_LONG);
  }
  strcpy(hl->hl_fn, fn);

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
  FILE      *f;
  HLOG      *hl;
  time_t    t;
  va_list   args;
  struct tm *tm;

  hl = (HLOG *)hlog;

  if (hl == NULL) {
    return (RC_LOG_PRINT_HANDLE_INVALID);
  }

  if (hl->hl_level >= lvl) {
    if ((f = fopen(hl->hl_fn, "a")) == NULL) {
      return (RC_LOG_PRINT_FOPEN_FAILED);
    }

    time(&t);
    tm = localtime(&t);
    fprintf(f, "%s %2d %02d:%02d:%02d %s: ", month[tm->tm_mon], tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, hl->hl_pfx);

    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");

    fflush(f);
    fclose(f);
  }

  return (RC_OK);
}
