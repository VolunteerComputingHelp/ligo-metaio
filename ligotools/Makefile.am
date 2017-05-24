# $Id$

include @top_srcdir@/ligotools/ligotools.mk

LDFLAGS=-all-static
AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src

EXTRA_DIST = ligotools.mk pkgfiles

lib_LTLIBRARIES =
noinst_PROGRAMS = _getMetaLoopHelper lwtcut lwtdiff lwtprint lwtscan
noinst_LTLIBRARIES = libmetaio.la

lwtscan_SOURCES = $(top_srcdir)/src/lwtscan.c $(top_srcdir)/src/metaio.h
lwtscan_LDADD = libmetaio.la

lwtprint_SOURCES = $(top_srcdir)/src/lwtprint.c $(top_srcdir)/src/metaio.h
lwtprint_LDADD = libmetaio.la

lwtdiff_SOURCES = $(top_srcdir)/src/lwtdiff.c $(top_srcdir)/src/metaio.h
lwtdiff_LDADD = libmetaio.la

lwtcut_SOURCES = $(top_srcdir)/src/lwtcut.c $(top_srcdir)/src/metaio.h
lwtcut_LDADD = libmetaio.la

_getMetaLoopHelper_SOURCES = $(top_srcdir)/src/_getMetaLoopHelper.c $(top_srcdir)/src/metaio.h
_getMetaLoopHelper_LDADD = libmetaio.la

libmetaio_la_SOURCES = $(top_srcdir)/src/metaio.c $(top_srcdir)/src/metaio.h $(top_srcdir)/src/ligo_lw_header.h
nodist_libmetaio_la_SOURCES = $(top_builddir)/src/ligo_lw_header.c

if READMETA_MEX
LIGOTOOLS_DIST_FILE = @PACKAGE_NAME@_@PACKAGE_VERSION@_$(host_triplet)-matlab-$(MATLAB_VERSION).tar.gz
else
LIGOTOOLS_DIST_FILE = @PACKAGE_NAME@_@PACKAGE_VERSION@_$(host_triplet)-nomatlab.tar.gz
endif

ligotools-local: all $(LIGOTOOLS_DIST_FILE)

LIGOTOOLS_BIN = \
    $(top_srcdir)/src/concatMeta \
    $(top_srcdir)/src/lwtselect

LIGOTOOLS_INCLUDE = \
    $(top_srcdir)/src/metaio.h \
    $(top_srcdir)/src/ligo_lw_header.h

if READMETA_MEX 
LIGOTOOLS_MATLAB = \
    $(top_srcdir)/matlab/readMeta.m \
    $(top_builddir)/matlab/readMeta$(MEXEXT)
else
LIGOTOOLS_MATLAB =
endif

$(LIGOTOOLS_DIST_FILE): \
    $(LIGOTOOLS_BIN) \
    $(LIGOTOOLS_SCR) \
    $(LIGOTOOLS_LIB) \
    $(LIGOTOOLS_INCLUDE) \
    $(LIGOTOOLS_MATLAB) \
    $(top_srcdir)/README \
    $(top_srcdir)/HISTORY \
    pkgfiles
	+mkdir -p bin include lib matlab
	+cp $(LIGOTOOLS_BIN) bin
	+cp $(noinst_PROGRAMS) bin
	+cp $(LIGOTOOLS_INCLUDE) include
	+cp $(top_srcdir)/README .
	+cp $(top_srcdir)/HISTORY .
	+cp $(top_builddir)/ligotools/.libs/libmetaio.a lib
	if test ! -f pkgfiles ; then \
          cp $(top_srcdir)/ligotools/pkgfiles . ; \
        else :; fi;
if READMETA_MEX
	+cp $(LIGOTOOLS_MATLAB) matlab
endif
	tar cf - README HISTORY pkgfiles bin include lib matlab \
          | gzip - > $(LIGOTOOLS_DIST_FILE)

clean-local:
	-rm -rf README HISTORY bin include lib matlab *.tar.gz
