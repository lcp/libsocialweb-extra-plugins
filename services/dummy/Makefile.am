noinst_LTLIBRARIES = libdummy.la

libdummy_la_SOURCES = module.c dummy.c dummy.h
libdummy_la_CFLAGS = $(LIBSOCIWEB_MODULE_CFLAGS) $(GOBJECT_CFLAGS) -DG_LOG_DOMAIN=\"Dummy\"
libdummy_la_LIBADD = $(LIBSOCIWEB_MODULE_LIBS) $(GOBJECT_LIBS)
libdummy_la_LDFLAGS = -module -avoid-version
