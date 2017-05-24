dnl matlabver.m4 --- check for Matlab version number.
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

# AX_MATLAB_VERSION
# -----------------
# Check for Matlab version number.
AC_DEFUN([AX_MATLAB_VERSION],
[dnl
AC_PREREQ([2.50])
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AX_MATLAB])
AC_CACHE_CHECK([for Matlab version], [ax_cv_matlab_version],
[if test "${MATLAB_VERSION+set}" = set ; then
    ax_cv_matlab_version=$MATLAB_VERSION
else
    ax_cv_matlab_version=
    # Loop over all known architectures.  The final dot covers
    # Matlab R11 and Matlab V4 for Windows.
    for ax_arch in alpha glnxa64 glnx86 hp700 hpux ibm_rs sgi sol2 win32 mac maci . ; do
        for ax_exec_name in matlab$EXEEXT MATLAB ; do
		ax_matlab_exec=$MATLAB/bin/$ax_arch/${ax_exec_name}
		if test -f $ax_matlab_exec ; then
	    		# For Matlab R12, the version number is stored in a
	    		# shared library.
	    		ax_matlab_exec_2=`find $MATLAB/bin/$ax_arch -type f -name libmwservices\* -print 2> /dev/null`
	    		if test -n "$ax_matlab_exec_2" ; then
				ax_cv_matlab_version=`strings $ax_matlab_exec_2 2> /dev/null | egrep '^\|build_version_\|@<:@0-9@:>@+\.@<:@0-9@:>@+\.@<:@0-9@:>@+\.@<:@0-9@:>@+' | head -1 | sed 's/^|build_version_|\(@<:@0-9@:>@*\.@<:@0-9@:>@*\).*/\1/'`
				if test -n "$ax_cv_matlab_version" ; then
		    			break
				fi
	    		fi
	    		# For Matlab R11 and Matlab V4, the version number
	    		# is stored in the executable program.
	    		ax_cv_matlab_version=`strings $ax_matlab_exec 2> /dev/null | egrep '^@<:@0-9@:>@+\.@<:@0-9@:>@+\.@<:@0-9@:>@+\.@<:@0-9@:>@+' | head -1 | sed 's/^\(@<:@0-9@:>@*\.@<:@0-9@:>@*\).*/\1/'`
	    		if test -n "$ax_cv_matlab_version" ; then
				break
	    		fi
		fi
    	done
	if test -n "$ax_cv_matlab_version" ; then
		break
	fi
    done
    if test -z "$ax_cv_matlab_version" ; then
	ax_cv_matlab_version="not found"
    fi
fi])
case $ax_cv_matlab_version in
  @<:@1-9@:>@.@<:@0-9@:>@ | @<:@1-9@:>@@<:@0-9@:>@.@<:@0-9@:>@)
    MATLAB_VERSION=$ax_cv_matlab_version
    MATLAB_MAJOR=`echo $MATLAB_VERSION | sed -e 's/^\(@<:@0-9@:>@*\)\.@<:@0-9@:>@*.*/\1/'`
    MATLAB_MINOR=`echo $MATLAB_VERSION | sed -e 's/^@<:@0-9@:>@*\.\(@<:@0-9@:>@*\).*/\1/'`
    ;;
  *)
    if test x$ax_enable_matlab = xyes ; then
	AC_MSG_ERROR([can not determine Matlab version number])
    fi
    MATLAB_VERSION=
    MATLAB_MAJOR=
    MATLAB_MINOR=
    ;;
esac
AC_SUBST([MATLAB_VERSION])
AC_SUBST([MATLAB_MAJOR])
AC_SUBST([MATLAB_MINOR])
if test x$MATLAB_VERSION != x ; then
    AC_DEFINE_UNQUOTED([MATLAB_MAJOR], [$MATLAB_MAJOR], [Define to the Matlab major version number.])
    AC_DEFINE_UNQUOTED([MATLAB_MINOR], [$MATLAB_MINOR], [Define to the Matlab minor version number.])
fi
])

# AX_REQUIRE_MATLAB_VERSION([MINIMUM-VERSION])
# --------------------------------------------
# Check if Matlab version number is sufficient.
AC_DEFUN([AX_REQUIRE_MATLAB_VERSION],
[dnl
AC_PREREQ([2.50])
AC_REQUIRE([AX_MATLAB_VERSION])
if test x$MATLAB_VERSION = x ; then
    AC_MSG_ERROR([can not determine Matlab version number])
fi
m4_if([$1], [], [],
[AC_MSG_CHECKING([if Matlab version is sufficient])
ax_version='$1'
case $ax_version in
  @<:@1-9@:>@ | @<:@1-9@:>@@<:@0-9@:>@)
    ax_major=$ax_version
    ax_minor=''
    ;;
  @<:@1-9@:>@.@<:@0-9@:>@ | @<:@1-9@:>@@<:@0-9@:>@.@<:@0-9@:>@ | @<:@1-9@:>@.@<:@1-9@:>@@<:@0-9@:>@ | @<:@1-9@:>@@<:@0-9@:>@.@<:@1-9@:>@@<:@0-9@:>@)
    ax_major=`echo $ax_version | sed 's/^\(@<:@0-9@:>@*\)\.@<:@0-9@:>@*.*/\1/'`
    ax_minor=`echo $ax_version | sed 's/^@<:@0-9@:>@*\.\(@<:@0-9@:>@*\).*/\1/'`
    ;;
  *)
    AC_MSG_RESULT([failure])
    AC_MSG_NOTICE([report this bug to the responsible package maintainer])
    AC_MSG_ERROR([invalid Matlab version number argument to AX_REQUIRE_MATLAB_VERSION])
    ;;
esac
ax_ans=yes
if test $MATLAB_MAJOR -eq $ax_major ; then
    if test x$ax_minor != x && test $MATLAB_MINOR -lt $ax_minor ; then
	ax_ans=no
    fi
elif test $MATLAB_MAJOR -lt $ax_major ; then
    ax_ans=no
fi
AC_MSG_RESULT([$ax_ans])
if test x$ax_ans = xno ; then
    AC_MSG_ERROR([require Matlab version $ax_version or above])
fi])
])

dnl matlabver.m4 ends here
