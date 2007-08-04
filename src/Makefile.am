lib_LTLIBRARIES = libdvb.la

AM_CFLAGS = @AUDACIOUS_CFLAGS@ @GTK_CFLAGS@ @GLIB_CFLAGS@
LIBS = @AUDACIOUS_LIBS@ @GTK_LIBS@ @GLIB_LIBS@

libdir = @PLUGIN_DIR@

libdvb_la_LDFLAGS = -module -avoid-version -export-dynamic -lmp3lame
libdvb_la_SOURCES = main.c gui.c gui.h epg.c epg.h dvb.c dvb.h log.c log.h
