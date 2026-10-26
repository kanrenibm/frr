# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestSockunion(frrtest.TestMultiOut):
    program = "./test_sockunion"


TestSockunion.onesimple("all sockunion tests passed")
