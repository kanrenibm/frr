# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestVector(frrtest.TestMultiOut):
    program = "./test_vector"


TestVector.exit_cleanly()
