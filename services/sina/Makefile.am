services_LTLIBRARIES = libsina.la
libsina_la_SOURCES = module.c sina.c sina.h \
			sina-item-view.h sina-item-view.c
libsina_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(LIBSOCIWEB_KEYFOB_CFLAGS) $(LIBSOCIWEB_KEYSTORE_CFLAGS) $(REST_CFLAGS) $(GCONF_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Sina\"
libsina_la_LIBADD = $(LIBSOCIWEB_MODULE_LIBS) $(LIBSOCIWEB_KEYFOB_LIBS) $(LIBSOCIWEB_KEYSTORE_LIBS) $(REST_LIBS) $(GCONF_LIBS) $(DBUS_GLIB_LIBS)
libsina_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = sina.png

servicesdata_DATA = sina.keys
CLEANFILES = sina.keys
EXTRA_DIST = sina.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
