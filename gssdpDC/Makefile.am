## Process this file with automake to generate a Makefile.in
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

bin_PROGRAMS =

if DOGSSDP
bin_PROGRAMS += gssdpDC
endif

gssdpDC_SOURCES = gssdpDC.c
gssdpDC_CPPFLAGS = @GTK_CFLAGS@ @GTKDS_CFLAGS@
gssdpDC_LDADD = @GTK_LIBS@ @GTKDS_LIBS@

-include $(top_srcdir)/git.mk
