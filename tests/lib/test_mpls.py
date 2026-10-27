# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestMpls(frrtest.TestMultiOut):
    program = "./test_mpls"


TestMpls.exit_cleanly()
