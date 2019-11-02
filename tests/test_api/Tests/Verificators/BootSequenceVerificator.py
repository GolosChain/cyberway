from Tests.Verificators.OutputBaseVerificator import *
from Tests.Utils.JsonUtils import *
from testfixtures import *

import hashlib

def verifyContractHash(output, contractName, contractsManager):
    path = contractsManager.getWasmFilePath(contractName)
    file = open(path, 'rb')
    hash = hashlib.sha256()
    hash.update(file.read())
    file.close()
    verifyOutputContains(output, hash.hexdigest())

def verifyContractAbi(output, contractName, contractsManager):
    path = contractsManager.getAbiFilePath(contractName)
    file = open(path, 'r')
    abiFromFileString = file.read()
    file.close()
    abiFromNodeString = "".join(output)

    abiFromNode = jsonToPython(abiFromNodeString)
    removeAdditionalFields(abiFromNode)
    abiFromFile = jsonToPython(abiFromFileString)

    compare(Comparison(abiFromFile), abiFromNode)

def removeAdditionalFields(abi):
    delattr(abi, 'error_messages')

    for table in abi.tables:
        if not table.scope_type:
            delattr(table, 'scope_type')


