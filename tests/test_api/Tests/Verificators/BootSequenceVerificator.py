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
    abiFromFile = jsonToPython(abiFromFileString)
    addNodeosChanges(abiFromFile)

    compare(Comparison(abiFromFile), abiFromNode)

def addNodeosChanges(abi):
    setattr(abi, 'error_messages', [])
    if hasattr(abi, 'ricardian_clauses'):
        delattr(abi, 'ricardian_clauses')

    if not hasattr(abi, 'events'):
        setattr(abi, 'events', [])

    for table in abi.tables:
        if not hasattr(table, 'scope_type'):
            setattr(table, 'scope_type', "")

        for index in table.indexes:
            if type(index.unique) is str:
                value = index.unique
                delattr(index, 'unique')
                setattr(index, 'unique', value == "true")

    for action in abi.actions:
        if hasattr(action, 'ricardian_contract'):
            delattr(action, 'ricardian_contract')
