Name:       fpolicyd
Version:    1.0
Release:    1
Summary:    A Policy File Server for Flash Player

Group:      Application/Server
License:    GPL
Source0:    https://github.com/51web/fpolicyd/release/%{name}-%{version}.tar.gz
URL:        https://github.com/51web/fpolicyd
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires: libev-devel
Requires: libev
Requires(post): chkconfig

%description
fpolicyd is a Policy File Server for Flash Player

%prep
%setup -q

%build
make

%install
rm -rf %{buildroot}
install -m 755 -Dp fpolicyd %{buildroot}%{_sbindir}/fpolicyd
install -m 644 -Dp fpolicy.xml %{buildroot}%{_sysconfdir}/fpolicy.xml
install -m 755 -Dp fpolicyd.init %{buildroot}%{_initrddir}/fpolicyd
install -m 644 -Dp fpolicyd.logrotate %{buildroot}%{_sysconfdir}/logrotate.d/fpolicyd

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_sbindir}/fpolicyd
%{_initrddir}/fpolicyd
%config(noreplace) %{_sysconfdir}/fpolicy.xml
%config(noreplace) %{_sysconfdir}/logrotate.d/fpolicyd

%post
if [ $1 -eq 1 ]; then
    /sbin/chkconfig --add fpolicyd
fi

%preun
if [ $1 -eq 0 ]; then
    /sbin/service fpolicyd stop > /dev/null 2>&1
    /sbin/chkconfig --del fpolicyd
fi

%changelog
* Wed Aug 05 2015 Itxx00 <itxx00@gmail.com> - 1.0-1
- Initial build
