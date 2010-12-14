panedir = $(BISHO_MODULES_DIR)
pane_LTLIBRARIES = liboauth-webkit.la

AM_CPPFLAGS = $(DEPS_CFLAGS) -DLIBEXECDIR=\"@libexecdir@\"

liboauth_webkit_la_SOURCES = oauth-webkit.c oauth-webkit.h

liboauth_webkit_la_CFLAGS = $(BISHO_MODULE_CFLAGS) -I$(top_srcdir)/utils
liboauth_webkit_la_LDFLAGS = -module -avoid-version
liboauth_webkit_la_LIBADD = $(BISHO_MODULE_LIBS) $(top_builddir)/utils/libutil.la
