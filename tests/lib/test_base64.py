# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestBase64(frrtest.TestMultiOut):
    program = "./test_base64"


TestBase64.exit_cleanly()
