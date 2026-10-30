# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestPtmLib(frrtest.TestMultiOut):
    program = "./test_ptm_lib"


TestPtmLib.exit_cleanly()
