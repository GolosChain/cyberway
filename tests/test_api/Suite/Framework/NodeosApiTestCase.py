import unittest
from abc import abstractmethod

from Suite.Framework.NodeosApiTest import NodeosApiTest

class NodeosApiTestCase(unittest.TestCase):
    def setUp(self) -> None:
        apiClient = NodeosApiTest.instance.apiClient
        contractsManager = NodeosApiTest.instance.contractsManager
        self.setApiTestExecutables(apiClient, contractsManager)

    @abstractmethod
    def setApiTestExecutables(self, apiClient, contractsManager):
        ...
