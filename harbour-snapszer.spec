Name: harbour-snapszer
Version: 1.1.0
Release: 2
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
Hungarian card game also known as Snapszli or 66, with three- and
four-player Schnapsen variants. Play against the computer or against other
phones in the same Wi-Fi network.

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
cp -a sailfish/*.qml sailfish/qmldir qml-common/*.qml %{buildroot}/usr/share/%{name}/qml/
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
* Fri Sep 11 2026 smatkovi - 1.1.0-2
- Keep the device awake while hosting or joining a LAN game

* Fri Sep 11 2026 smatkovi - 1.1.0-1
- Three- and four-player games: Hungarian hármas and négyes snapszer, Dreierschnapsen, Bauernschnapsen
- LAN tables for up to four devices, empty seats played by the computer, joins over IPv6
- LAN multiplayer against a second phone

* Fri Sep 04 2026 edp17 - 1.0.0-0.4.rc4
- Allow names to be replaced through an empty editing state and enlarge/recenter the Snapszer icon artwork

* Fri Sep 04 2026 edp17 - 1.0.0-0.3.rc3
- Fix Settings page loading, refresh About page and add a Snapszer-specific icon

* Fri Sep 04 2026 edp17 - 1.0.0-0.2.rc2
- Fix Settings page loading and move the player won pile beside the lower hand

* Fri Sep 04 2026 edp17 - 1.0.0-0.1.rc1
- Initial two-player Snapszer RC with Hungarian cards, AI, animations, localization, sandboxing and interrupted-game restore
