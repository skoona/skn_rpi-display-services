dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([skn_rpi-locator-service], [0.2.1], [James Scott Jr. <skoona@gmail.com>], skn_rpi-locator-service-0.2.1.tar.bz2)


AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

dnl Standard C constructs
AC_GNU_SOURCE
AC_HEADER_STDC
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl stricter options -ansi -pedantic
CFLAGS='-Wall -g -O0 -I/usr/include -I/usr/local/include -Winline -pipe -DSYSCONFDIR=\"/etc\"'

AC_CONFIG_FILES(Makefile src/Makefile)
AC_OUTPUT

