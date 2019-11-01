from Suite.Framework.WalletTestCase import WalletTestCase
from abc import abstractmethod

from Tests.Verificators.TransactionExecutionVerificator import *
from Tests.Verificators.GetAccountVerificator import *

class BootSequenceTest(WalletTestCase):
    @abstractmethod
    def setCleosTestExecutables(self, cleos, wallet, apiClient, contractsManager):
        self.cleos = cleos
        self.wallet = wallet
        self.apiClient = apiClient
        self.contractsManager = contractsManager
        self.contractsManager.requireContract('cyber.token')
        self.contractsManager.requireContract('cyber.stake')
        self.contractsManager.requireContract('cyber.bios')

    def test_00_loadSystemContracts(self):
        verifyAccountCreated(self.cleos.exec('create account cyber cyber.token {key}'.format(key = self.wallet.createKeys('cyber.token test key'))))
        verifyContractWasSet(self.cleos.exec('set contract cyber.token {contractPath} -p cyber.token@active'.format(contractPath=self.contractsManager.getContractPath('cyber.token'))))
        verifyContractWasSet(self.cleos.exec('set contract cyber.stake {contractPath} -p cyber.stake@active'.format(contractPath=self.contractsManager.getContractPath('cyber.stake'))))
        verifyContractWasSet(self.cleos.exec('set contract cyber {contractPath} -p cyber@active'.format(contractPath=self.contractsManager.getContractPath('cyber.bios'))))

    def test_01_createSystemToken(self):
        output = self.cleos.exec('push action cyber.token create \'[cyber, "100000.0000 CYBER"]\' -p cyber.token')

        verifyOutputContains(output, 'cyber.token <= cyber.token::create')
        verifyOutputContains(output, '{"issuer":"cyber","maximum_supply":"100000.0000 CYBER"}')

    def test_02_getSystemContractsHash(self):
        verifyContractHash(self.cleos.exec('get code cyber.token'), 'cyber.token', self.contractsManager)
        verifyContractHash(self.cleos.exec('get code cyber.stake'), 'cyber.stake', self.contractsManager)
        verifyContractHash(self.cleos.exec('get code cyber'), 'cyber.bios', self.contractsManager)

    def test_03_getSystemContractsAbi(self):
        verifyContractAbi(self.cleos.exec('get abi cyber.token'), 'cyber.token', self.contractsManager)
        verifyContractAbi(self.cleos.exec('get abi cyber.stake'), 'cyber.stake', self.contractsManager)
