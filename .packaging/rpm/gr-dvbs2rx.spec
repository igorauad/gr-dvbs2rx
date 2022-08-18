Name:           gr-dvbs2rx
Version:        [%version%]
Release:        [%release%]%{?dist}
Summary:        DVB-S2 Receiver GNU Radio Module

License:        GPLv3
URL:            https://igorauad.github.io/%{name}/
Source0:        https://github.com/igorauad/%{name}/archive/%{version}/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  doxygen
BuildRequires:  fftw-devel
BuildRequires:  g++
BuildRequires:  gmp-devel
BuildRequires:  gnuradio-devel >= 3.10
BuildRequires:  graphviz
BuildRequires:  libsndfile-devel
BuildRequires:  pybind11-devel
BuildRequires:  spdlog-devel
Requires: gnuradio >= 3.10
Requires: python3

%description

A GNU Radio out-of-tree module containing signal processing blocks and full
transmitter/receiver applications for DVB-S2 over software-defined radio.

%package devel
Summary: DVB-S2 Receiver GNU Radio Module
Requires:	%{name}% = %{version}-%{release}
Requires:	cmake

%description devel
gr-dvbs2rx GNU Radio out-of-tree module headers.

%package doc
Summary: DVB-S2 Receiver GNU Radio Module
Requires:	%{name} = %{version}-%{release}

%description doc
gr-dvbs2rx GNU Radio out-of-tree module documentation.

%prep
%setup -q

%build
%cmake -DNATIVE_OPTIMIZATIONS=OFF
%cmake_build

%install
%cmake_install

%check
%ctest

%ldconfig_scriptlets

%files
%{_bindir}/*
%{_libdir}/lib*.so.*
%{_datadir}/gnuradio
%{python3_sitearch}/gnuradio/dvbs2rx/

%files devel
%{_includedir}/*
%{_libdir}/lib*.so
%{_libdir}/cmake/dvbs2rx

%files doc
%doc %{_docdir}/%{name}

%changelog
* Sat Aug 20 2022 Igor Freire <igor@ifcomm.com.br> - 1.0.0-1
- Initial version
