# - Find JPEGTURBO
# Find the libjpeg-turbo includes and library
# This module defines
#  JPEGTURBO_INCLUDE_DIR, where to find jpeglib.h and turbojpeg.h, etc.
#  JPEGTURBO_LIBRARIES, the libraries needed to use libjpeg-turbo.
#  JPEGTURBO_FOUND, If false, do not try to use libjpeg-turbo.
# also defined, but not for general use are
#  JPEGTURBO_LIBRARY, where to find the libjpeg-turbo library.

#=============================================================================
# Copyright 2001-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

#FIND_PATH(JPEGTURBO_INCLUDE_DIR turbojpeg.h)

find_path(
    JPEGTURBO_PREFIX
    "include/turbojpeg.h"
    $ENV{JPEGTURBO_HOME}
    $ENV{EXTERNLIBS}/libjpeg-turbo64
    $ENV{EXTERNLIBS}/libjpeg-turbo
    ~/Library/Frameworks
    /Library/Frameworks
    /opt/homebrew/opt/jpeg-turbo # Homebrew on arm64
    /usr/local/opt/jpeg-turbo # Homebrew
    /usr/local
    /usr
    /sw # Fink
    /opt/local # DarwinPorts
    /opt/csw # Blastwave
    /opt
    DOC "JPEGTURBO - Prefix")

find_path(
    JPEGTURBO_INCLUDE_DIR "turbojpeg.h"
    HINTS ${JPEGTURBO_PREFIX}/include
    PATHS $ENV{JPEGTURBO_HOME}/include
          $ENV{EXTERNLIBS}/libjpeg-turbo64/include
          $ENV{EXTERNLIBS}/libjpeg-turbo/include
          ~/Library/Frameworks/include
          /Library/Frameworks/include
          /opt/homebrew/opt/jpeg-turbo/include
          /usr/local/opt/jpeg-turbo/include
          /usr/local/include
          /usr/include
          /sw/include # Fink
          /opt/local/include # DarwinPorts
          /opt/csw/include # Blastwave
          /opt/include
    DOC "JPEGTURBO - Headers")

find_path(
    JPEGTURBO_INCLUDE_DIR_INT "jpegint.h"
    PATHS ${JPEGTURBO_INCLUDE_DIR}
    DOC "JPEGTURBO - Internal Headers")

#FIND_LIBRARY(TURBOJPEG_LIBRARY NAMES turbojpeg)
#FIND_LIBRARY(JPEGTURBO_LIBRARY NAMES jpeg)

find_library(
    JPEGTURBO_LIBRARY
    NAMES jpeg
    HINTS ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
    PATHS $ENV{JPEGTURBO_HOME}
          $ENV{EXTERNLIBS}/libjpeg-turbo64
          $ENV{EXTERNLIBS}/libjpeg-turbo
          ~/Library/Frameworks
          /Library/Frameworks
          /usr/local
          /opt/homebrew/opt/jpeg-turbo
          /usr/local/opt/jpeg-turbo
          /usr
          /sw
          /opt/local
          /opt/csw
          /opt
    PATH_SUFFIXES lib lib64
    DOC "JPEGTURBO - Library")

if(MSVC)
    find_library(
        JPEGTURBO_LIBRARY_DEBUG
        NAMES jpegd
        HINTS ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
        PATHS $ENV{JPEGTURBO_HOME}
              $ENV{EXTERNLIBS}/libjpeg-turbo64
              $ENV{EXTERNLIBS}/libjpeg-turbo
              ~/Library/Frameworks
              /Library/Frameworks
              /usr/local
              /opt/homebrew/opt/jpeg-turbo
              /usr/local/opt/jpeg-turbo
              /usr
              /sw
              /opt/local
              /opt/csw
              /opt
        PATH_SUFFIXES lib lib64
        DOC "JPEGTURBO - Library")
endif()

