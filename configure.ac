AC_PREREQ(2.53)
AC_INIT(libsocialweb-extra-plugins, 0.0.1)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign -Wno-portability])
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_ISC_POSIX
AC_HEADER_STDC
AM_PROG_CC_C_O

AS_ALL_LINGUAS
IT_PROG_INTLTOOL([0.40], [no-xml])
GETTEXT_PACKAGE=libsocialweb-extra-plugins
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

#AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

# TODO add po files
INTLTOOL_SOCIALWEB_KEYS='%.keys: %.keys.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST(INTLTOOL_SOCIALWEB_KEYS)

# TODO: bit nasty, should we use gnome-common?
CFLAGS="$CFLAGS -Wall"

# Libtool versioning
# See for details: http://bit.ly/Y5oX

# Dependencies
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14)
PKG_CHECK_MODULES(GIO, gio-2.0)
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.14)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4 gthread-2.0)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1)
PKG_CHECK_MODULES(REST, rest-0.6 rest-extras-0.6)
#PKG_CHECK_MODULES(KEYRING, gnome-keyring-1)
PKG_CHECK_MODULES(PANGO, pango)
PKG_CHECK_MODULES(JSON_GLIB, json-glib-1.0)
PKG_CHECK_MODULES(LIBSOCIWEB_MODULE, libsocialweb-module >= 0.24.9)
PKG_CHECK_MODULES(LIBSOCIWEB_KEYFOB, libsocialweb-keyfob >= 0.24.9)
PKG_CHECK_MODULES(LIBSOCIWEB_KEYFOB, libsocialweb-keystore >= 0.24.9)

#SOCIALWEB_ENABLE_SERVICE(Digg, digg, DIGG)
#OCIALWEB_ENABLE_SERVICE(MySpace, myspace, MYSPACE)
#SOCIALWEB_ENABLE_SERVICE(Facebook, facebook, FACEBOOK)
SOCIALWEB_ENABLE_SERVICE(Youtube, youtube, YOUTUBE)
SOCIALWEB_ENABLE_SERVICE(Plurk, plurk, PLURK)

servicesdir=`pkg-config --variable=servicesdir libsocialweb-module`
AC_SUBST(servicesdir)

servicesdatadir=`pkg-config --variable=servicesdatadir libsocialweb-module`
AC_SUBST(servicesdatadir)

SHAVE_INIT([build-aux],[enable])

AC_OUTPUT([
        build-aux/shave:m4/shave.in
        build-aux/shave-libtool:m4/shave-libtool.in
        Makefile
        services/Makefile
        services/dummy/Makefile
        services/youtube/Makefile
        services/plurk/Makefile
	po/Makefile.in
])
