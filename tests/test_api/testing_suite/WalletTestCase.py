from testing_suite.NodeosApiTestCase import NodeosApiTestCase
from abc import abstractmethod
from testing_suite.NodeosApiTest import WalletTestSuite

import re

class WalletTestCase(NodeosApiTestCase):
    @abstractmethod
    def setApiTestExecutables(self, apiClient, contractsManager):
        walletTest = WalletTestSuite.instance

        cleos = walletTest.cleos
        wallet = walletTest.wallet
        self.setCleosTestExecutables(cleos, wallet, apiClient, contractsManager)

    @abstractmethod
    def setCleosTestExecutables(self, cleos, wallet, apiClient, contractsManager):
        ...

    def verifyCleosOutputContains(self, output, value):
        for line in output:
            if value in line:
                return

        self.assertTrue(False, "The value \'" + value + "\' not found in the cleos output")

    def verifyCleosOutputContainsRegex(self, output, regex):
        for line in output:
            if re.match(regex, line):
                return

        self.assertTrue(False, "The regexp \'" + regex + "\' does not match the cleos output")

    def verifyCleosOutputNotContains(self, output, value):
        for line in output:
            self.assertNotIn(value, line, "The value \'" + value + "\' should not be in the cleos output")

