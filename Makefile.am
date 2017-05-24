# $Id$

ACLOCAL_AMFLAGS = -I gnuscripts

include @top_srcdir@/ligotools/ligotools.mk

if LIGOTOOLS
LIGOTOOLS_SUBDIR = ligotools
endif
if READMETA_MEX
MATLAB_SUBDIR = matlab
endif

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA = libmetaio.pc

SUBDIRS = gnuscripts debian src $(MATLAB_SUBDIR) $(LIGOTOOLS_SUBDIR)

EXTRA_DIST = HISTORY libmetaio.spec

release :
	-$(MAKE) clean
	./configure --prefix=/usr
	$(MAKE)
	$(MAKE) dist
