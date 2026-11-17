# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestPullwr(frrtest.TestMultiOut):
    program = "./test_pullwr"


TestPullwr.exit_cleanly()
