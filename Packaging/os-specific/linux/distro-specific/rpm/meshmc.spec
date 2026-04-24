%bcond_with toolchain_clang
%bcond_with toolchain_gcc

%if %{with toolchain_gcc}
%global toolchain gcc
%else
%global toolchain clang
%endif

# Change these variables if you want to use custom keys
# Leave blank if you want to build MeshMC without an MSA ID or CurseForge API key
%global msa_id default
%global curseforge_key default

# Set the Qt version
%global qt_version 6
%global min_qt_version 6.8

# Give the launcher our build platform
%global build_platform unknown

%if 0%{?fedora}
%global build_platform Fedora
%endif

%if 0%{?rhel}
%global build_platform RedHat
%endif

%if 0%{?centos}
%global build_platform CentOS
%endif

%global snapshot 202604221645

Name:           meshmc
Version:        %{snapshot}
Release:        4%{?dist}
Summary:        Custom Minecraft launcher with multi-instance management
Group:          Amusements/Games
License:        GPL-3.0-or-later AND Apache-2.0 AND LGPL-3.0-only AND LGPL-2.1 AND OFL-1.1 AND MIT
URL:            https://projecttick.org/
Source0:        https://ftp.projecttick.org/Project-Tick/meshmc/releases/download/v%{snapshot}/meshmc-v%{snapshot}.tar.gz
Patch0:         patch.patch

%if "%{toolchain}" == "gcc"
BuildRequires:    gcc-c++
%endif
%if "%{toolchain}" == "clang"
BuildRequires:    clang
BuildRequires:    lld
%endif

%if 0%{?fedora} > 41
BuildRequires:    temurin-17-jdk
%else
BuildRequires:    java-17-openjdk-devel
%endif

BuildRequires:    cmake >= 3.22
BuildRequires:    ninja-build
BuildRequires:    extra-cmake-modules

BuildRequires:    cmake(VulkanHeaders)
BuildRequires:    pkgconfig(libarchive)
BuildRequires:    pkgconfig(libcmark)
# https://bugzilla.redhat.com/show_bug.cgi?id=2166815
# Fedora versions < 38 (and thus RHEL < 10) don't contain cmark's binary target
# We need that
%if 0%{?fedora} && 0%{?fedora} < 38 || 0%{?rhel} && 0%{?rhel} < 10
BuildRequires:    cmark
%endif
BuildRequires:    pkgconfig(libqrencode)
BuildRequires:    pkgconfig(scdoc)
BuildRequires:    pkgconfig(tomlplusplus)
BuildRequires:    pkgconfig(zlib)

BuildRequires:    cmake(Qt%{qt_version}Concurrent) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Core) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}CoreTools) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Network) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}NetworkAuth) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}OpenGL) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Test) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Widgets) >= %{min_qt_version}
BuildRequires:    cmake(Qt%{qt_version}Xml) >= %{min_qt_version}

BuildRequires:    desktop-file-utils
BuildRequires:    libappstream-glib

Requires:         qt%{qt_version}-qtimageformats
Requires:         qt%{qt_version}-qtsvg

Requires:         javapackages-filesystem
Recommends:       java-25-openjdk
Recommends:       java-21-openjdk
# See note above
%if 0%{?fedora} && 0%{?fedora} < 42
Recommends:       java-17-openjdk
Suggests:         java-1.8.0-openjdk
%endif

# Used to gather GPU with `lspci`
Requires:         pciutils
# Needed for LWJGL [2.9.2, 3) https://github.com/LWJGL/lwjgl/issues/128
Recommends:       xrandr
# Needed for using narrator in minecraft
Recommends:       flite

%description
A custom launcher for Minecraft that allows you to easily manage
multiple installations of Minecraft at once (Fork of MultiMC)

%prep
%autosetup -n meshmc-v%{snapshot} -p1

%build
%cmake -G Ninja \
    %if "%{toolchain}" == "clang"
    -D CMAKE_LINKER_TYPE=LLD \
    %endif
    -D MeshMC_QT_VERSION_MAJOR="%{qt_version}" \
    -D MeshMC_BUILD_PLATFORM="%{build_platform}" \
    -D MeshMC_PLUGINS=ON \
    -D MeshMC_STAGING_PLUGINS=ON \
    -W no-dev \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    %if 0%{?fedora} < 41
    -D MeshMC_DISABLE_JAVA_DOWNLOADER=ON \
    %endif
    %if "%{msa_id}" != "default"
    -D MeshMC_MICROSOFT_CLIENT_ID="%{msa_id}" \
    %endif
    %if "%{curseforge_key}" != "default"
    -D MeshMC_CURSEFORGE_API_KEY="%{curseforge_key}" \
    %endif
%cmake_build

%install
%cmake_install

%check
%ctest
desktop-file-validate %{buildroot}%{_datadir}/applications/org.projecttick.MeshMC.desktop
%if 0%{?fedora} > 37 || 0%{?rhel} > 9
appstream-util validate-relax --nonet \
  %{buildroot}%{_metainfodir}/org.projecttick.MeshMC.metainfo.xml
%endif

%files
%license COPYING.md
%doc README.md
%{_bindir}/mmcmodules/
%{_bindir}/meshmc
%{_bindir}/meshmc-crashreporter
%{_datadir}/applications/org.projecttick.MeshMC.desktop
%{_datadir}/metainfo/org.projecttick.MeshMC.metainfo.xml
%{_datadir}/mime/packages/org.projecttick.MeshMC.xml
%{_datadir}/icons/hicolor/*/apps/org.projecttick.MeshMC.*
%{_datadir}/MeshMC/
%{_libdir}/libKatabasis.so
%{_libdir}/libLocalPeer.so
%{_libdir}/libclassparser.so
%{_libdir}/libganalytics.so
%{_libdir}/libiconfix.so
%{_libdir}/libnbt++.so.3*
%{_libdir}/libneozip.so.2*
%{_libdir}/libnbt++.so
%{_libdir}/libneozip.so
%{_libdir}/libneozip.so.10.1.0
%{_libdir}/librainbow.so
%{_libdir}/libsysteminfo.so
%{_libdir}/libxz-embedded.so
%{_mandir}/man6/*

%package devel
Summary: Development files for MeshMC
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Development files for building against MeshMC and its bundled libraries.

%files devel
%{_includedir}/LocalPeer/
%{_includedir}/classparser/
%{_includedir}/ganalytics/
%{_includedir}/iconfix/
%{_includedir}/katabasis/
%{_includedir}/meshmc-sdk/
%{_includedir}/nbt++/
%{_includedir}/optional-bare/
%{_includedir}/rainbow/
%{_includedir}/systeminfo/
%{_includedir}/xz-embedded/
%{_includedir}/neozip.h
%{_includedir}/zconf-ng.h
%{_includedir}/zlib_name_mangling-ng.h

/usr/lib/cmake/MeshMC_SDK/
%{_libdir}/cmake/JavaCheck/
%{_libdir}/cmake/Katabasis/
%{_libdir}/cmake/LocalPeer/
%{_libdir}/cmake/NewLaunch/
%{_libdir}/cmake/classparser/
%{_libdir}/cmake/ganalytics/
%{_libdir}/cmake/iconfix/
%{_libdir}/cmake/nbt++/
%{_libdir}/cmake/neozip/
%{_libdir}/cmake/optional-bare/
%{_libdir}/cmake/rainbow/
%{_libdir}/cmake/systeminfo/
%{_libdir}/cmake/xz-embedded/

%{_libdir}/pkgconfig/neozip.pc

%changelog
* Mon Apr 20 2026 Mehmet Samet Duman <yongdohyun@projecttick.org> - %{version}-1
- Initial Fedora draft
