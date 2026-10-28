# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestHook(frrtest.TestMultiOut):
    program = "./test_hook"


TestHook.exit_cleanly()
