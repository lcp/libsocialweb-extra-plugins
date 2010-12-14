panedir = $(BISHO_MODULES_DIR)
pane_LTLIBRARIES = libsina.la

AM_CPPFLAGS = $(DEPS_CFLAGS) -DLIBEXECDIR=\"@libexecdir@\"

libsina_la_SOURCES = sina.c sina.h

libsina_la_CFLAGS = $(BISHO_MODULE_CFLAGS) -I$(top_srcdir)/util
libsina_la_LDFLAGS = -module -avoid-version
libsina_la_LIBADD = $(BISHO_MODULE_LIBS) $(top_builddir)/util/libutil.la
