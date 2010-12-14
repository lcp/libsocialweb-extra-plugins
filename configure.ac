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
PKG_CHECK_MODULES(REST, rest-0.7 rest-extras-0.7 >= 0.7.1)
PKG_CHECK_MODULES(PANGO, pango)
PKG_CHECK_MODULES(JSON_GLIB, json-glib-1.0)
PKG_CHECK_MODULES(KEYRING, gnome-keyring-1)
PKG_CHECK_MODULES(LIBSOCIWEB_MODULE, libsocialweb-module >= 0.25.5)
PKG_CHECK_MODULES(LIBSOCIWEB_KEYFOB, libsocialweb-keyfob >= 0.25.5)
PKG_CHECK_MODULES(LIBSOCIWEB_KEYSTORE, libsocialweb-keystore >= 0.25.5)

SOCIALWEB_ENABLE_SERVICE(Youtube, youtube, YOUTUBE)
SOCIALWEB_ENABLE_SERVICE(Plurk, plurk, PLURK)
SOCIALWEB_ENABLE_SERVICE(Sina, sina, SINA)
SOCIALWEB_ENABLE_SERVICE(Digg, digg, DIGG)
SOCIALWEB_ENABLE_SERVICE(Myspace, myspace, MYSPACE)

PKG_CHECK_MODULES(BISHO_MODULE,
                  bisho
                  mx-gtk-1.0
                  gnome-keyring-1
                  libsocialweb-keystore
                  rest-0.7
                  libsoup-gnome-2.4
                  json-glib-1.0
                  gconf-2.0 >= 1.2.0)

PKG_CHECK_MODULES(UTIL, 
                  webkit-1.0,
                  gtk+-2.0)

AC_MSG_CHECKING([Bisho modules dir])
AC_ARG_WITH([bisho-modules-dir],
            [AC_HELP_STRING([--with-bisho-modules-dir],
                            [directory to install bisho modules])],
                            [], [with_bisho_modules_dir=""])
if test -z "$with_bisho_modules_dir"; then
  BISHO_MODULES_DIR=`$PKG_CONFIG --variable=modulesdir bisho`
else
  BISHO_MODULES_DIR="$with_bisho_modules_dir"
fi

if test "x$BISHO_MODULES_DIR" = "x"; then
    AC_MSG_ERROR([could not determine modulesdir])
else
    AC_MSG_RESULT([$BISHO_MODULES_DIR])
fi
AC_SUBST(BISHO_MODULES_DIR)

servicesdir=`pkg-config --variable=servicesdir libsocialweb-module`
AC_SUBST(servicesdir)

servicesdatadir=`pkg-config --variable=servicesdatadir libsocialweb-module`
AC_SUBST(servicesdatadir)

SHAVE_INIT([build-aux],[enable])

AC_OUTPUT([
        build-aux/shave:m4/shave.in
        build-aux/shave-libtool:m4/shave-libtool.in
        Makefile
	utils/Makefile
	bisho/Makefile
	bisho/dummy/Makefile
	bisho/sina/Makefile
        services/Makefile
        services/dummy/Makefile
        services/youtube/Makefile
        services/plurk/Makefile
        services/sina/Makefile
        services/myspace/Makefile
        services/digg/Makefile
	po/Makefile.in
])
