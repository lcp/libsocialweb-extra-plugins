services_LTLIBRARIES = libdigg.la
libdigg_la_SOURCES = module.c digg.c digg.h \
		     digg-item-view.c digg-item-view.h
libdigg_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(LIBSOCIWEB_KEYFOB_CFLAGS) $(LIBSOCIWEB_KEYSTORE_CFLAGS) $(REST_CFLAGS) $(KEYRING_CFLAGS) $(DBUS_GLIB_CFLAGS) $(JSON_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Digg\"
libdigg_la_LIBADD =  $(LIBSOCIWEB_MODULE_LIBS) $(LIBSOCIWEB_KEYFOB_LIBS) $(LIBSOCIWEB_KEYSTORE_LIBS) $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS) $(JSON_GLIB_LIBS)
libdigg_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = digg.png

servicesdata_DATA = digg.keys
CLEANFILES = digg.keys
EXTRA_DIST = digg.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
