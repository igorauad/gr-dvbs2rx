# Copyright 2022 Free Software Foundation, Inc.
#
# This file is part of gr-dvbs2rx.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# ##############################################################################
# Man pages
# ##############################################################################
set(MAN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MAN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(MAN_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/share/man)

list(APPEND manpages dvbs2-tx.1 dvbs2-rx.1)

foreach(manpage ${manpages})
  configure_file(${MAN_SRC_DIR}/${manpage}.in ${MAN_BUILD_DIR}/${manpage} @ONLY)
  install(FILES ${MAN_BUILD_DIR}/${manpage} DESTINATION ${MAN_INSTALL_DIR}/man1)
endforeach(manpage)
