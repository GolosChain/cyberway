from Suite.Framework.WalletTestCase import WalletTestCase
from Suite.Framework.Utils import jsonArg

from abc import abstractmethod

from Tests.Verificators.TransactionExecutionVerificator import *
from Tests.Verificators.GetAccountVerificator import *

class GetAccountTest(WalletTestCase):
    @abstractmethod
    def setCleosTestExecutables(self, cleos, wallet, apiClient, contractsManager):
        self.cleos = cleos
        self.wallet = wallet
        self.apiClient = apiClient
        self.contractsManager = contractsManager

    def test_04_createTestAccount(self):
        key = self.wallet.createKeys('alice test key')
        verifyAccountCreated(self.cleos.exec('create account cyber alice {key}'.format(key = key)))
        verifyGetNewAccount(self.cleos.exec('get account alice'), key)

    def test_05_issueTokens(self):
        args = jsonArg(["alice", "1000.0000 CYBER", ""])
        verifyTokenIssued(self.cleos.exec('push action cyber.token issue {args} -p cyber@active'.format(args=args)), 'alice',  '1000.0000 CYBER')

        getAccountOutput = self.cleos.exec('get account alice')
        verifyAccountLiquidBalance(getAccountOutput,  '1000.0000 CYBER')
        verifyAccountTotalBalance(getAccountOutput,  '1000.0000 CYBER')

    def test_06_createStake(self):
        args = jsonArg(["4,CYBER", [20, 16, 13], 30, 50, 0])
        verifyStakeCreated(self.cleos.exec('push action cyber.stake create {args} -p cyber@active'.format(args = args)), '4,CYBER')

    def test_07_stakeTokens(self):
        verifyTokensStaked(self.cleos.exec('system stake alice "400.0000 CYBER"'), 'alice', '400.0000 CYBER')

        getAccountOutput = self.cleos.exec('get account alice')

        verifyAccountStakeBalance(getAccountOutput, '400.0000 CYBER')
        verifyAccountEffectiveBalance(getAccountOutput, '400.0000 CYBER')
        verifyAccountLiquidBalance(getAccountOutput, '600.0000 CYBER')
        verifyAccountTotalBalance(getAccountOutput, '1000.0000 CYBER')

    def test_08_delegateStake(self):
        self.cleos.exec('create account cyber bob {key}'.format(key = self.wallet.createKeys('bob test key')))

        args = jsonArg({ "owner" : "bob", "token_code" : "CYBER" })
        verifyStakeOpened(self.cleos.exec('push action cyber.stake open {args} -p bob@active'.format(args = args)), 'bob')
        verifyStakeDelegated(self.cleos.exec('system delegatebw alice bob "100.0000 CYBER"'), 'alice', 'bob', '100.0000 CYBER')

        getAliceAccountOutput = self.cleos.exec('get account alice')

        verifyAccountStakeBalance(getAliceAccountOutput, '400.0000 CYBER')
        verifyAccountOwnedBalance(getAliceAccountOutput, '300.0000 CYBER')
        verifyAccountProvidedBalance(getAliceAccountOutput, '100.0000 CYBER')
        verifyAccountLiquidBalance(getAliceAccountOutput, '600.0000 CYBER')
        verifyAccountEffectiveBalance(getAliceAccountOutput, '300.0000 CYBER')
        verifyAccountTotalBalance(getAliceAccountOutput, '900.0000 CYBER')

        getBobAccountOutput = self.cleos.exec('get account bob')

        verifyAccountStakeBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountOwnedBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountProvidedBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountReceivedBalance(getBobAccountOutput, '100.0000 CYBER')
        verifyAccountLiquidBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountEffectiveBalance(getBobAccountOutput, '100.0000 CYBER')
        verifyAccountTotalBalance(getBobAccountOutput, '100.0000 CYBER')

    def test_09_delegateStake(self):
        self.cleos.exec('create account cyber clara {key}'.format(key =  self.wallet.createKeys('clara test key')))

        args = jsonArg(["clara", "300.0000 CYBER", ""])
        self.cleos.exec('push action cyber.token issue {args} -p cyber@active'.format(args=args))
        self.cleos.exec('system stake clara "200.0000 CYBER"')

        self.cleos.exec('system delegatebw clara alice "80.0000 CYBER"')

        getAliceAccountOutput = self.cleos.exec('get account alice')

        verifyAccountLiquidBalance(getAliceAccountOutput, '600.0000 CYBER')
        verifyAccountStakeBalance(getAliceAccountOutput, '400.0000 CYBER')
        verifyAccountOwnedBalance(getAliceAccountOutput, '300.0000 CYBER')
        verifyAccountProvidedBalance(getAliceAccountOutput, '100.0000 CYBER')
        verifyAccountReceivedBalance(getAliceAccountOutput, '80.0000 CYBER')
        verifyAccountEffectiveBalance(getAliceAccountOutput, '380.0000 CYBER')
        verifyAccountTotalBalance(getAliceAccountOutput, '980.0000 CYBER')

    def test_10_unstakeTokens(self):
        args = jsonArg(["alice", "100.0000 CYBER"])
        verifyTokenUnstaked(self.cleos.exec('push action cyber.stake withdraw {args} -p alice'.format(args=args)), 'alice', '100.0000 CYBER')

        getAliceAccountOutput = self.cleos.exec('get account alice')
        verifyAccountLiquidBalance(getAliceAccountOutput, '700.0000 CYBER')
        verifyAccountStakeBalance(getAliceAccountOutput, '300.0000 CYBER')
        verifyAccountUnstakingBalance(getAliceAccountOutput, '0.0000 CYBER')
        verifyAccountOwnedBalance(getAliceAccountOutput, '200.0000 CYBER')
        verifyAccountEffectiveBalance(getAliceAccountOutput, '280.0000 CYBER')
        verifyAccountTotalBalance(getAliceAccountOutput, '980.0000 CYBER')

    def test_11_enableStaking(self):
        args = jsonArg(["4,CYBER"])
        verifyStakeEnabled(self.cleos.exec('push action cyber.stake enable {args} -p cyber'.format(args=args)))

    def test_12_stakeUsage(self):
        self.cleos.exec('create account alice alice.child {key}'.format(key = self.wallet.createKeys('alice.child test key')))

        output = self.cleos.exec('get account alice')

        verifyResoursesStakeEndowment(output)
        verifyResoursesStakeUsage(output)

    def test_13_stakeForAnotherAccount(self):
        verifyTokensStaked(self.cleos.exec('system stake alice "200.0000 CYBER" --beneficiary bob'), 'alice', '200.0000 CYBER', 'bob')

        getAliceAccountOutput = self.cleos.exec('get account alice')

        verifyAccountLiquidBalance(getAliceAccountOutput, '500.0000 CYBER')
        verifyAccountStakeBalance(getAliceAccountOutput, '300.0000 CYBER')
        verifyAccountProvidedBalance(getAliceAccountOutput, '100.0000 CYBER')
        verifyAccountReceivedBalance(getAliceAccountOutput, '80.0000 CYBER')
        verifyAccountEffectiveBalance(getAliceAccountOutput, '280.0000 CYBER')
        verifyAccountTotalBalance(getAliceAccountOutput, '780.0000 CYBER')

        getBobAccountOutput = self.cleos.exec('get account bob')
        verifyAccountLiquidBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountStakeBalance(getBobAccountOutput, '200.0000 CYBER')
        verifyAccountProvidedBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountReceivedBalance(getBobAccountOutput, '100.0000 CYBER')
        verifyAccountEffectiveBalance(getBobAccountOutput, '300.0000 CYBER')
        verifyAccountTotalBalance(getBobAccountOutput, '300.0000 CYBER')


    def test_14_transferBandwitdth(self):
        verifyStakeTransfered(self.cleos.exec('system delegatebw alice bob "100.0000 CYBER" --transfer'), 'alice', 'bob', '100.0000 CYBER')

        getAliceAccountOutput = self.cleos.exec('get account alice')

        verifyAccountLiquidBalance(getAliceAccountOutput, '500.0000 CYBER')
        verifyAccountStakeBalance(getAliceAccountOutput, '200.0000 CYBER')
        verifyAccountProvidedBalance(getAliceAccountOutput, '100.0000 CYBER')
        verifyAccountReceivedBalance(getAliceAccountOutput, '80.0000 CYBER')
        verifyAccountEffectiveBalance(getAliceAccountOutput, '180.0000 CYBER')
        verifyAccountTotalBalance(getAliceAccountOutput, '680.0000 CYBER')

        getBobAccountOutput = self.cleos.exec('get account bob')
        verifyAccountLiquidBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountStakeBalance(getBobAccountOutput, '300.0000 CYBER')
        verifyAccountProvidedBalance(getBobAccountOutput, '0.0000 CYBER')
        verifyAccountReceivedBalance(getBobAccountOutput, '100.0000 CYBER')
        verifyAccountEffectiveBalance(getBobAccountOutput, '400.0000 CYBER')
        verifyAccountTotalBalance(getBobAccountOutput, '400.0000 CYBER')


