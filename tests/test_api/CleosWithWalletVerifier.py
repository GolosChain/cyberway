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


