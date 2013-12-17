Name:		restraint
Version:	0.1.2
Release:	1%{?dist}
Summary:	Simple test harness which can be used with beaker

Group:		Applications/Internet
License:	GPLv3+
URL:		https://github.com/p3ck/%{name}
Source0:	https://github.com/p3ck/%{name}/%{name}-%{version}.tar.gz

BuildRequires:	libselinux-static
BuildRequires:	openssl-static
BuildRequires:	glibc-static

%description
restraint harness which can run standalone or with beaker.  when provided a recipe xml it will execute
each task listed in the recipe until done.

%prep
%setup -q


%build
%configure
make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}


%files
%doc



%changelog
* Tue Dec 17 2013 Bill Peck <bpeck@redhat.com> 0.1.2-1
- new package built with tito


