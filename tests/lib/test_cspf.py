# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestCspf(frrtest.TestMultiOut):
    program = "./test_cspf"


TestCspf.onesimple("CSPF tests passed.")
