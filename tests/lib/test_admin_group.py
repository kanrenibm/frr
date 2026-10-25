# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestAdminGroup(frrtest.TestMultiOut):
    program = "./test_admin_group"


TestAdminGroup.exit_cleanly()
