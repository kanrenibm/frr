# SPDX-License-Identifier: GPL-2.0-or-later
import frrtest


class TestKeychain(frrtest.TestMultiOut):
    program = "./test_keychain"


TestKeychain.exit_cleanly()
