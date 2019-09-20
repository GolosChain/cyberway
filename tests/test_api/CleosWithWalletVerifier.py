import hashlib
import json

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
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+recieved:[ ]+0.0000 CYBER")
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+provided:[ ]+0.0000 CYBER")
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+unstaking:[ ]+0.0000 CYBER")
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+owned:[ ]+0.0000 CYBER")
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+effective:[ ]+0.0000 CYBER")
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+total:[ ]+0.0000 CYBER")

    def verifyTokenIssued(self, output, receiver, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::issue[ ]+{\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")

    def verifyAccountLiquidBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+liquid:[ ]+" + amount)

    def verifyAccountTotalBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+total:[ ]+" + amount)

    def verifyStakeCreated(self, output, symbol):
        self.testCase.verifyCleosOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::create[ ]+{\"token_symbol\":\"" + symbol)

    def verifyTokensStaked(self, output, stakeHolder, amount):
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
        self.testCase.verifyCleosOutputContainsRegex(output,
                                                     "#[ ]+cyber.stake <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"")

    def verifyAccountStakeBalance(self, output, amount):
        self.testCase.verifyCleosOutputContainsRegex(output, "[ ]+staked:[ ]+" + amount)