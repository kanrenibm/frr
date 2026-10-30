# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestFerr(frrtest.TestMultiOut):
    program = "./test_ferr"


TestFerr.exit_cleanly()
