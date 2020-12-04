
Name: libjsonrpccpp
Version: 0.2.1
Release: 1

Summary: C++ JSON-RPC Library
License: MIT License
Group: Development/Libraries
Vendor: Peter Spiess-Knafl
Url: https://github.com/cinemast/libjson-rpc-cpp

BuildRequires: automake
BuildRequires: gcc-c++
BuildRequires: libcurl-devel
Requires: libcurl

Source: %name-%version.tar.gz

Prefix: %_prefix
BuildRoot: %{_tmppath}/%name-%version-root

%description
This C++ library provides a json-rpc (remote procedure call) framework for 
Linux and MacOS (or any other UNIX derivate). It is fully JSON-RPC 2.0 
compatible (JSON-RPC 2.0) and provides additional features, such as generic 
authentication mechanisms.

# Install header files
%package devel
Requires: %name
Requires: libcurl-devel
Group: Development/Libraries
Vendor: Peter Spiess-Knafl
Summary: Header files for %name

%description devel
This C++ library provides a json-rpc (remote procedure call) framework for 
Linux and MacOS (or any other UNIX derivate). It is fully JSON-RPC 2.0 
compatible (JSON-RPC 2.0) and provides additional features, such as generic 
authentication mechanisms.

%prep
%setup -q

%build
%configure
make %{_smp_mflags} all

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%attr(755,root,root) %{_libdir}/libjsonrpccpp.so*
%attr(755,root,root) %{_bindir}/jsonrpcstub

%files devel
%defattr(644,root,root,755)
%{_libdir}/libjsonrpccpp.la
%{_libdir}/libjsonrpccpp.a

%dir %attr(755,root,root) %{_includedir}/jsonrpc
%{_includedir}/jsonrpc/*

