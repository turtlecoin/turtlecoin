Summary: shared libraries for terminal handling
Name: ncurses6
Version: 6.1
Release: 20180804
License: X11
Group: Development/Libraries
Source: ncurses-%{version}-%{release}.tgz
# URL: https://invisible-island.net/ncurses/

%define CC_NORMAL -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion
%define CC_STRICT %{CC_NORMAL} -W -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wwrite-strings -ansi -pedantic

%global MY_ABI 6

# save value before redefining
%global sys_libdir %{_libdir}

# was redefined...
#global _prefix /usr/local/ncurses#{MY_ABI}

%global MY_PKG %{sys_libdir}/pkgconfig
%define MYDATA /usr/local/ncurses/share/terminfo

%description
The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI %{MY_ABI}.

%package -n ncursest6
Summary:        Curses library with POSIX thread support.

%description -n ncursest6
The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI %{MY_ABI} with POSIX threads.

%prep

%global is_mandriva %(test -f /etc/mandriva-release && echo 1 || echo 0)
%global is_redhat   %(test -f /etc/redhat-release && echo 1 || echo 0)
%global is_suse     %(test -f /etc/SuSE-release && echo 1 || echo 0)

# nor are debug-symbols
%define debug_package %{nil}

%if %{is_mandriva}
%define _disable_ld_as_needed 1
%define _disable_ld_no_undefined 1
# libtool is not used here...
%define _disable_libtoolize 1
%define _disable_ld_build_id 1
%endif

%setup -q -n ncurses-%{version}-%{release}

%build
%define my_srcdir ..
%define CFG_OPTS \\\
	--target %{_target_platform} \\\
	--prefix=%{_prefix} \\\
	--bindir=%{_bindir} \\\
	--includedir=%{_includedir} \\\
	--libdir=%{_libdir} \\\
	--includedir='${prefix}/include' \\\
	--disable-echo \\\
	--disable-getcap \\\
	--disable-leaks \\\
	--disable-macros  \\\
	--disable-overwrite  \\\
	--disable-termcap \\\
	--enable-hard-tabs \\\
	--enable-opaque-curses \\\
	--enable-opaque-form \\\
	--enable-opaque-menu \\\
	--enable-opaque-panel \\\
	--enable-pc-files \\\
	--enable-rpath \\\
	--enable-warnings \\\
	--enable-wgetch-events \\\
	--enable-widec \\\
	--enable-xmc-glitch \\\
	--program-suffix=%{MY_ABI} \\\
	--verbose \\\
	--with-abi-version=%{MY_ABI} \\\
	--with-config-suffix=dev \\\
	--with-cxx-shared \\\
	--with-default-terminfo-dir=%{MYDATA} \\\
	--with-develop \\\
	--with-extra-suffix=%{MY_ABI} \\\
	--with-install-prefix=$RPM_BUILD_ROOT \\\
	--with-pkg-config-libdir=%{MY_PKG} \\\
	--with-shared \\\
	--with-terminfo-dirs=%{MYDATA}:/usr/share/terminfo \\\
	--with-termlib \\\
	--with-ticlib \\\
	--with-trace \\\
	--with-versioned-syms \\\
	--with-xterm-kbs=DEL \\\
	--without-ada \\\
	--without-debug \\\
	--without-normal

%global _configure ../configure

mkdir BUILD-ncurses6
pushd BUILD-ncurses6
CFLAGS="%{CC_NORMAL}" \
RPATH_LIST=../lib:%{_libdir} \
CONFIGURE_TOP=%{my_srcdir} \
%configure %{CFG_OPTS}
make
popd

mkdir BUILD-ncursest6
pushd BUILD-ncursest6
CFLAGS="%{CC_NORMAL}" \
RPATH_LIST=../lib:%{_libdir} \
CONFIGURE_TOP=%{my_srcdir} \
%configure %{CFG_OPTS} \
	--enable-interop \
	--enable-sp-funcs \
	--program-suffix=t%{MY_ABI} \
	--with-pthread
make
popd

%install
rm -rf $RPM_BUILD_ROOT

pushd BUILD-ncurses6
make install.libs install.progs
rm -f test/ncurses
( cd test && make ncurses LOCAL_LIBDIR=%{_libdir} && mv ncurses $RPM_BUILD_ROOT/%{_bindir}/ncurses%{MY_ABI} )
popd

pushd BUILD-ncursest6
make install.libs install.progs
rm -f test/ncurses
( cd test && make ncurses LOCAL_LIBDIR=%{_libdir} && mv ncurses $RPM_BUILD_ROOT/%{_bindir}/ncursest%{MY_ABI} )
popd

%clean
rm -rf $RPM_BUILD_ROOT

%files -n ncurses6
%defattr(-,root,root,-)
%{_bindir}/*
%{_includedir}/*
%{_libdir}/*
%exclude %{_bindir}/tict6
%exclude %{_bindir}/toet6
%exclude %{_bindir}/tabst6
%exclude %{_bindir}/resett6
%exclude %{_bindir}/???*tt6
%exclude %{_bindir}/?????*t6
%exclude %{_bindir}/*tw6*
%exclude %dir %{_includedir}/*tw6*
%exclude %{_includedir}/*tw6*/*
%exclude %{_libdir}/*tw6*
%exclude %{_libdir}/pkgconfig/*tw6*

%files -n ncursest6
%defattr(-,root,root,-)
%{_bindir}/tict6
%{_bindir}/toet6
%{_bindir}/tabst6
%{_bindir}/???*tt6
%{_bindir}/?????*t6
%{_bindir}/*tw6*
%{_includedir}/*tw6*
%{_libdir}/*tw6*
%{_libdir}/pkgconfig/*tw6*

%changelog

* Sat Jun 02 2018 Thomas E. Dickey
- build-fix for Mageia

* Sat May 26 2018 Thomas E. Dickey
- use predefined configure-macro
- separate ncurses6/ncursest6 packages

* Sat Feb 10 2018 Thomas E. Dickey
- add ncursest6 package
- add several development features

* Mon Jan 01 2018 Thomas E. Dickey
- drop redundant files pattern for "*.pc"

* Tue Dec 26 2017 Thomas E. Dickey
- add --with-config-suffix option

* Sun Apr 26 2015 Thomas E. Dickey
- move package to /usr

* Sun Apr 12 2015 Thomas E. Dickey
- factor-out MY_ABI

* Sat Mar 09 2013 Thomas E. Dickey
- add --with-cxx-shared option to demonstrate c++ binding as shared library

* Sat Oct 27 2012 Thomas E. Dickey
- add ncurses program as "ncurses6" to provide demonstration.

* Fri Jun 08 2012 Thomas E. Dickey
- initial version.
