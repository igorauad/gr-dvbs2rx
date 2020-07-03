INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_DVBS2RX dvbs2rx)

FIND_PATH(
    DVBS2RX_INCLUDE_DIRS
    NAMES dvbs2rx/api.h
    HINTS $ENV{DVBS2RX_DIR}/include
        ${PC_DVBS2RX_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    DVBS2RX_LIBRARIES
    NAMES gnuradio-dvbs2rx
    HINTS $ENV{DVBS2RX_DIR}/lib
        ${PC_DVBS2RX_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/dvbs2rxTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DVBS2RX DEFAULT_MSG DVBS2RX_LIBRARIES DVBS2RX_INCLUDE_DIRS)
MARK_AS_ADVANCED(DVBS2RX_LIBRARIES DVBS2RX_INCLUDE_DIRS)
