services_LTLIBRARIES = libmyspace.la
libmyspace_la_SOURCES = module.c myspace.c myspace.h \
		        myspace-item-view.h myspace-item-view.c
libmyspace_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(LIBSOCIWEB_KEYFOB_CFLAGS) $(LIBSOCIWEB_KEYSTORE_CFLAGS) $(REST_CFLAGS) $(DBUS_GLIB_CFLAGS) $(PANGO_CFLAGS) -DG_LOG_DOMAIN=\"MySpace\"
libmyspace_la_LIBADD = $(LIBSOCIWEB_MODULE_LIBS) $(LIBSOCIWEB_KEYFOB_LIBS) $(LIBSOCIWEB_KEYSTORE_LIBS) $(REST_LIBS) $(DBUS_GLIB_LIBS) $(PANGO_LIBS)
libmyspace_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = myspace.png

servicesdata_DATA = myspace.keys
CLEANFILES = myspace.keys
EXTRA_DIST = myspace.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
