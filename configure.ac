dnl Process this file with autoconf to produce a configure script.

AC_INIT([skn_rpi-locator-service], [0.5.2], [James Scott Jr. <skoona@gmail.com>], 
        [skn_rpi-locator-service], [https://github.com/skoona/skn_rpi-display-services])
        
AC_PREREQ([2.69])

AC_CONFIG_SRCDIR([src/skn_rpi_helpers.c])
AC_CONFIG_HEADERS([config.h])

# Put autotools auxiliary files in subdirectories to reduce clutter:
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign])
AM_SILENT_RULES([yes])
# m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_CONFIG_FILES(Makefile src/Makefile)

# stricter options -ansi -pedantic
CFLAGS='-Wall -Wextra -g -O0 -I/usr/include -I/usr/local/include -Winline -pipe -DPACKAGE_NAME="AC_PACKAGE_NAME" -DPACKAGE_VERSION=\"AC_PACKAGE_VERSION\"'
AC_SUBST(CFLAGS)

dnl Standard C constructs
AC_GNU_SOURCE
AC_HEADER_STDC
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_CHECK_LIB([pthread],[pthread_create],
  [AC_MSG_RESULT([pthread library is ready to use])],
  [AC_MSG_FAILURE([Unable to identify pthread_create method in pthread or pthread not available.])]
)

AM_CONDITIONAL([OSXBUILD], [test "x$_system_type" = "xDarwin"])
AM_COND_IF([OSXBUILD],  
   [AC_MSG_RESULT([Building udp_locator_service and limited clients only for OSX platforms])],
   [AC_CHECK_LIB([wiringPi],[wiringPiSetup],
      [AC_MSG_RESULT([Building all components for NON-OSX platforms])],
      [AC_MSG_FAILURE([wiringPiDev.so and wiringPi.so are Required.])]
   )],[-lwiringPiDev -lrt -lm -lpthread]
)

AC_OUTPUT

