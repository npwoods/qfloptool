#!/bin/bash

###################################################################################
# qfloptool_build_msys2.sh - Builds qfloptool under MSYS2                         #
###################################################################################

# Sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# Identify directories
QFLOPTOOL_DIR=$(dirname $BASH_SOURCE)/..
QFLOPTOOL_BUILD_DIR=build/msys2
QFLOPTOOL_INSTALL_DIR=${QFLOPTOOL_BUILD_DIR}
DEPS_INSTALL_DIR=$(dirname $BASH_SOURCE)/../deps/install/msys2
QT_INSTALL_DIR=C:/msys64/mingw64/qt6-static

# parse arguments
USE_PROFILER=off
BUILD_TYPE=Release					# can be Release/Debug/RelWithDebInfo etc
while getopts "b:" OPTION; do
   case $OPTION in
      b)
         BUILD_TYPE=$OPTARG
         ;;
   esac
done

# set up build directory
rm -rf ${QFLOPTOOL_BUILD_DIR}
echo "Build Type: $BUILD_TYPE"
cmake -S. -B${QFLOPTOOL_BUILD_DIR}								\
	-DUSE_SHARED_LIBS=off										\
	-DCMAKE_BUILD_TYPE=${BUILD_TYPE}							\
	-DCMAKE_PREFIX_PATH="${DEPS_INSTALL_DIR};${QT_INSTALL_DIR}"	\

# and build!
cmake --build ${QFLOPTOOL_BUILD_DIR} --parallel
cmake --install ${QFLOPTOOL_BUILD_DIR} --strip --prefix ${QFLOPTOOL_INSTALL_DIR}
