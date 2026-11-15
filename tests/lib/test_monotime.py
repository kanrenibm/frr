# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestMonotime(frrtest.TestMultiOut):
    program = "./test_monotime"


TestMonotime.exit_cleanly()
