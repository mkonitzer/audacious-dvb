lib_LTLIBRARIES = libdvb.la

libdir = @XMMS_INPUT_PLUGIN_DIR@

AM_CFLAGS = -Wall @XMMS_CFLAGS@ @CFLAGS@ -I$(top_builddir) -I$(top_srcdir) \
	    @GTK_CFLAGS@

LIBS = @XMMS_LIBS@
libdvb_la_LDFLAGS = -module -avoid-version -lmp3lame
libdvb_la_SOURCES = main.c gui.c epg.c dvb.c dvb.h log.c log.h
