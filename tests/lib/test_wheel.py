# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestWheel(frrtest.TestMultiOut):
    program = "./test_wheel"


TestWheel.exit_cleanly()
