# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestFrrstr(frrtest.TestMultiOut):
    program = "./test_frrstr"


TestFrrstr.exit_cleanly()
