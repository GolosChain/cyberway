from testing_suite.NodeosApiTestCase import NodeosApiTestCase
from abc import abstractmethod
from testing_suite.NodeosApiTest import WalletTestSuite

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


