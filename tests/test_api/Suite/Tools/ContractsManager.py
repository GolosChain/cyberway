import glob

class ContractsManager:
    def __init__(self, contracts):
        self.contractsMap={}
        contractName = ""
        for i in range(len(contracts)):
            if i % 2 == 0:
                contractName = contracts[i]
            if i % 2 == 1:
                directory = contracts[i]
                wasm, abi = self.getWasmAndAbi(directory)
                self.contractsMap[contractName] = dict(directory=directory, wasm=wasm, abi=abi)
                contractName = ""

    def getWasmAndAbi(self, directory):
        wasmFiles = glob.glob(directory + "/*.wasm")
        abiFiles = glob.glob(directory + "/*.abi")
        return self.getWasmFile(directory, wasmFiles), self.getAbiFile(directory, abiFiles)

    def getWasmFile(self, directory, filesInDirectory):
        return self.getFile(directory, filesInDirectory, 'wasm')

    def getAbiFile(self, directory, filesInDirectory):
        return self.getFile(directory, filesInDirectory, 'abi')

    def getFile(self,  directory, filesInDirectory, ext):
        for file in filesInDirectory:
            pathItems = file.split('/')
            if pathItems[len(pathItems) - 1 ] == (pathItems[len(pathItems) - 2 ] + "." + ext):
                return file

        raise NameError("No correct \'" + ext + "\' file in the directory: " + directory)

    def getContractPath(self, contractName):
        return self.contractsMap[contractName]['directory']

    def requireContract(self, contractName):
        if type(self.contractsMap.get(contractName)) == None:
            raise NameError("The contract " + contractName + " has not been specified in the param --contracts")

    def getAbiFilePath(self, contractName):
        return self.contractsMap[contractName]['abi']

    def getWasmFilePath(self, contractName):
        return self.contractsMap[contractName]['wasm']