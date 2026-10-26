# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestHash(frrtest.TestMultiOut):
    program = "./test_hash"


TestHash.exit_cleanly()
