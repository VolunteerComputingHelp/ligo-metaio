# AX_MATLAB_SYS_ZLIB
# ------------
# Matlab preloads an old version of zlib which means that we cannot
# just dynamically link against the libz.so found by the linker.
# Instead we search for a working version of zlib in the system 
# location and use that.
AC_DEFUN([AX_MATLAB_SYS_ZLIB],
[dnl
AC_REQUIRE([AX_PATH_MEX])
AC_LANG([C])

ax_matlab_sys_zlib=

if test x$ax_cv_mexext != xunknown && test x$ax_cv_mexext != x ; then
  # Where should we search for libraries
  ax_sys_lib_search_path="/lib /usr/lib /usr/local/lib"

  # If this is a 64 bit link box look in the place where redhat keeps libs
  if test x$ax_cv_mexext = xmexa64; then
    ax_sys_lib_search_path="/lib64 /usr/lib64 /usr/local/lib64 $ax_sys_lib_search_path"
  fi

  # set the extension of the library we are looking for
  if test x$ax_cv_mexext = xmexmac || test x$ax_cv_mexext = xmexmaci ; then
    ax_zlib_lib_ext=".dylib"
  else
    ax_zlib_lib_ext=".a"
  fi

  for lib_path in $ax_sys_lib_search_path ; do
    ax_matlab_search_zlib=$lib_path/libz$ax_zlib_lib_ext

   AC_CHECK_FILE([$ax_matlab_search_zlib],
     [
       AC_MSG_CHECKING([for gzungetc in $ax_matlab_search_zlib])
       ac_save_LIBS=$LIBS
       LIBS="$ax_matlab_search_zlib $LIBS"
       AC_LINK_IFELSE(
         [ AC_LANG_PROGRAM( ,[[gzungetc();]]) ],
         [ 
           AC_MSG_RESULT([yes]) 
           ax_matlab_sys_zlib=$ax_matlab_search_zlib
         ],
         [ AC_MSG_RESULT([no]) ]
       )
       LIBS=$ac_save_LIBS
     ], )
    
    if test x$ax_matlab_sys_zlib != x ; then
      break
    fi
  done
fi

MATLAB_SYS_ZIB=$ax_matlab_sys_zlib

AC_SUBST([MATLAB_SYS_ZIB])

])
