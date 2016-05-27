## Process this file with automake to generate a Makefile.in
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS = cmdDC

cmdDC_SOURCES = cmdDC.c
cmdDC_CPPFLAGS = @GTK_CFLAGS@
cmdDC_LDADD = @GTK_LIBS@

-include $(top_srcdir)/git.mk
