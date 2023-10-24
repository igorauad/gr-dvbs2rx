#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package
'''
This is the GNU Radio DVBS2RX module. Place your Python package
description here (python/__init__.py).
'''
from __future__ import unicode_literals

# import pybind11 generated symbols into the dvbs2rx namespace. Try from
# bindings/ first, which works when importing from the build dir.
try:
    from .bindings.dvbs2rx_python import *  # noqa: F401,F403
except ImportError:
    from .dvbs2rx_python import *  # noqa: F401,F403

# import any pure python here
from .params import *  # noqa: F401, F403
