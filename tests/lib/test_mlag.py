# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestMlag(frrtest.TestMultiOut):
    program = "./test_mlag"


TestMlag.exit_cleanly()
