# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestFlexAlgo(frrtest.TestMultiOut):
    program = "./test_flex_algo"


TestFlexAlgo.exit_cleanly()
