# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestOpenbsdTree(frrtest.TestMultiOut):
    program = "./test_openbsd_tree"


TestOpenbsdTree.exit_cleanly()
