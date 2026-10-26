# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestBitfield(frrtest.TestMultiOut):
    program = "./test_bitfield"


TestBitfield.exit_cleanly()
