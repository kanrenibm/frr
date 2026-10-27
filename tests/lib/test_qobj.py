# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestQobj(frrtest.TestMultiOut):
    program = "./test_qobj"


TestQobj.exit_cleanly()
