# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestAsn(frrtest.TestMultiOut):
    program = "./test_asn"


TestAsn.exit_cleanly()
