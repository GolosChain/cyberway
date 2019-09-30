from Suite.Framework.WalletTestCase import WalletTestCase
from Suite.Framework.NodeosApiTest import WalletTestSuite
from abc import abstractmethod

from Tests.Verificators.TransactionExecutionVerificator import *
from Tests.Verificators.GetAccountVerificator import *

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

    def test_00_loadSystemContracts(self):
        verifyAccountCreated(self.cleos.exec("create", "account", "cyber", "cyber.token", self.wallet.createKeys("cyber.token test key")))
        verifyContractWasSet(self.cleos.exec("set", "contract", "cyber.token", self.contractsManager.getContractPath("cyber.token"), "-p", "cyber.token@active"))
        verifyContractWasSet(self.cleos.exec("set", "contract", "cyber.stake", self.contractsManager.getContractPath("cyber.stake"), "-p", "cyber.stake@active"))
        verifyContractWasSet(self.cleos.exec("set", "contract", "cyber", self.contractsManager.getContractPath("cyber.bios"), "-p", "cyber@active"))

    def test_01_createSystemToken(self):
        output = self.cleos.exec("push", "action", "cyber.token", "create", "[cyber, \"100000.0000 CYBER\"]", "-p", "cyber.token")

        verifyOutputContains(output, "cyber.token <= cyber.token::create")
        verifyOutputContains(output, "{\"issuer\":\"cyber\",\"maximum_supply\":\"100000.0000 CYBER\"}")

    def test_02_getSystemContractsHash(self):
        verifyContractHash(self.cleos.exec("get", "code", "cyber.token"), "cyber.token", self.contractsManager)
        verifyContractHash(self.cleos.exec("get", "code", "cyber.stake"), "cyber.stake", self.contractsManager)
        verifyContractHash(self.cleos.exec("get", "code", "cyber"), "cyber.bios", self.contractsManager)

    def test_03_getSystemContractsAbi(self):
        verifyContractAbi(self.cleos.exec("get", "abi", "cyber.token"), "cyber.token", self.contractsManager)
        verifyContractAbi(self.cleos.exec("get", "abi", "cyber.stake"), "cyber.stake", self.contractsManager)

    def test_04_createTestAccount(self):
        testKey = self.wallet.createKeys("alice test key")
        verifyAccountCreated(self.cleos.exec("create", "account", "cyber", "alice", testKey))
        verifyGetNewAccount(self.cleos.exec("get", "account", "alice"), testKey)

    def test_05_issueTokens(self):
        verifyTokenIssued(self.cleos.exec("push", "action", "cyber.token", "issue", "[alice, \"1000.0000 CYBER\", \"\"]", "-p", "cyber@active"), "alice",  "1000.0000 CYBER")

        getAccountOutput = self.cleos.exec("get", "account", "alice")
        verifyAccountLiquidBalance(getAccountOutput,  "1000.0000 CYBER")
        verifyAccountTotalBalance(getAccountOutput,  "1000.0000 CYBER")

    def test_06_createStake(self):
        verifyStakeCreated(self.cleos.exec("push", "action", "cyber.stake", "create", "[\"4,CYBER\", [20, 16, 13], 30, 50, 0]", "-p", "cyber@active"), "4,CYBER")

    def test_07_stakeTokens(self):
        verifyTokensStaked(self.cleos.exec("system", "stake", "alice", "400.0000 CYBER"), "alice", "400.0000 CYBER")

        getAccountOutput = self.cleos.exec("get", "account", "alice")

        verifyAccountStakeBalance(getAccountOutput, "400.0000 CYBER")
        verifyAccountEffectiveBalance(getAccountOutput, "400.0000 CYBER")
        verifyAccountLiquidBalance(getAccountOutput, "600.0000 CYBER")
        verifyAccountTotalBalance(getAccountOutput, "1000.0000 CYBER")

    def test_08_delegateStake(self):
        self.cleos.exec("create", "account", "cyber", "bob", self.wallet.createKeys("bob test key"))

        verifyStakeOpened(self.cleos.exec("push", "action", "cyber.stake", "open", "{ \"owner\" : bob, \"token_code\" : \"CYBER\" }", "-p", "bob@active"), "bob")
        verifyStakeDelegated(self.cleos.exec("system", "delegatebw", "alice", "bob", "100.0000 CYBER"), "alice", "bob", "100.0000 CYBER")

        getAliceAccountOutput = self.cleos.exec("get", "account", "alice")

        verifyAccountStakeBalance(getAliceAccountOutput, "400.0000 CYBER")
        verifyAccountOwnedBalance(getAliceAccountOutput, "300.0000 CYBER")
        verifyAccountProvidedBalance(getAliceAccountOutput, "100.0000 CYBER")
        verifyAccountLiquidBalance(getAliceAccountOutput, "600.0000 CYBER")
        verifyAccountEffectiveBalance(getAliceAccountOutput, "300.0000 CYBER")
        verifyAccountTotalBalance(getAliceAccountOutput, "900.0000 CYBER")

        getBobAccountOutput = self.cleos.exec("get", "account", "bob")

        verifyAccountStakeBalance(getBobAccountOutput, "0.0000 CYBER")
        verifyAccountOwnedBalance(getBobAccountOutput, "0.0000 CYBER")
        verifyAccountProvidedBalance(getBobAccountOutput, "0.0000 CYBER")
        verifyAccountReceivedBalance(getBobAccountOutput, "100.0000 CYBER")
        verifyAccountLiquidBalance(getBobAccountOutput, "0.0000 CYBER")
        verifyAccountEffectiveBalance(getBobAccountOutput, "100.0000 CYBER")
        verifyAccountTotalBalance(getBobAccountOutput, "100.0000 CYBER")

    def test_09_delegateStake(self):
        self.cleos.exec("create", "account", "cyber", "clara", self.wallet.createKeys("clara test key"))
        self.cleos.exec("push", "action", "cyber.token", "issue", "[clara, \"300.0000 CYBER\", \"\"]", "-p", "cyber@active")
        self.cleos.exec("system", "stake", "clara", "200.0000 CYBER")

        self.cleos.exec("system", "delegatebw", "clara", "alice", "80.0000 CYBER")

        getAliceAccountOutput = self.cleos.exec("get", "account", "alice")

        verifyAccountLiquidBalance(getAliceAccountOutput, "600.0000 CYBER")
        verifyAccountStakeBalance(getAliceAccountOutput, "400.0000 CYBER")
        verifyAccountOwnedBalance(getAliceAccountOutput, "300.0000 CYBER")
        verifyAccountProvidedBalance(getAliceAccountOutput, "100.0000 CYBER")
        verifyAccountReceivedBalance(getAliceAccountOutput, "80.0000 CYBER")
        verifyAccountEffectiveBalance(getAliceAccountOutput, "380.0000 CYBER")
        verifyAccountTotalBalance(getAliceAccountOutput, "980.0000 CYBER")

    def test_10_unstakeTokens(self):
        self.cleos.exec("push", "action", "cyber.stake", "withdraw", "[\"alice\", \"100.0000 CYBER\"]", "-p", "alice")

        getAliceAccountOutput = self.cleos.exec("get", "account", "alice")
        verifyAccountLiquidBalance(getAliceAccountOutput, "700.0000 CYBER")
        verifyAccountStakeBalance(getAliceAccountOutput, "300.0000 CYBER")
        verifyAccountUnstakingBalance(getAliceAccountOutput, "0.0000 CYBER")
        verifyAccountOwnedBalance(getAliceAccountOutput, "200.0000 CYBER")
        verifyAccountEffectiveBalance(getAliceAccountOutput, "280.0000 CYBER")
        verifyAccountTotalBalance(getAliceAccountOutput, "980.0000 CYBER")

    def test_11_enableStaking(self):
        verifyStakeEnabled(self.cleos.exec("push", "action", "cyber.stake", "enable", "[\"4,CYBER\"]", "-p", "cyber"))

    def test_12_stakeUsage(self):
        self.cleos.exec("create", "account", "alice", "alice.child", self.wallet.createKeys("alice.child test key"))

        output = self.cleos.exec("get", "account", "alice")

        verifyResoursesStakeEndowment(output)
        verifyResoursesStakeUsage(output)

    
if __name__ == '__main__':
    WalletTestSuite().execute()
