Name: libmetaio
Version: 8.4.0
Release: 1.lscsoft
Summary: LIGO Light-Weight XML library
License: GPL
Group: LSC Software/Data Analysis
Requires: zlib >= 1.2.0.2
BuildRequires: zlib-devel zlib-static
Source: metaio-%{version}.tar.gz
URL: http://www.lsc-group.phys.uwm.edu/daswg/projects/metaio.html
Packager: Xavier Amador <xavier.amador@gravity.phys.uwm.edu>
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Obsoletes: metaio
Prefix: /usr
%description
This code implements a simple recursive-descent parsing scheme for LIGO_LW
files, based on the example in Chapter 2 of "Compilers: Principles,
Techniques and Tools" by Aho, Sethi and Ullman.  Written by Philip Charlton
and Peter Shawhan.

This package contains the shared libraries needed for running libmetaio
applications.


%package devel
Summary: Files and documentation needed for compiling libframe programs
Group: LSC Software/Data Analysis
Requires: %{name} = %{version}
%description devel
This code implements a simple recursive-descent parsing scheme for LIGO_LW
files, based on the example in Chapter 2 of "Compilers: Principles,
Techniques and Tools" by Aho, Sethi and Ullman.  Written by Philip Charlton
and Peter Shawhan.

This package contains the files needed for building libmetaio applications.


%package utils
Summary: LIGO Light-Weight XML file manipulation utilities
Group: LSC Software/Data Analysis
Requires: %{name} = %{version}
%description utils
This package provides the utilities, such as lwtprint, which accompany the
libmetaio source code.


#%package matlab
#Summary: LIGO Light-Weight XML MatLab module
#Group: LSC Software/Data Analysis
#Requires: %{name} = %{version}
#%description matlab
#This package provides the MatLab readMeta module from libmetaio.


%prep
%setup -q -n metaio-%{version}


%build
%configure --without-matlab
%{__make}


%install
%makeinstall
# remove .so symlinks from libdir.  these are not included in the .rpm,
# they will be installed by ldconfig in the post-install script, except for
# the .so symlink which isn't created by ldconfig and gets shipped in the
# devel package
[ ${RPM_BUILD_ROOT} != "/" ] && find ${RPM_BUILD_ROOT}/%{_libdir} -name "*.so.*" -type l -delete
# remove .la files from libdir.  these are not included in the .rpm
[ ${RPM_BUILD_ROOT} != "/" ] && find ${RPM_BUILD_ROOT}/%{_libdir} -name "*.la" -delete


%post
ldconfig %{_libdir}


%postun
ldconfig %{_libdir}


%clean
[ ${RPM_BUILD_ROOT} != "/" ] && rm -Rf ${RPM_BUILD_ROOT}
rm -Rf ${RPM_BUILD_DIR}/%{name}-%{version}


%files
%defattr(-,root,root)
%{_libdir}/*.so.*

%files devel
%defattr(-,root,root)
%{_libdir}/*.a
%{_libdir}/*.so
%{_libdir}/pkgconfig/*
%{_includedir}/*

#%files matlab
#%defattr(-,root,root)
#%{_datadir}/matlab/*

%files utils
%defattr(-,root,root)
%{_bindir}/*

