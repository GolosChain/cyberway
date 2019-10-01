from testing_suite.NodeosApiTestCase import NodeosApiTestCase

class ApiTest(NodeosApiTestCase):
    def setApitTestExecutables(self, apiClient, contractsManager):
        self.apiClient = apiClient
        self.contractsManager = contractsManager

    def test_2_getSystemContractsAbiThroughApi(self):
        self.apiClient.sendRequest("get_abi", {'account_name' : 'cyber.token'})

    def test_4_addUserAccount(self):
         self.verifyAccountCreated(self.cleos.exec("create", "account", "cyber", "alice", self.wallet.createKeys("alice test key")))
         self.cleos.exec("get", "account", "alice")
         self.apiClient.sendRequest("get_account", {'account_name': 'alice'})
