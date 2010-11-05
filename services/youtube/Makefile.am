services_LTLIBRARIES = libyoutube.la
libyoutube_la_SOURCES = module.c \
			youtube.c youtube.h \
			youtube-item-view.h youtube-item-view.c
libyoutube_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(LIBSOCIWEB_KEYFOB_CFLAGS) $(LIBSOCIWEB_KEYSTORE_CFLAGS) $(REST_CFLAGS) $(KEYRING_CFLAGS) $(DBUS_GLIB_CFLAGS) -DG_LOG_DOMAIN=\"Youtube\"
libyoutube_la_LIBADD = $(LIBSOCIWEB_MODULE_LIBS) $(LIBSOCIWEB_KEYFOB_LIBS) $(LIBSOCIWEB_KEYSTORE_LIBS) $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS)
libyoutube_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = youtube.png

servicesdata_DATA = youtube.keys
CLEANFILES = youtube.keys
EXTRA_DIST = youtube.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
