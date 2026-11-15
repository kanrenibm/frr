# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestJson(frrtest.TestMultiOut):
    program = "./test_json"


TestJson.exit_cleanly()
