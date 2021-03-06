dnl mexext.m4 --- check for MEX-file suffix.
dnl
dnl Copyright (C) 2000--2003 Ralph Schleicher
dnl
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2,
dnl or (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; see the file COPYING.  If not, write to
dnl the Free Software Foundation, Inc., 59 Temple Place, Suite 330,
dnl Boston, MA 02111-1307, USA.
dnl
dnl As a special exception to the GNU General Public License, if
dnl you distribute this file as part of a program that contains a
dnl configuration script generated by GNU Autoconf, you may include
dnl it under the same distribution terms that you use for the rest
dnl of that program.
dnl
dnl Code:

# AX_MEXEXT
# ---------
# Check for MEX-file suffix.
AC_DEFUN([AX_MEXEXT],
[dnl
AC_PREREQ([2.50])
AC_REQUIRE([AX_PATH_MEX])
AC_CACHE_CHECK([for MEX-file suffix], [ax_cv_mexext],
[if test "${MEXEXT+set}" = set ; then
    ax_cv_mexext="$MEXEXT"
else
    if test -x ${MATLAB}/bin/mexext ; then
      ax_cv_mexext=`${MATLAB}/bin/mexext`
    fi
fi
if test "x${ax_cv_mexext}" = "x" ; then
    echo 'mexFunction () {}' > ax_c_test.c
    $MEX $MEXOPTS $MEXFLAGS -output ax_c_test ax_c_test.c $MEXLDADD 2> /dev/null 1>&2
    if test -f ax_c_test.dll ; then
	ax_cv_mexext=dll
    elif test -f ax_c_test.mex ; then
	ax_cv_mexext=mex
    elif test -f ax_c_test.mexaxp ; then
	ax_cv_mexext=mexaxp
    elif test -f ax_c_test.mexglx ; then
	ax_cv_mexext=mexglx
    elif test -f ax_c_test.mexa64 ; then
	ax_cv_mexext=mexa64
    elif test -f ax_c_test.mexmac ; then
	ax_cv_mexext=mexmac
    elif test -f ax_c_test.mexmaci ; then
	ax_cv_mexext=mexmaci
    elif test -f ax_c_test.mexhp7 ; then
	ax_cv_mexext=mexhp7
    elif test -f ax_c_test.mexhpux ; then
	ax_cv_mexext=mexhpux
    elif test -f ax_c_test.mexrs6 ; then
	ax_cv_mexext=mexrs6
    elif test -f ax_c_test.mexsg ; then
	ax_cv_mexext=mexsg
    elif test -f ax_c_test.mexsol ; then
	ax_cv_mexext=mexsol
    elif test -f ax_c_test.mexs64 ; then
	ax_cv_mexext=mexs64
    elif test -f ax_c_test.mexw32 ; then
	ax_cv_mexext=mexw32
    elif test -f ax_c_test.mexw64 ; then
	ax_cv_mexext=mexw64
    else
	ax_cv_mexext=unknown
    fi
    rm -f ax_c_test*
fi])
MEXEXT="$ax_cv_mexext"
AC_SUBST([MEXEXT])
])

# AX_DOT_MEXEXT
# -------------
# Check for MEX-file suffix with leading dot.
AC_DEFUN([AX_DOT_MEXEXT],
[dnl
AC_REQUIRE([AX_MEXEXT])
case $MEXEXT in
  .*)
    ;;
  unknown)
    ;;
  *)
    if test -n "$MEXEXT" ; then
	MEXEXT=.$MEXEXT
	AC_MSG_RESULT([setting MEX-file suffix to $MEXEXT])
	AC_SUBST([MEXEXT])
    fi
    ;;
esac
])

dnl mexext.m4 ends here
