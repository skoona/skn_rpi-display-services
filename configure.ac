dnl Process this file with autoconf to produce a configure script.

AC_INIT([skn_rpi-locator-service], [0.2.2], [James Scott Jr. <skoona@gmail.com>], 
        [skn_rpi-locator-service], [https://github.com/skoona/skn_rpi-display-services])
        
AC_PREREQ([2.69])

AC_CONFIG_SRCDIR([src/skn_rpi_helpers.c])
AC_CONFIG_HEADERS([config.h])

# Put autotools auxiliary files in subdirectories to reduce clutter:
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE
AC_CONFIG_FILES(Makefile src/Makefile)

# stricter options -ansi -pedantic
CFLAGS=-Wall -g -O0 -I/usr/include -I/usr/local/include -pipe

dnl Standard C constructs
AC_GNU_SOURCE
AC_HEADER_STDC
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CHECK_LIB([pthread],[pthread_create])

# AC_CHECK_LIB([wiringPi],[lcdInit])
# AC_CHECK_LIB([wiringPi],[pcf8574Setup])

# ref: http://www.gnu.org/software/autoconf-archive/ax_pthread.html#ax_pthread
# AX_PTHREAD


AC_OUTPUT

