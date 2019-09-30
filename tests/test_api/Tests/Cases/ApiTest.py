from Suite.Framework.NodeosApiTestCase import NodeosApiTestCase

class ApiTest(NodeosApiTestCase):
    def setApitTestExecutables(self, apiClient, contractsManager):
        self.apiClient = apiClient
        self.contractsManager = contractsManager

    def test_2_getSystemContractsAbiThroughApi(self):
        self.apiClient.sendRequest("get_abi", {'account_name' : 'cyber.token'})


