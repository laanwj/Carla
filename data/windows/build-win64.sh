#!/bin/bash

set -e

MINGW=x86_64-w64-mingw32
MINGW_PATH=/opt/mingw64

JOBS="-j 8"

if [ ! -f Makefile ]; then
  cd ../..
fi

export WIN32=true
export WIN64=true

export PATH=$MINGW_PATH/bin:$MINGW_PATH/$MINGW/bin:$PATH
export CC=$MINGW-gcc
export CXX=$MINGW-g++
export WINDRES=$MINGW-windres

export CFLAGS=-DPTW32_STATIC_LIB
export CXXFLAGS=-DPTW32_STATIC_LIB

export WINEARCH=win64
export WINEPREFIX=~/.winepy3_x64
export PYTHON_EXE="wine C:\\\\Python34\\\\python.exe"

export CXFREEZE="$PYTHON_EXE C:\\\\Python34\\\\Scripts\\\\cxfreeze"
export PYUIC="$PYTHON_EXE -m PyQt5.uic.pyuic"
export PYRCC="wine C:\\\\Python34\\\\Lib\\\\site-packages\\\\PyQt5\\\\pyrcc5.exe"

export DEFAULT_QT=5

# Clean build
make clean

# Build PyQt5 resources
make $JOBS UI RES WIDGETS

# FIXME: linuxsampler fails to link on 64bit

# Build discovery
make $JOBS discovery HAVE_LINUXSAMPLER=false
mv bin/carla-discovery-native.exe bin/carla-discovery-win64.exe

# Build backend
make $JOBS backend HAVE_LINUXSAMPLER=false

# Build Plugin bridges
# make $JOBS bridges

# Build UI bridges
# make $JOBS -C source/bridges ui_lv2-win32 ui_vst-hwnd

export PYTHONPATH=`pwd`/source

rm -rf ./data/windows/Carla
cp ./source/carla ./source/carla.pyw
$PYTHON_EXE ./data/windows/app.py build_exe
rm -f ./source/carla.pyw
mv build data/windows/Carla

cd data/windows/
cp ../../bin/*.dll Carla/
cp ../../bin/*.exe Carla/
mv Carla/exe.*/* Carla/
rmdir Carla/exe.*

rm -f Carla/PyQt5.Qsci.pyd Carla/PyQt5.QtNetwork.pyd Carla/PyQt5.QtSql.pyd Carla/PyQt5.QtTest.pyd

cp $WINEPREFIX/drive_c/Python34/python34.dll                                Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/icudt49.dll         Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/icuin49.dll         Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/icuuc49.dll         Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/libEGL.dll          Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/libGLESv2.dll       Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5Core.dll         Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5Gui.dll          Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5Widgets.dll      Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5OpenGL.dll       Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5PrintSupport.dll Carla/
cp $WINEPREFIX/drive_c/Python34/Lib/site-packages/PyQt5/Qt5Svg.dll          Carla/

# Build unzipfx
make -C unzipfx-carla -f Makefile.win32

# Create static build
rm -f Carla.zip CarlaControl.zip
zip -r -9 Carla.zip Carla

rm -f Carla.exe CarlaControl.exe
cat unzipfx-carla/unzipfx2cat.exe Carla.zip > Carla.exe
chmod +x Carla.exe

# Cleanup
make -C unzipfx-carla -f Makefile.win32 clean
make -C unzipfx-carla-control -f Makefile.win32 clean
rm -f Carla.zip CarlaControl.zip
rm -f unzipfx-*/*.exe

cd ../..

# Testing:
echo "export WINEPREFIX=~/.winepy3_x64"
echo "$PYTHON_EXE ./source/carla -platformpluginpath \"C:\\\\Python34\\\\Lib\\\\site-packages\\\\PyQt5\\\\plugins\\\\platforms\""
