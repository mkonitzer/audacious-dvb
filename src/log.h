/*******************************************************************************
**
** Filename:      log.h
**
** Function:      This include file ...
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
** 010902 -- CMO: Module created.
**
** 030622 -- CMO: Added a few return codes and some default log
**                levels.
**
** 040107 -- CMO: Fixed copyright statement to reflect the GPL status
**                of the code.
**
*******************************************************************************/

#ifndef RC_OK
#define RC_OK                         0
#endif

#define RC_LOG_OPEN_MALLOC_FAILED     3000
#define RC_LOG_OPEN_PREFIX_TOO_LONG   3002
#define RC_LOG_CLOSE_HANDLE_INVALID   3100
#define RC_LOG_PRINT_HANDLE_INVALID   3200


#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7


int log_open(void **, char *, int);
int log_close(void *);
int log_print(void *, int, char *, ...);
