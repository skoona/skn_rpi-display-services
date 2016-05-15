## Process this file with automake to generate a Makefile.in

if DOGTK
bin_PROGRAMS = gtkDS
endif

gtkDS_SOURCES = gtkDS.c
gtkDS_CPPFLAGS = @GTK_CFLAGS@ @GTKDS_CFLAGS@
gtkDS_LDADD = @GTK_LIBS@ @GTKDS_LIBS@

-include $(top_srcdir)/git.mk
