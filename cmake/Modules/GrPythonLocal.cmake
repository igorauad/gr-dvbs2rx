# Copyright 2022 Free Software Foundation, Inc.
#
# This file is part of gr-dvbs2rx
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(DEFINED __INCLUDED_GR_PYTHON_LOCAL_CMAKE)
    return()
endif()
set(__INCLUDED_GR_PYTHON_LOCAL_CMAKE TRUE)

# FIXME GR 3.10 updated the GR_PYTHON_DIR implementation from the GrPython cmake
# module to replace the get_python_lib function from the deprecated distutils
# package. However, the new implementation (without distutils) is not equivalent
# and does not work when building Debian packages. It installs the dvbs2rx
# package at "/usr/lib/python3.x/site-packages/gnuradio/dvbs2rx/" while gnuradio
# from the deb package is at "/usr/lib/python3/dist-packages/gnuradio/". That's
# because the new implementation falls back to "sysconfig.get_path('platlib')"
# when multiple paths returned by site.getsitepackages() match with the current
# prefix. Since deb packages use /usr as prefix, usually all results from
# site.getsitepackages() match and none are used, so the function proceeds to
# using sysconfig.get_path('platlib'), which uses the site-packages dir. In
# contrast, the old implementation used the result from get_python_lib directly.
# As a workaround, use the old implementation based on distutils for now:
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import os
import sysconfig
import site
from distutils.sysconfig import get_python_lib

prefix = '${CMAKE_INSTALL_PREFIX}'

# ask distutils where to install the python module
install_dir = get_python_lib(plat_specific=True, prefix=prefix)

# use sites when the prefix is already recognized
try:
  paths = [p for p in site.getsitepackages() if p.startswith(prefix)]
  if len(paths) == 1: install_dir = paths[0]
except AttributeError: pass

# strip the prefix to return a relative path
print(os.path.relpath(install_dir, prefix))"
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE GR_PYTHON_DIR
)