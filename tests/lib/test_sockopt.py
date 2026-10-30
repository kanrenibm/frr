# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestSockopt(frrtest.TestMultiOut):
    program = "./test_sockopt"


TestSockopt.exit_cleanly()
