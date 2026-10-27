# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestCsv(frrtest.TestMultiOut):
    program = "./test_csv"


TestCsv.exit_cleanly()
