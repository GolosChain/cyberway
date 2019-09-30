from Suite.Framework.NodeosApiTestCase import NodeosApiTestCase
from abc import abstractmethod
from Suite.Framework.NodeosApiTest import WalletTestSuite

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


