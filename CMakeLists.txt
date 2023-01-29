cmake_minimum_required(VERSION 3.15)

project(qfloptool LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# By default, qfloptool should build linking to shared libraries; its the norm on Linux and it
# is desirable in debugging scenarios.  But for when performing releases on platforms where such
# releases may entail distributing large Qt libraries (Windows and probably macOS), building linking
# to Qt and QuaZip statically is desirable to minimize dependency issues
option(USE_SHARED_LIBS "Use shared libraries" ON)
if (NOT USE_SHARED_LIBS)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ ${CMAKE_CXX_STANDARD_LIBRARIES} -lstdc++ -lpthread")
add_definitions(-DQUAZIP_STATIC)
endif()


#############################################################################
# General Dependencies                                                      #
#############################################################################

# Expat
include(FindEXPAT)
find_package(EXPAT REQUIRED)
include_directories(${EXPAT_INCLUDE_DIR})

# ZLib
include(FindZLIB)
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

# Qt
find_package(Qt6 COMPONENTS Widgets LinguistTools REQUIRED)
if (NOT USE_SHARED_LIBS)
set(QT_STATIC_LIBRARIES bz2 harfbuzz brotlidec brotlicommon graphite2 ${Qt6_DIR}/../../libQt6Core5Compat.a)
if(WIN32)
set(QT_STATIC_LIBRARIES ${QT_STATIC_LIBRARIES} rpcrt4)
endif()
endif()


#############################################################################
# MAME dependencies                                                         #
#############################################################################

# MAME compile definitions
if(WIN32)
set(MAME_COMPILE_DEFS CRLF=3 OSD_WINDOWS WIN32_LEAN_AND_MEAN NOMINMAX)
else()
set(MAME_COMPILE_DEFS CRLF=2)
endif()
if("${CMAKE_CXX_BYTE_ORDER}" STREQUAL LITTLE_ENDIAN)
list(APPEND MAME_COMPILE_DEFS LSB_FIRST)
endif()

# appeasing MSVC
if(MSVC)
list(APPEND MAME_COMPILE_DEFS _CRT_SECURE_NO_WARNINGS)					# MSVC specific defs
add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=1>)		# debug iterators slow down src/mame/formats
endif()

# src/lib/util from MAME
file(GLOB MAME_SRC_LIB_UTIL_CPP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} deps/mame/src/lib/util/*.cpp deps/mame/src/osd/osdcore.cpp deps/mame/src/osd/strconv.cpp)
file(GLOB MAME_SRC_LIB_UTIL_H RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} deps/mame/src/lib/util/*.h deps/mame/src/osd/osdcore.h deps/mame/src/osd/strconv.h)
set_property(SOURCE ${MAME_SRC_LIB_UTIL_CPP} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${MAME_SRC_LIB_UTIL_H} PROPERTY SKIP_AUTOGEN ON)
add_library(mame_lib_util ${MAME_SRC_LIB_UTIL_CPP} ${MAME_SRC_LIB_UTIL_H})
target_include_directories(mame_lib_util PRIVATE deps/mame/src/lib/util deps/mame/src/osd deps/mame/3rdparty/libflac/include deps/mame/3rdparty deps/mame/3rdparty/libflac/include deps/mame/3rdparty/utf8proc)
target_compile_definitions(mame_lib_util PRIVATE ${MAME_COMPILE_DEFS})

# src/lib/formats from MAME
file(GLOB MAME_SRC_LIB_FORMATS_CPP deps/mame/src/lib/formats/*.cpp)
file(GLOB MAME_SRC_LIB_FORMATS_H deps/mame/src/lib/formats/*.h)
set_property(SOURCE ${MAME_SRC_LIB_FORMATS_CPP} PROPERTY SKIP_AUTOGEN ON)
set_property(SOURCE ${MAME_SRC_LIB_FORMATS_H} PROPERTY SKIP_AUTOGEN ON)
add_library(mame_lib_formats ${MAME_SRC_LIB_FORMATS_CPP} ${MAME_SRC_LIB_FORMATS_H}  src/mame_generated/has_formats.h)
target_include_directories(mame_lib_formats PRIVATE deps/mame/src/lib deps/mame/src/lib/formats deps/mame/src/lib/util deps/mame/src/osd src/mame_generated)
target_compile_definitions(mame_lib_formats PRIVATE ${MAME_COMPILE_DEFS})

# optimizing MAME code to make debugging a bit less painful
if(MSVC)
target_compile_options(mame_lib_util PRIVATE "$<$<CONFIG:DEBUG>:/O2 /Ob2>")
target_compile_options(mame_lib_formats PRIVATE "$<$<CONFIG:DEBUG>:/O2 /Ob2>")
endif()


#############################################################################
# qfloptool executable                                                      #
#############################################################################

add_executable(qfloptool
  src/floptool.cpp
  src/floptool.h
  src/imageitemmodel.cpp
  src/imageitemmodel.h
  src/main.cpp
  src/mainwindow.cpp
  src/mainwindow.h
  src/mainwindow.ui
  src/resources.qrc
  src/utility.cpp
  src/utility.h
  src/dialogs/identify.cpp
  src/dialogs/identify.h
  src/dialogs/identify.ui
)

target_include_directories(qfloptool PRIVATE deps/mame/src/lib deps/mame/src/lib/formats deps/mame/src/lib/util deps/mame/src/osd src/mame_generated)
target_link_libraries(qfloptool PRIVATE mame_lib_formats mame_lib_util Qt6::Widgets ${EXPAT_LIBRARIES} ${ZLIB_LIBRARIES} ${QT_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
qt_import_plugins(qfloptool INCLUDE_BY_TYPE imageformats Qt::QGifPlugin Qt::QJpegPlugin Qt::QICOPlugin EXCLUDE_BY_TYPE sqldrivers)
