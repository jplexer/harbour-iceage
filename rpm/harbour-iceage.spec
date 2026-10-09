Name:       harbour-iceage

Summary:    Ice Age
Version:    0.1
Release:    1
License:    MIT
URL:        https://funnyna.me
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   nemo-qml-plugin-notifications-qt5
Requires:   nemo-qml-plugin-dbus-qt5
Requires:   qt5-qtdeclarative-import-multimedia
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(sailfishsecrets)
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
A Bluesky client for Sailfish

%if 0%{?_chum}
Title: Ice Age
Type: desktop-application
DeveloperName: jplexer
Categories:
 - Network
Custom:
  Repo: https://github.com/jplexer/harbour-iceage
PackageIcon: https://github.com/jplexer/harbour-iceage/raw/main/icons/harbour-iceage.svg
Links:
  Homepage: https://funnyna.me
  Bugtracker: https://github.com/jplexer/harbour-iceage/issues
AIRating: C
%endif


%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 

%make_build


%install
%qmake5_install


desktop-file-install --delete-original         --dir %{buildroot}%{_datadir}/applications                %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
