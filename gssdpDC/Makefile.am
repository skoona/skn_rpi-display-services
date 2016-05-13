## Process this file with automake to generate a Makefile.in

if DOGSSDP
	bin_PROGRAMS = gssdpDC
endif

gssdpDC_SOURCES = gssdpDC.c
gssdpDC_CPPFLAGS = @GTKDS_CFLAGS@
gssdpDC_LDADD = @GTKDS_LIBS@

CLEANFILES = *~ 

-include $(top_srcdir)/git.mk
