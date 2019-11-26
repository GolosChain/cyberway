from os import listdir

class ContractsManager:
    def __init__(self, contracts):
        self.contractsMap={}

        for contract in contracts:
            self.addContract(contract)


    def addContract(self, directory):
        pathSegments = directory.split('/')
        files = listdir(directory)

        while len(pathSegments) != 0:
            segment = pathSegments.pop()
            abi = segment + '.abi'
            wasm = segment + '.wasm'
            if len(segment) != 0 and abi in files and wasm in files :
                self.contractsMap[segment] = {'directory' : directory, 'abi' : directory + '/' + abi, 'wasm' : directory + '/' + wasm}
                return

        raise NameError("No correct 'abi' or 'wasm' file in the directory: " + directory)

    def getContractPath(self, contractName):
        return self.contractsMap[contractName]['directory']

    def requireContract(self, contractName):
        if type(self.contractsMap.get(contractName)) == None:
            raise NameError("The contract " + contractName + " has not been specified in the param --contracts")

    def getAbiFilePath(self, contractName):
        return self.contractsMap[contractName]['abi']

    def getWasmFilePath(self, contractName):
        return self.contractsMap[contractName]['wasm']