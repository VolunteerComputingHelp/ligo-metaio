# $Id$

include @top_srcdir@/ligotools/ligotools.mk

ligotools-local: all

EXEEXT = $(MEXEXT)
AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src @MEX_INCLUDES@
CFLAGS =
CPPFLAGS =

matlab_PROGRAMS = readMeta
matlab_SCRIPTS = readMeta.m
readMeta_SOURCES = readMeta.c
nodist_readMeta_SOURCES = mexversion.c
readMeta_CFLAGS = @MEX_CFLAGS@
readMeta_CPPFLAGS = @MEX_CPPFLAGS@
readMeta_LDFLAGS = @MEX_LDFLAGS@
readMeta_LDADD = $(top_builddir)/src/metaio.lo $(top_builddir)/src/base64.lo $(top_builddir)/src/ligo_lw_header.lo @MATLAB_SYS_ZIB@ @MEX_LIBS@

CLEANFILES = mexversion.c
EXTRA_DIST = readMeta.c readMeta.m

if MEX_VERSION_C
mexversion.c:
	cp -f @MEX_VERSION_C@ mexversion.c
else
mexversion.c:
	touch mexversion.c
endif

