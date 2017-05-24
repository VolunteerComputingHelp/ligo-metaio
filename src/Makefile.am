# $Id$

include @top_srcdir@/ligotools/ligotools.mk

ligotools-local: all

AM_CPPFLAGS = -I$(top_builddir)/src -I$(top_srcdir)/src

bin_PROGRAMS = lwtscan lwtprint lwtdiff lwtcut _getMetaLoopHelper
bin_SCRIPTS = lwtselect concatMeta
include_HEADERS = metaio.h ligo_lw_header.h

check_PROGRAMS = parse_test parse_test_table_only
TESTS = metaio_test.sh

lib_LTLIBRARIES = libmetaio.la
libmetaio_la_LDFLAGS = -version-info $(LIBVERSION)

lwtscan_SOURCES = lwtscan.c metaio.h
lwtscan_LDADD = libmetaio.la

lwtprint_SOURCES = lwtprint.c metaio.h
lwtprint_LDADD = libmetaio.la

lwtdiff_SOURCES = lwtdiff.c metaio.h
lwtdiff_LDADD = libmetaio.la

lwtcut_SOURCES = lwtcut.c metaio.h
lwtcut_LDADD = libmetaio.la

parse_test_SOURCES = parse_test.c metaio.h
parse_test_LDADD = libmetaio.la

parse_test_table_only_SOURCES = parse_test_table_only.c metaio.h
parse_test_table_only_LDADD = libmetaio.la

_getMetaLoopHelper_SOURCES = _getMetaLoopHelper.c metaio.h
_getMetaLoopHelper_LDADD = libmetaio.la

libmetaio_la_SOURCES = metaio.c metaio.h base64.c base64.h ligo_lw_header.h
nodist_libmetaio_la_SOURCES = ligo_lw_header.c

ligo_lw_header.c : $(srcdir)/ligo_lw_header.xml
	sed -e 's/\"/\\\"/g' $< | awk 'BEGIN { printf "#include \"ligo_lw_header.h\"\n\nconst char MetaIO_Header[] = \"" } { printf $$0"\\\n" }; END { printf "\";\n" }' >$@

EXTRA_DIST = \
	$(bin_SCRIPTS) \
	blobtest.xml.gz \
	blobtest.xml.lwtcut_output \
	dmt_sample.xml \
	dmt_sample.xml.lwtcut_output \
	gdstrig10.xml \
	gdstrig10.xml.gz \
	gdstrig10.xml.lwtcut_output \
	gdstrig10.xml.lwtprint_output \
	gdstrig5000.xml \
	gdstrig5000.xml.gz \
	glueligolw_sample.xml.gz \
	ligo_lw_header.xml \
	metaio_test.sh

CLEANFILES = ligo_lw_header.c
