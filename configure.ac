AC_PREREQ([2.62])

AC_INIT([traktor], [0.1.0], [mfs-dm-2011@igalia.com],
	[traktor], [https://github.com/ceyusa/mfs2012])

AC_CONFIG_SRCDIR([backend/feed-service.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_HEADER_STDC

PKG_CHECK_MODULES([DEPS],  [dbus-1
			    glib-2.0
			    gio-2.0
			    gobject-2.0
			    libsoup-2.4
			    json-glib-1.0])

AC_ARG_WITH([services_dir],
	    AS_HELP_STRING([--with-services-dir],
            [Path to DBus services directory]))

if test "x$with_services_dir" = "x" ; then
   with_services_dir="`pkg-config --variable session_bus_services_dir dbus-1`"
fi

DBUS_SERVICES_DIR="$with_services_dir"
AC_SUBST(DBUS_SERVICES_DIR)



# ***********
# Translation
# ***********

LOCALE_DIR='$localedir'
AM_GLIB_DEFINE_LOCALEDIR([LOCALE_DIR])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.40.0])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package.])

AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)

#
#

AC_CONFIG_FILES([
	Makefile
	backend/Makefile
	po/Makefile.in
])


AC_OUTPUT
