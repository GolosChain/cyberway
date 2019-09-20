from testing_suite.WalletTestCase import WalletTestCase
from testing_suite.NodeosApiTest import WalletTestSuite
from abc import abstractmethod
from CleosWithWalletVerifier import CleosWithWalletVerifier

class CleosWithWalletTest(WalletTestCase):
    @abstractmethod
    def setCleosTestExecutables(self, cleos, wallet, apiClient, contractsManager):
        self.cleos = cleos
        self.wallet = wallet
        self.apiClient = apiClient
        self.contractsManager = contractsManager
        self.contractsManager.requireContract("cyber.token")
        self.contractsManager.requireContract("cyber.stake")
        self.contractsManager.requireContract("cyber.bios")
        self.verifier = CleosWithWalletVerifier(self)


    def test_0_loadSystemContracts(self):
        self.verifier.verifyAccountCreated(self.cleos.exec("create", "account", "cyber", "cyber.token", self.wallet.createKeys("cyber.token test key")))
        self.verifier.verifyContractWasSet(self.cleos.exec("set", "contract", "cyber.token", self.contractsManager.getContractPath("cyber.token"), "-p", "cyber.token@active"))
        self.verifier.verifyContractWasSet(self.cleos.exec("set", "contract", "cyber.stake", self.contractsManager.getContractPath("cyber.stake"), "-p", "cyber.stake@active"))
        self.verifier.verifyContractWasSet(self.cleos.exec("set", "contract", "cyber", self.contractsManager.getContractPath("cyber.bios"), "-p", "cyber@active"))

    def test_1_createSystemToken(self):
        output = self.cleos.exec("push", "action", "cyber.token", "create", "[cyber, \"100000.0000 CYBER\"]", "-p", "cyber.token")
        self.verifyCleosOutputContains(output, "cyber.token <= cyber.token::create")
        self.verifyCleosOutputContains(output, "{\"issuer\":\"cyber\",\"maximum_supply\":\"100000.0000 CYBER\"}")

    def test_2_getSystemContractsHash(self):
        self.verifier.verifyContractHash(self.cleos.exec("get", "code", "cyber.token"), "cyber.token")
        self.verifier.verifyContractHash(self.cleos.exec("get", "code", "cyber.stake"), "cyber.stake")
        self.verifier.verifyContractHash(self.cleos.exec("get", "code", "cyber"), "cyber.bios")

    def test_3_getSystemContractsAbi(self):
        self.verifier.verifyContractAbi(self.cleos.exec("get", "abi", "cyber.token"), "cyber.token")
        self.verifier.verifyContractAbi(self.cleos.exec("get", "abi", "cyber.stake"), "cyber.stake")


    def test_4_createTestAccount(self):
        testKey = self.wallet.createKeys("alice test key")
        self.verifier.verifyAccountCreated(self.cleos.exec("create", "account", "cyber", "alice", testKey))
        self.verifier.verifyGetAccount(self.cleos.exec("get", "account", "alice"), testKey)

    def test_5_issueTokens(self):
        self.verifier.verifyTokenIssued(self.cleos.exec("push", "action", "cyber.token", "issue", "[alice, \"1000.0000 CYBER\", \"\"]", "-p", "cyber@active"), "alice",  "1000.0000 CYBER")

        getAccountOutput = self.cleos.exec("get", "account", "alice")
        self.verifier.verifyAccountLiquidBalance(getAccountOutput,  "1000.0000 CYBER")
        self.verifier.verifyAccountTotalBalance(getAccountOutput,  "1000.0000 CYBER")

    def test_6_createStake(self):
        self.verifier.verifyStakeCreated(self.cleos.exec("push", "action", "cyber.stake", "create", "[\"4,CYBER\", [20, 16, 13], 30, 50, 0]", "-p", "cyber@active"), "4,CYBER")

    def test_7_stakeTokens(self):
        self.verifier.verifyTokensStaked(self.cleos.exec("system", "stake", "alice", "400.0000 CYBER"), "alice", "400.0000 CYBER")

        getAccountOutput = self.cleos.exec("get", "account", "alice")
        self.verifier.verifyAccountStakeBalance(getAccountOutput, "400.0000 CYBER")
        self.verifier.verifyAccountLiquidBalance(getAccountOutput, "600.0000 CYBER")
        self.verifier.verifyAccountTotalBalance(getAccountOutput, "1000.0000 CYBER")

if __name__ == '__main__':
    WalletTestSuite().execute()
