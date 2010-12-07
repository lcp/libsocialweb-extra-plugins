ACLOCAL_AMFLAGS = -I m4

SUBDIRS = services bisho po

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-youtube \
	--enable-plurk \
	--enable-sina \
	--enable-digg \
	--enable-myspace

EXTRA_DIST = intltool-extract.in intltool-merge.in intltool-update.in
DISTCLEANFILES = intltool-extract intltool-merge intltool-update
