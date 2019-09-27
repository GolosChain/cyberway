import hashlib
import json
import re

class CleosWithWalletVerifier:
    def __init__(self, testCase):
        self.testCase = testCase

    def verifyAccountCreated(self, output):
        self.testCase.verifyCleosOutputContains(output, "cyber <= cyber::newaccount")


    def verifyContractWasSet(self, output):
        self.testCase.verifyCleosOutputContains(output, "cyber <= cyber::setcode")
        self.testCase.verifyCleosOutputContains(output, "cyber <= cyber::setabi")

    def verifyContractHash(self, output, contractName):
        path = self.testCase.contractsManager.getWasmFilePath(contractName)
        file = open(path, 'rb')
        hash = hashlib.sha256()
        hash.update(file.read())
        file.close()
        self.testCase.verifyCleosOutputContains(output, hash.hexdigest())

    def verifyContractAbi(self, output, contractName):
        path = self.testCase.contractsManager.getAbiFilePath(contractName)
        file = open(path, 'r')
        abiObjectFromFile = json.loads(file.read())
        file.close()
        abiObjectFromNode = json.loads("".join(output))

        # as abi file serialized/desiralizes on node some fields are absent in the source file.
        # check abi files by key fields
        self.testCase.assertEqual(abiObjectFromNode['version'], abiObjectFromFile['version'])
        self.testCase.assertEqual(abiObjectFromNode['structs'], abiObjectFromFile['structs'])

        for sourceTable in abiObjectFromFile['actions']:
            for testTable in abiObjectFromNode['actions']:
                if sourceTable['name'] == testTable['name']:
                    self.testCase.assertEqual(sourceTable['type'], testTable['type'])


        for sourceTable in abiObjectFromFile['tables']:
            for testTable in abiObjectFromNode['tables']:
                if sourceTable['name'] == testTable['name']:
                    self.testCase.assertEqual(sourceTable['type'], testTable['type'])

    def verifyGetAccount(self, output, testKey):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+owner[ ]+1:[ ]+1 " + testKey)
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+active[ ]+1:[ ]+1 " + testKey)
        self.verifyAccountLiquidBalance(output, "0.0000 CYBER")
        self.verifyAccountStakeBalance(output, "0.0000 CYBER")
        self.verifyAccountReceivedBalance(output, "0.0000 CYBER")
        self.verifyAccountProvidedBalance(output, "0.0000 CYBER")
        self.verifyAccountUnstakingBalance(output, "0.0000 CYBER")
        self.verifyAccountOwnedBalance(output, "0.0000 CYBER")
        self.verifyAccountEffectiveBalance(output, "0.0000 CYBER")
        self.verifyAccountTotalBalance(output, "0.0000 CYBER")

    def verifyTokenIssued(self, output, receiver, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::issue[ ]+{\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")

    def verifyAccountLiquidBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+liquid:[ ]+" + amount)

    def verifyAccountStakeBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+staked:[ ]+" + amount)

    def verifyAccountEffectiveBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+effective:[ ]+" + amount)

    def verifyAccountUnstakingBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+unstaking:[ ]+" + amount)

    def verifyAccountOwnedBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+owned:[ ]+" + amount)

    def verifyAccountReceivedBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+recieved:[ ]+" + amount)

    def verifyAccountProvidedBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+provided:[ ]+" + amount)

    def verifyAccountTotalBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+total:[ ]+" + amount)

    def verifyStakeCreated(self, output, symbol):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::create[ ]+{\"token_symbol\":\"" + symbol)

    def verifyStakeDelegated(self, output, grantor, receiver, quantity):
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber.stake <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")

    def verifyStakeOpened(self, output, owner):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::open[ ]+{\"owner\":\"" + owner + "\",\"token_code\":\"CYBER\",\"ram_payer\":null}")


    def verifyTokensStaked(self, output, stakeHolder, amount):
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber.stake <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"")


    def verifyStakeEnabled(self, output):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::enable[ ]+{\"token_symbol\":\"4,CYBER\"}")

    def verifyResoursesStakeEndowment(self, output):
        effective = self.getEffectiveBalance(output)

        memoryStaked = self.getResourceStaked(output, "memory")
        storageStaked = self.getResourceStaked(output, "storage")
        netStaked = self.getResourceStaked(output, "net bandwidth")
        cpuStaked = self.getResourceStaked(output, "cpu bandwidth")

        received = self.getReceivedBalance(output)

        memoryReceived = self.getResourceReceived(output, "memory")
        storageReceived = self.getResourceReceived(output, "storage")
        netReceived = self.getResourceReceived(output, "net bandwidth")
        cpuReceived = self.getResourceReceived(output, "cpu bandwidth")

        resourcesReceived = memoryReceived + storageReceived + netReceived + cpuReceived
        resourcesStaked = memoryStaked + storageStaked + netStaked + cpuStaked
        resourcesEffective = resourcesStaked + resourcesReceived

        self.testCase.assertTrue(effective - resourcesEffective <= 1)
        self.testCase.assertTrue(received - resourcesReceived <= 1)

    def getEffectiveBalance(self, output):
        return self.getBalance(output, "effective")

    def getReceivedBalance(self, output):
        return self.getBalance(output, "recieved")

    def getBalance(self, output, type):
        balances = False
        for line in output:
            if balances == False and re.match("CYBER[ ]+balances:", line):
                balances = True

            if balances == True and re.match("[ ]+" + type + ":[ ]+[0-9.]+ CYBER", line) :
                return self.getBalanceValue(line)

    def getResourceStaked(self, output, resource):
        return self.getResourceCost(output, resource, "staked")

    def getResourceReceived(self, output, resource):
        return self.getResourceCost(output, resource, "delegated")

    def getResourceCost(self, output, resource, type):
        resourceFound = False
        for line in output:
            if resourceFound == False and re.match(resource + ":", line):
                resourceFound = True

            if resourceFound == True and re.match("[ ]+" + type + ":[ ]+[0-9.]+ CYBER", line) :
                return self.getBalanceValue(line)

    def getBalanceValue(self, string):
        value = re.findall("[0-9.]+", string)[0]

        return int(re.sub('\.', '', value))

    def verifyResoursesStakeUsage(self, output):
        #implement stake usage calculation in the api
        return
