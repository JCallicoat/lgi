%global luaver 5.1
%global luajitver 2.1
%global lualibdir %{_libdir}/lua/%{luaver}
%global luapkgdir %{_datadir}/lua/%{luaver}
%global luajitincludedir %{_includedir}/luajit-%{luajitver}

Name:		luajit-lgi
Version:	{{{ git_version name=lgi lead=0.9.2 }}}
Release:	1%{?dist}
Summary:	Luajit bindings to GObject libraries
License:	MIT
URL:		https://github.com/pavouk/lgi
VCS:        git+https://github.com/JCallicoat/lgi#HEAD:
Source0:	{{{ git_dir_pack }}}
BuildRequires:	pkgconfig(gobject-introspection-1.0) >= 0.10.8
BuildRequires:	pkgconfig(gmodule-2.0)
BuildRequires:	pkgconfig(libffi)
BuildRequires:	luajit >= %{luajitver}
BuildRequires:	luajit-devel >= %{luajitver}
BuildRequires:	lua-markdown
# for the testsuite:
BuildRequires:	pkgconfig(gio-2.0)
BuildRequires:	pkgconfig(cairo)
BuildRequires:	pkgconfig(cairo-gobject)
BuildRequires:	pkgconfig(gtk+-3.0)
BuildRequires:	gcc
BuildRequires:	make
BuildRequires:	which
BuildRequires:	Xvfb xauth
BuildRequires:	dbus-x11 at-spi2-core

Requires:	luajit >= %{luajitver}

%global __requires_exclude_from %{_docdir}
%{!?_pkgdocdir: %global _pkgdocdir %{_docdir}/%{name}-%{version}}

%description
LGI is gobject-introspection based dynamic Lua binding to GObject
based libraries. It allows using GObject-based libraries directly from
Lua.


%package samples
Summary:    Examples of luajit-lgi usage
# gtk-demo is LGPLv2+
License:    LGPLv2+ and MIT
Requires:   %{name}%{?_isa} = %{version}-%{release}

%description samples
%{summary}


%prep
{{{ git_dir_setup_macro }}}


%build
export CFLAGS="%{optflags} -DLUA_COMPAT_APIINTCASTS"
%configure || :
make LUA_INCDIR=%{luajitincludedir} LUA_CFLAGS="$(pkgconf --cflags luajit)" \
    %{?_smp_mflags}

# generate html documentation
markdown.lua README.md docs/*.md


%install
mkdir -p \
  %{buildroot}%{lualibdir} \
  %{buildroot}%{luapkgdir}
make install \
  "PREFIX=%{_prefix}" \
  "LUA_LIBDIR=%{lualibdir}" \
  "LUA_SHAREDIR=%{luapkgdir}" \
  "DESTDIR=%{buildroot}"

# install docs
mkdir -p %{buildroot}%{_pkgdocdir}
cp -av README.html docs/*.html \
  %{buildroot}%{_pkgdocdir}
cp -av samples %{buildroot}%{_pkgdocdir}
find %{buildroot}%{_pkgdocdir} -type f \
  -exec chmod -x {} \;


%check
export CFLAGS="%{optflags} -DLUA_COMPAT_APIINTCASTS"
%configure || :

# report failing tests, don't fail the build
xvfb-run -a -w 1 make check \
  LUA=%{_bindir}/luajit \
  LUA_CFLAGS=-I%{luajitincludedir} || :


%files
%dir %{_pkgdocdir}
%license LICENSE
%{_pkgdocdir}/*.html
%{luapkgdir}/lgi.lua
%{luapkgdir}/lgi
%{lualibdir}/lgi


%files samples
%{_pkgdocdir}/samples


%changelog
* Sat May 25 2024 Jordan Callicoat <jordan.callicoat@gmail.com> - master
- Switch to building from VCS master

* Sun Jul 26 2020 James Reed <jcrd@tuta.io> - 0.9.2-1
- Initial package (based on lua-lgi.spec)

