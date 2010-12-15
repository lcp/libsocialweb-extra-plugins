services_LTLIBRARIES = libplurk.la
libplurk_la_SOURCES = module.c plurk.c plurk.h \
			plurk-item-view.h plurk-item-view.c
libplurk_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(LIBSOCIWEB_KEYFOB_CFLAGS) $(LIBSOCIWEB_KEYSTORE_CFLAGS) $(REST_CFLAGS) $(KEYRING_CFLAGS) $(DBUS_GLIB_CFLAGS) $(JSON_GLIB_CFLAGS) -I$(top_srcdir)/utils -DG_LOG_DOMAIN=\"Plurk\"
libplurk_la_LIBADD = $(LIBSOCIWEB_MODULE_LIBS) $(LIBSOCIWEB_KEYFOB_LIBS) $(LIBSOCIWEB_KEYSTORE_LIBS) $(REST_LIBS) $(KEYRING_LIBS) $(DBUS_GLIB_LIBS) $(JSON_GLIB_LIBS) $(top_builddir)/utils/libutil.la
libplurk_la_LDFLAGS = -module -avoid-version

dist_servicesdata_DATA = plurk.png

servicesdata_DATA = plurk.keys
CLEANFILES = plurk.keys
EXTRA_DIST = plurk.keys.in
@INTLTOOL_SOCIALWEB_KEYS@
