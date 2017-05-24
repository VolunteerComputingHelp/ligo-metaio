# Preamble
AC_PREREQ([2.69])
AC_INIT([metaio], [8.4.0], [daswg@ligo.org])
AM_CONFIG_HEADER([src/config.h])
AC_CONFIG_AUX_DIR([gnuscripts])
AC_CONFIG_MACRO_DIR([gnuscripts])
AC_CONFIG_SRCDIR([libmetaio.pc.in])
AC_CONFIG_FILES([ \
	Makefile \
	gnuscripts/Makefile \
	debian/Makefile \
	debian/control \
	src/Makefile \
	matlab/Makefile \
	ligotools/Makefile \
	libmetaio.spec \
	libmetaio.pc \
])
AM_INIT_AUTOMAKE([foreign])

# Set the library API info
#  0.0.0   Original version
#  0.1.0   Add gzip file support
#  0.1.1   Minor bugfixes: tablename string not freed, parsing of 
#          backslash-delimited octal chars reads past delimiter.
#  1.0.0   8.0 release.  Add support for additional types, change string
#          parsing rules.
#  2.0.1   8.3 release.  Add MetaioClearErrno(), add
#          MetaioGetErrorMessage(), add support for complex numbers
#
# NOTE:  this is the *libtool* version information.  the numbers attached
# to the .so file will be different, they are determined by libtool using
# a platform-specific algorithm.  we need to know the leading digit for
# packaging purposes, so I have attempted to reproduce libtool's algorithm
# below and store the result in SONAME.  however, this might not always be
# correct, so watch for that when building packages.
AC_SUBST([LIBAPI], [2])
AC_SUBST([LIBREL], [0])
AC_SUBST([LIBAGE], [1])
AC_SUBST([LIBVERSION], [${LIBAPI}:${LIBREL}:${LIBAGE}])
AC_SUBST([SONAME], [$(($LIBAPI - $LIBAGE))])

# Checks for programs.  AC_PROG_CC_STDC tries to find a C99 compiler, which
# is required to enable the optional zlib-related features, and provide
# support for complex numbers
AC_EXEEXT
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
LT_INIT

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AX_CFLAGS_WARN_ALL([AM_CFLAGS])
AC_SUBST([AM_CFLAGS])

# Checks for libraries.
AC_HAVE_LIBRARY([m])
AX_CHECK_ZLIB()

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strcasecmp strcspn strdup strrchr])
if test x$HAVE_LIBZ = "xyes" ; then
	AC_CHECK_FUNCS([vsscanf])
	if test x$HAVE_VSSCANF = "xno" ; then
		AC_MSG_FAILURE([vsscanf() is required for compression support.  specify a working C99 compiler using the CC environment variable, or disable support for compressed files with --without-zlib])
	fi
	AC_CHECK_FUNCS([gzungetc])
	if test x$HAVE_GZUNGETC = "xno" ; then
		AC_MSG_FAILURE([gzungetc() missing from zlib.  zlib >= 1.2.x is required])
	fi
fi

# Check for MatLab
AX_MATLAB
if test x$ax_enable_matlab != "xno" ; then
	AX_MATLAB_DIR
	AX_CC_MEX
	AX_MATLAB_SYS_ZLIB
	if test x$ac_cv_mex_working = "xyes" -a x$ac_cv_use_zlib = "xyes" -a x$ax_matlab_sys_zlib = "x" ; then
		AC_MSG_FAILURE([cannot find a version of zlib with gzungetc needed for readMeta])
	fi
fi
AX_MATLAB_CONDITIONAL
AX_MEX_CONDITIONAL
AM_CONDITIONAL([READMETA_MEX], [test x$ac_cv_mex_working = "xyes"])

# ligotools switch
AC_ARG_ENABLE(
	[ligotools],
	[AS_HELP_STRING(
		[--disable-ligotools],
		[disable building the ligotools subdirectory (default).  use --enable-ligotools to enable.]
	)]
)
AM_CONDITIONAL([LIGOTOOLS], [test x$enable_ligotools = "xyes"])

# Output
AC_PROG_MAKE_SET
AC_OUTPUT
