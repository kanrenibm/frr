# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestLogFilter(frrtest.TestMultiOut):
    program = "./test_log_filter"


TestLogFilter.exit_cleanly()
