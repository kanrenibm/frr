# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestDigest(frrtest.TestMultiOut):
    program = "./test_digest"


TestDigest.exit_cleanly()
