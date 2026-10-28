# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestAggTable(frrtest.TestMultiOut):
    program = "./test_agg_table"


TestAggTable.exit_cleanly()