find_library(
    TURBOJPEG_LIBRARY turbojpeg
    HINTS ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
    PATHS $ENV{JPEGTURBO_HOME}
          $ENV{EXTERNLIBS}/libjpeg-turbo64
          $ENV{EXTERNLIBS}/libjpeg-turbo
          ~/Library/Frameworks
          /Library/Frameworks
          /usr/local
          /opt/homebrew/opt/jpeg-turbo
          /usr/local/opt/jpeg-turbo
          /usr
          /sw
          /opt/local
          /opt/csw
          /opt
    PATH_SUFFIXES lib lib64
    DOC "JPEGTURBO - Library")

find_library(
    TURBOJPEG_LIBRARY_STATIC
    NAMES libturbojpeg.a
    HINTS ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
    PATHS $ENV{JPEGTURBO_HOME}
          $ENV{EXTERNLIBS}/libjpeg-turbo64
          $ENV{EXTERNLIBS}/libjpeg-turbo
          ~/Library/Frameworks
          /Library/Frameworks
          /usr/local
          /opt/homebrew/opt/jpeg-turbo
          /usr/local/opt/jpeg-turbo
          /usr
          /sw
          /opt/local
          /opt/csw
          /opt
    PATH_SUFFIXES lib lib64
    DOC "JPEGTURBO - Static library")

if(MSVC)
    find_library(
        TURBOJPEG_LIBRARY_DEBUG turbojpegd
        HINTS ${JPEGTURBO_PREFIX}/debug/lib ${JPEGTURBO_PREFIX}/debug/lib64 ${JPEGTURBO_PREFIX}/lib ${JPEGTURBO_PREFIX}/lib64
        PATHS $ENV{JPEGTURBO_HOME}
              $ENV{EXTERNLIBS}/libjpeg-turbo64
              $ENV{EXTERNLIBS}/libjpeg-turbo
              ~/Library/Frameworks
              /Library/Frameworks
              /usr/local
              /opt/homebrew/opt/jpeg-turbo
              /usr/local/opt/jpeg-turbo
              /usr
              /sw
              /opt/local
              /opt/csw
              /opt
        PATH_SUFFIXES debug/lib debug/lib64 lib lib64
        DOC "JPEGTURBO - Library")
endif()

# handle the QUIETLY and REQUIRED arguments and set JPEGTURBO_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
if(MSVC)
    find_package_handle_standard_args(
        JPEGTURBO
        DEFAULT_MSG
        JPEGTURBO_INCLUDE_DIR
        TURBOJPEG_LIBRARY
        TURBOJPEG_LIBRARY_DEBUG
        JPEGTURBO_LIBRARY
        JPEGTURBO_LIBRARY_DEBUG)
else()
    find_package_handle_standard_args(JPEGTURBO DEFAULT_MSG TURBOJPEG_LIBRARY JPEGTURBO_LIBRARY JPEGTURBO_INCLUDE_DIR)
endif()

if(JPEGTURBO_FOUND)
    if(MSVC)
        set(JPEGTURBO_LIBRARIES optimized ${JPEGTURBO_LIBRARY} debug ${JPEGTURBO_LIBRARY_DEBUG})
        set(TURBOJPEG_LIBRARIES optimized ${TURBOJPEG_LIBRARY} debug ${TURBOJPEG_LIBRARY_DEBUG})
    else()
        set(JPEGTURBO_LIBRARIES ${JPEGTURBO_LIBRARY})
        set(TURBOJPEG_LIBRARIES ${TURBOJPEG_LIBRARY})
    endif()

    include(CheckSymbolExists)
    set(CMAKE_REQUIRED_INCLUDES ${JPEGTURBO_INCLUDE_DIR})
    check_symbol_exists(tjMCUWidth "turbojpeg.h" TURBOJPEG_HAVE_TJMCUWIDTH)

    if(JPEGTURBO_INCLUDE_DIR_INT)
        set(TURBOJPEG_HAVE_INTERNAL TRUE)
    else()
        set(TURBOJPEG_HAVE_INTERNAL FALSE)
    endif()
endif(JPEGTURBO_FOUND)

mark_as_advanced(TURBOJPEG_LIBRARY JPEGTURBO_LIBRARY JPEGTURBO_INCLUDE_DIR TURBOJPEG_LIBRARY_STATIC TURBOJPEG_HAVE_TJMCUWIDTH TURBOJPEG_HAVE_INTERNAL)
