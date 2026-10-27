# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestStrlcpy(frrtest.TestMultiOut):
    program = "./test_strlcpy"


TestStrlcpy.exit_cleanly()
