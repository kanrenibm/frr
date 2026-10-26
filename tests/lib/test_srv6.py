# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestSrv6(frrtest.TestMultiOut):
    program = "./test_srv6"


TestSrv6.exit_cleanly()
