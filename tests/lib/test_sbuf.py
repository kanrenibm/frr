# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestSbuf(frrtest.TestMultiOut):
    program = "./test_sbuf"


TestSbuf.exit_cleanly()
