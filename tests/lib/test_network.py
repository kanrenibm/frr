# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestNetwork(frrtest.TestMultiOut):
    program = "./test_network"


TestNetwork.exit_cleanly()
