# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestJhash(frrtest.TestMultiOut):
    program = "./test_jhash"


TestJhash.exit_cleanly()
