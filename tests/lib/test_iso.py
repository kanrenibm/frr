# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestIso(frrtest.TestMultiOut):
    program = "./test_iso"


TestIso.exit_cleanly()
