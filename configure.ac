dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.69])
AC_INIT([skn_rpi-locator-service], [0.6.1], [James Scott Jr. <skoona@gmail.com>], 
        [skn_rpi-locator-service], [https://github.com/skoona/skn_rpi-display-services])
        
# Put autotools auxiliary files in subdirectories to reduce clutter:
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])

dnl Standard C constructs
AC_GNU_SOURCE
AC_HEADER_STDC
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# stricter options -ansi -pedantic
CFLAGS='-g -O2 -Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Winline -pipe -I/usr/include -I/usr/local/include -DPACKAGE_NAME="\"AC_PACKAGE_NAME\"" -DPACKAGE_VERSION="\"AC_PACKAGE_VERSION\""'
AC_SUBST(CFLAGS)

AC_CHECK_LIB([pthread],[pthread_create],
  AC_MSG_RESULT([pthread library is ready to use]),
  AC_MSG_FAILURE([Unable to identify pthread_create method in pthread or pthread not available.], [1])
)

AC_CHECK_LIB([wiringPi],[wiringPiSetup],
       AC_MSG_RESULT([Building WIRINGPI based services and clients.])
       AM_CONDITIONAL([WIRINGPI], [test "xY" = "xY"]),
       AC_MSG_RESULT([Building NON-WIRINGPI services and clients.])
       AM_CONDITIONAL([WIRINGPI], [test "xY" = "xX"]),
      [-lwiringPiDev -lrt -lm -lpthread]
)

PKG_CHECK_MODULES([GTKDS], [gssdp-1.0 >= 0.12],  
       AC_MSG_RESULT([Building GSSDP services and clients.])
       AM_CONDITIONAL([DOGSSDP], [test "xY" = "xY"]),
       AC_MSG_RESULT([Skipping GSSDP services and clients GSSDP not found.])
       AM_CONDITIONAL([DOGSSDP], [test "xY" = "xX"])
)

PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.10], 
       AC_MSG_RESULT([Building GTK+-3.0 services and clients.])
       AM_CONDITIONAL([DOGTK], [$DOGSSDP]),
       AC_MSG_RESULT([Skipping GTK+-3.0 services and clients GTK+ not found.])
       AM_CONDITIONAL([DOGTK], [test "xY" = "xX"])
)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/skn_rpi_helpers.c])
AC_CONFIG_FILES([Makefile src/Makefile gtkDS/Makefile cmdDS/Makefile cmdDC/Makefile gssdpDC/Makefile])

AC_OUTPUT

