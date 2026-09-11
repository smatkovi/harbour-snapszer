Name: harbour-snapszer
Version: 1.0.0
Release: 0.6.0
Summary: Classic Hungarian Snapszer card game
License: MIT
URL: https://github.com/edp17/harbour-snapszer
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

Requires:       sailfishsilica-qt5
BuildRequires:  pkgconfig(sailfishapp)
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  qt5-qttools-linguist

%description
Snapszer is a native Sailfish OS implementation of the classic two-player
Hungarian card game also known as Snapszli or 66. Play against the AI or
against another phone in the same Wi-Fi network.

%prep
%setup -q

%build
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/bin
install -m 755 build/harbour-snapszer %{buildroot}/usr/bin/

mkdir -p %{buildroot}/usr/share/%{name}/qml
cp -a sailfish/*.qml %{buildroot}/usr/share/%{name}/qml/
cp -a sailfish/icons %{buildroot}/usr/share/%{name}/qml/

mkdir -p %{buildroot}/usr/share/%{name}/images
cp -a images/cards %{buildroot}/usr/share/%{name}/images/

mkdir -p %{buildroot}/usr/share/%{name}/translations
install -m 644 build/harbour-snapszer-hu.qm %{buildroot}/usr/share/%{name}/translations/

mkdir -p %{buildroot}/usr/share/applications
install -m 644 sailfish/desktop/%{name}.desktop %{buildroot}/usr/share/applications/%{name}.desktop

mkdir -p %{buildroot}/usr/share/icons/hicolor/256x256/apps
install -m 644 sailfish/icons/icon-256.png %{buildroot}/usr/share/icons/hicolor/256x256/apps/%{name}.png

mkdir -p %{buildroot}/usr/share/doc/%{name}
install -m 644 README_Sailfish.md %{buildroot}/usr/share/doc/%{name}/
mkdir -p %{buildroot}/usr/share/licenses/%{name}
install -m 644 LICENSE %{buildroot}/usr/share/licenses/%{name}/

%files
%defattr(-,root,root,-)
/usr/bin/%{name}
/usr/share/%{name}
/usr/share/icons/hicolor/256x256/apps/%{name}.png
/usr/share/applications/%{name}.desktop
/usr/share/doc/%{name}
/usr/share/licenses/%{name}

%changelog
* Fri Sep 11 2026 smatkovi - 1.0.0-0.6.0
- Add LAN multiplayer against a second phone running Snapszer, with automatic discovery

* Fri Sep 04 2026 edp17 - 1.0.0-0.4.rc4
- Allow names to be replaced through an empty editing state and enlarge/recenter the Snapszer icon artwork

* Fri Sep 04 2026 edp17 - 1.0.0-0.3.rc3
- Fix Settings page loading, refresh About page and add a Snapszer-specific icon

* Fri Sep 04 2026 edp17 - 1.0.0-0.2.rc2
- Fix Settings page loading and move the player won pile beside the lower hand

* Fri Sep 04 2026 edp17 - 1.0.0-0.1.rc1
- Initial two-player Snapszer RC with Hungarian cards, AI, animations, localization, sandboxing and interrupted-game restore
