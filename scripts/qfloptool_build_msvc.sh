#!/bin/bash

###################################################################################
# qfloptool_build_msvc.sh - Builds qfloptool under MSVC                           #
###################################################################################

# sanity check
if [ -z "$BASH_SOURCE" ]; then
  echo "Null BASH_SOURCE"
  exit
fi

# toolset is v142 (MSVC2019), but can also be ClangCL
TOOLSET=v142

# config is "Debug" by default, but can also be "Release"
CONFIG=Debug
MSVC_VER=msvc2022

# QT version is 6.4.1 by default
QT_VERSION=6.4.1

# parse arguments
USE_PROFILER=off
while getopts "c:q:t:v:" OPTION; do
   case $OPTION in
      c)
         CONFIG=$OPTARG
		 ;;
      q)
         QT_VERSION=$OPTARG
		 ;;
      t)
         TOOLSET=$OPTARG
		 ;;
      v)
         MSVC_VER=$OPTARG
         ;;
   esac
done

# MSVC versions
case $MSVC_VER in
    msvc2019)
        GENERATOR="Visual Studio 16 2019"
        ;;
    msvc2022)
        GENERATOR="Visual Studio 17 2022"
        ;;
    *)
        echo "Unknown MSVC version $MSVC_VER"
        exit 1
        ;;
esac

# identify directories
QFLOPTOOL_DIR=$(dirname $BASH_SOURCE)/..
QFLOPTOOL_BUILD_DIR=build/$MSVC_VER
QFLOPTOOL_INSTALL_DIR=${QFLOPTOOL_BUILD_DIR}
DEPS_INSTALL_DIR=$(realpath $(dirname $BASH_SOURCE)/../deps/install/$MSVC_VER)

if [ -z "${QT6_INSTALL_DIR:-}" ]; then
  QT6_INSTALL_DIR=/c/Qt/${QT_VERSION}/msvc2019_64
fi
if [ ! -d "$QT6_INSTALL_DIR" ]; then
  echo "Bad QT6_INSTALL_DIR"
  exit
fi

# set up build directory
rm -rf ${QFLOPTOOL_BUILD_DIR}
cmake -S. -B${QFLOPTOOL_BUILD_DIR}										\
	-G"$GENERATOR"	            										\
	-T"$TOOLSET"														\
	-DQt6_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6							\
	-DQt6Core_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Core					\
	-DQt6CoreTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6CoreTools			\
	-DQt6GuiTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6GuiTools			\
	-DQt6Test_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Test					\
	-DQt6Widgets_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6Widgets				\
	-DQt6WidgetsTools_DIR=$QT6_INSTALL_DIR/lib/cmake/Qt6WidgetsTools	\
	-DCMAKE_PREFIX_PATH=$DEPS_INSTALL_DIR								\
	-DCMAKE_LIBRARY_PATH=$DEPS_INSTALL_DIR/lib							\
	-DCMAKE_INCLUDE_PATH=$DEPS_INSTALL_DIR/include						\

# and build!
cmake --build ${QFLOPTOOL_BUILD_DIR} --parallel --config ${CONFIG}
cmake --install ${QFLOPTOOL_INSTALL_DIR} --prefix ${QFLOPTOOL_INSTALL_DIR} --config ${CONFIG}
