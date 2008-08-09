AUTOMAKE_OPTIONS = gnu
lib_LTLIBRARIES = libdvb.la

AM_CFLAGS = @AUDACIOUS_CFLAGS@ @LIBGLADE_CFLAGS@ @GTK_CFLAGS@ @GLIB_CFLAGS@ @LIBMAD_CFLAGS@
LIBS = @AUDACIOUS_LIBS@ @LIBGLADE_LIBS@ @GTK_LIBS@ @GLIB_LIBS@ @LIBMAD_LIBS@

libdir = @PLUGIN_DIR@

libdvb_la_LDFLAGS = -module -avoid-version -no-undefined -export-dynamic
libdvb_la_SOURCES = main.c gui.c gui.h epg.c epg.h dvb.c dvb.h log.c log.h cfg.c cfg.h rtxt.c rtxt.h mmusic.c mmusic.h record.c record.h util.h util.c glwidgets.h

BUILT_SOURCES = glwidgets.h
MAINTAINERCLEANFILES = glwidgets.h

glwidgets.h: ../audacious-dvb.glade
	sed -e 's/\"/\\"/g' \
	    -e 's/^/\"/' \
	    -e 's/$$/\"/' \
	    -e 's/<\/glade-interface>\"/<\/glade-interface>\";/' \
	    -e '1 s/^/#include <glib.h>\n\nstatic const gchar * glwidgets = /' \
	    ../audacious-dvb.glade > glwidgets.h

