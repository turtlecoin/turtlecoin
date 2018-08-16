Summary: example/test programs from ncurses
%define AppProgram ncurses-examples
%define AltProgram ncursest-examples
%define AppVersion MAJOR.MINOR
%define AppRelease YYYYMMDD
# $Id: ncurses-examples.spec,v 1.12 2018/06/02 22:46:44 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppRelease}
License: MIT
Group: Applications/Development
URL: ftp://ftp.invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppRelease}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
These are the example/test programs from the ncurses MAJOR.MINOR distribution,
for patch-date YYYYMMDD.

This package installs in "bin/%{AppProgram}" to avoid conflict with other
packages.

%package -n %{AltProgram}
Summary:  examples/test programs from ncurses with POSIX thread support

%description -n %{AltProgram}
These are the example/test programs from the ncurses MAJOR.MINOR distribution,
for patch-date YYYYMMDD, using the "ncurseswt" library to demonstrate the
use of POSIX threads, e.g., in ditto, rain, and worm.

This package installs in "bin/%{AltProgram}" to avoid conflict with other
packages.

%prep

%setup -q -n %{AppProgram}-%{AppRelease}

%define debug_package %{nil}

%build

%global _configure ../configure
%define my_srcdir ..

mkdir BUILD-%{AppProgram}
pushd BUILD-%{AppProgram}
INSTALL_PROGRAM='${INSTALL}' \
NCURSES_CONFIG_SUFFIX=dev \
CONFIGURE_TOP=%{my_srcdir} \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--bindir=%{_bindir}/%{AppProgram} \
	--datadir=%{_datadir}/%{AppProgram} \
	--with-screen=ncursesw6 \
	--disable-rpath-hack

make
popd

mkdir BUILD-%{AltProgram}
pushd BUILD-%{AltProgram}
INSTALL_PROGRAM='${INSTALL}' \
NCURSES_CONFIG_SUFFIX=dev \
CONFIGURE_TOP=%{my_srcdir} \
%configure \
	--target %{_target_platform} \
	--prefix=%{_prefix} \
	--bindir=%{_bindir}/%{AltProgram} \
	--datadir=%{_datadir}/%{AltProgram} \
	--with-screen=ncursestw6 \
	--disable-rpath-hack

make
popd

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

pushd BUILD-%{AppProgram}
make install DESTDIR=$RPM_BUILD_ROOT
popd

pushd BUILD-%{AltProgram}
make install DESTDIR=$RPM_BUILD_ROOT
popd

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files -n %{AppProgram}
%defattr(-,root,root)
%{_bindir}/%{AppProgram}/*
%{_datadir}/%{AppProgram}/*

%files -n %{AltProgram}
%defattr(-,root,root)
%{_bindir}/%{AltProgram}/*
%{_datadir}/%{AltProgram}/*

%changelog
# each patch should add its ChangeLog entries here

* Sat Nov 11 2017 Thomas Dickey
- add example data-files
- use rpm built-in "configure"
- suppress debug-package

* Thu Mar 25 2010 Thomas Dickey
- initial version
