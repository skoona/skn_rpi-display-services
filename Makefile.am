#
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
SUBDIRS = src

if DOGSSDP
SUBDIRS += gssdpDC
endif

if DOGTK
SUBDIRS += gtkDS cmdDS cmdDC
endif

dist_pkgdata_DATA = README.md  
doc_imagesdir = $(pkgdatadir)/images
doc_iscriptsdir = $(pkgdatadir)/iscripts
dist_doc_images_DATA = images/*
dist_doc_iscripts_DATA = iscripts/*
EXTRA_DIST = $(dist_doc_images_DATA) $(dist_doc_iscripts_DATA) autogen.sh COPYING 


DISTCLEANFILES =

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)


# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk

