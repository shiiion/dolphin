#!/bin/bash

set -e

export MACOSX_DEPLOYMENT_TARGET=10.15
INSTALLDIR="$HOME/deps"
NPROCS="$(getconf _NPROCESSORS_ONLN)"
SDL=SDL2-2.30.9
QT=6.2.10
QT_SUFFIX=-opensource

mkdir deps-build
cd deps-build

export PKG_CONFIG_PATH="$INSTALLDIR/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$INSTALLDIR/lib -dead_strip $LDFLAGS"
export CFLAGS="-I$INSTALLDIR/include -Os $CFLAGS"
export CXXFLAGS="-I$INSTALLDIR/include -Os $CXXFLAGS"

cat > SHASUMS <<EOF
24b574f71c87a763f50704bbb630cbe38298d544a1f890f099a4696b1d6beba4  $SDL.tar.gz
efbeff5ad6f4d46e82734a681909892401688432fd7ef02c63d2083304d8265c  qtbase-everywhere$QT_SUFFIX-src-$QT.tar.xz
5e04e4b7699d837c52641310ca386373801a24e8924de7ffcc4b84890431eb38  qtsvg-everywhere$QT_SUFFIX-src-$QT.tar.xz
62809b242ebcb0e65ac6738f76d005d24c352a8b813128fa3772906ca50cf980  qttools-everywhere$QT_SUFFIX-src-$QT.tar.xz
57e7cb80d31c32ccbb00bbd7da170970b9effb992970aaf687d4524117aca41e  qttranslations-everywhere$QT_SUFFIX-src-$QT.tar.xz
EOF

curl -L \
	-O "https://libsdl.org/release/$SDL.tar.gz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtbase-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qtsvg-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttools-everywhere$QT_SUFFIX-src-$QT.tar.xz" \
	-O "https://download.qt.io/archive/qt/${QT%.*}/$QT/submodules/qttranslations-everywhere$QT_SUFFIX-src-$QT.tar.xz" \

shasum -a 256 --check SHASUMS

echo "Installing SDL..."
tar xf "$SDL.tar.gz"
cd "$SDL"
CC="clang -arch x86_64 -arch arm64" ./configure --prefix "$INSTALLDIR" --without-x
make "-j$NPROCS"
make install
cd ..

echo "Installing Qt Base..."
tar xf "qtbase-everywhere$QT_SUFFIX-src-$QT.tar.xz"
cd "qtbase-everywhere-src-$QT"
cmake -B build -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_optimize_size=ON -DFEATURE_dbus=OFF -DFEATURE_framework=OFF -DFEATURE_icu=OFF -DFEATURE_opengl=OFF -DFEATURE_printsupport=OFF -DFEATURE_sql=OFF -DFEATURE_gssapi=OFF -DFEATURE_system_png=OFF -DFEATURE_system_jpeg=OFF -DCMAKE_MESSAGE_LOG_LEVEL=STATUS
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt SVG..."
tar xf "qtsvg-everywhere$QT_SUFFIX-src-$QT.tar.xz"
cd "qtsvg-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt Tools..."
tar xf "qttools-everywhere$QT_SUFFIX-src-$QT.tar.xz"
cd "qttools-everywhere-src-$QT"
# Linguist relies on a library in the Designer target, which takes 5-7 minutes to build on the CI
# Avoid it by not building Linguist, since we only need the tools that come with it
patch -u src/linguist/CMakeLists.txt <<EOF
--- src/linguist/CMakeLists.txt
+++ src/linguist/CMakeLists.txt
@@ -14,7 +14,7 @@
 add_subdirectory(lrelease-pro)
 add_subdirectory(lupdate)
 add_subdirectory(lupdate-pro)
-if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND NOT no-png)
+if(QT_FEATURE_process AND QT_FEATURE_pushbutton AND QT_FEATURE_toolbutton AND TARGET Qt::Widgets AND TARGET Qt::PrintSupport AND NOT no-png)
     add_subdirectory(linguist)
 endif()
EOF
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release -DFEATURE_assistant=OFF -DFEATURE_clang=OFF -DFEATURE_designer=OFF -DFEATURE_kmap2qmap=OFF -DFEATURE_pixeltool=OFF -DFEATURE_pkg_config=OFF -DFEATURE_qev=OFF -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtdiag=OFF -DFEATURE_qtplugininfo=OFF
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Installing Qt Translations..."
tar xf "qttranslations-everywhere$QT_SUFFIX-src-$QT.tar.xz"
cd "qttranslations-everywhere-src-$QT"
cmake -B build -DCMAKE_PREFIX_PATH="$INSTALLDIR" -DCMAKE_INSTALL_PREFIX="$INSTALLDIR" -DCMAKE_BUILD_TYPE=Release
make -C build "-j$NPROCS"
make -C build install
cd ..

echo "Cleaning up..."
cd ..
rm -r deps-build
