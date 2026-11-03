# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestSpfBackoff(frrtest.TestMultiOut):
    program = "./test_spf_backoff"


TestSpfBackoff.exit_cleanly()
