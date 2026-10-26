# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestLinklist(frrtest.TestMultiOut):
    program = "./test_linklist"


TestLinklist.exit_cleanly()
