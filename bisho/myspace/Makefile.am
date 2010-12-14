panedir = $(BISHO_MODULES_DIR)
pane_LTLIBRARIES = libmyspace.la

AM_CPPFLAGS = $(DEPS_CFLAGS) -DLIBEXECDIR=\"@libexecdir@\"

libmyspace_la_SOURCES = myspace.c myspace.h

libmyspace_la_CFLAGS = $(BISHO_MODULE_CFLAGS) -I$(top_srcdir)/util
libmyspace_la_LDFLAGS = -module -avoid-version
libmyspace_la_LIBADD = $(BISHO_MODULE_LIBS) $(top_builddir)/util/libutil.la
