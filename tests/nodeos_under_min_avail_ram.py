#!/usr/bin/env python3

from core_symbol import CORE_SYMBOL
from Cluster import Cluster
from WalletMgr import WalletMgr
from Node import Node
from TestHelper import TestHelper
from testUtils import Utils
import testUtils
import time

import decimal
import math
import re

Print=Utils.Print
errorExit=Utils.errorExit
from testUtils import maxInt64
from testUtils import maxInt32

class NamedAccounts:

    def __init__(self, cluster, numAccounts):
        Print("NamedAccounts %d" % (numAccounts))
        self.numAccounts=numAccounts
        self.accounts=cluster.createAccountKeys(numAccounts)
        if self.accounts is None:
            errorExit("FAILURE - create keys")
        accountNum = 0
        for account in self.accounts:
            Print("NamedAccounts Name for %d" % (accountNum))
            account.name=self.setName(accountNum)
            accountNum+=1

    def setName(self, num):
        retStr="test"
        digits=[]
        maxDigitVal=5
        maxDigits=8
        temp=num
        while len(digits) < maxDigits:
            digit=(num % maxDigitVal)+1
            num=int(num/maxDigitVal)
            digits.append(digit)

        digits.reverse()
        for digit in digits:
            retStr=retStr+str(digit)

        Print("NamedAccounts Name for %d is %s" % (temp, retStr))
        return retStr


args = TestHelper.parse_args({"--dump-error-details","--keep-logs","-v","--leave-running","--clean-run","--wallet-port"})
Utils.Debug=args.v
totalNodes=1
cluster=Cluster(walletd=True, enableMongo=True, enableProducerAPI=True)
dumpErrorDetails=args.dump_error_details
keepLogs=args.keep_logs
dontKill=args.leave_running
killAll=args.clean_run
walletPort=args.wallet_port

walletMgr=WalletMgr(True, port=walletPort)
testSuccessful=False
killEosInstances=not dontKill
killWallet=not dontKill
corePrecision = 10000

WalletdName=Utils.EosWalletName
ClientName="cleos"

def sendAction(node, contractAccount, actionName, fromAccount, data, opts, shouldSucceed = True):
    res, trans = node.pushMessage(contractAccount.name, actionName, data, opts)
    if res and not shouldSucceed:
        raise ValueError("Action should fail")
    if not res and shouldSucceed:
        raise ValueError("Failed to store")
    return trans

def store(node, contractAccount, fromAccount, toAccount, bytesNum, shouldSucceed = True):
    opts="--permission %s@active --permission %s@active --expiration 90" % (contractAccount.name, fromAccount.name)
    data="{\"from\":\"%s\",\"to\":\"%s\",\"num\":%d}" % (fromAccount.name, toAccount.name, int(bytesNum))
    return sendAction(node, contractAccount, "store", fromAccount, data, opts, shouldSucceed=shouldSucceed)

def modify(node, contractAccount, fromAccount, signer, val, shouldSucceed = True):
    opts="--permission %s@active --permission %s@active --expiration 90" % (signer.name, signer.name)
    data="{\"from\":\"%s\",\"val\":%d}" % (fromAccount.name, val)
    return sendAction(node, contractAccount, "modify", fromAccount, data, opts, shouldSucceed=shouldSucceed)

try:
    TestHelper.printSystemInfo("BEGIN")
    cluster.setWalletMgr(walletMgr)

    cluster.killall(allInstances=killAll)
    cluster.cleanup()
    minRAMFlag="--chain-state-db-guard-size-mb"
    minRAMValue=1002
    maxRAMFlag="--chain-state-db-size-mb"
    maxRAMValue=1010
    extraNodeosArgs=" %s %d %s %d " % (minRAMFlag, minRAMValue, maxRAMFlag, maxRAMValue)
    if cluster.launch(onlyBios=False, pnodes=totalNodes, totalNodes=totalNodes, totalProducers=totalNodes, extraNodeosArgs=extraNodeosArgs, useBiosBootFile=False) is False:
        Utils.cmdError("launcher")
        errorExit("Failed to stand up eos cluster.")
        
    nodes=[]
    for i in range(totalNodes):
        nodes.append(cluster.getNode(i))
    biosNode = cluster.biosNode
        
    nodes[0].freezeVirtualLimits([maxInt32, maxInt64, maxInt64, 1])

    Print("Validating system accounts after bootstrap")
    cluster.validateAccounts(None)

    Print("creating accounts")
    namedAccounts=NamedAccounts(cluster,3)
    accounts=namedAccounts.accounts

    testWalletName="test"

    Print("Creating wallet \"%s\"." % (testWalletName))
    testWallet=walletMgr.create(testWalletName, [cluster.eosioAccount])
    
    for _, account in cluster.defProducerAccounts.items():
        walletMgr.importKey(account, testWallet, ignoreDupKeyWarning=True)

    Print("Wallet \"%s\" password=%s." % (testWalletName, testWallet.password.encode("utf-8")))

    for account in accounts:
        walletMgr.importKey(account, testWallet)

    nodes[0].installStaking(cluster.eosioAccount.name, withGovern=False, waitForTransBlock=True, exitOnError=True)
    
    # create accounts via eosio as otherwise a bid is needed
    totalStaked = 0
    quantity = 10
    contractQuantity = quantity * 100
    amount = quantity * corePrecision
    
    for account in accounts:
        Print("Create new account %s via %s" % (account.name, cluster.eosioAccount.name))
        trans=nodes[0].createInitializeAccount(account, cluster.eosioAccount, stakedDeposit=0, waitForTransBlock=False, exitOnError=True)
        nodes[0].transferFunds(cluster.eosioAccount, account, "%s %s" % (quantity, CORE_SYMBOL))
        nodes[0].stakeFunds(account, quantity, cluster.eosioStakeAccount, waitForTransBlock=True, exitOnError=True)
        totalStaked += quantity

    contractAccount=cluster.createAccountKeys(1)[0]
    contractAccount.name="contracttest"
    walletMgr.importKey(contractAccount, testWallet)
    Print("Create new account %s via %s" % (contractAccount.name, cluster.eosioAccount.name))
    trans=nodes[0].createInitializeAccount(contractAccount, cluster.eosioAccount, stakedDeposit=0, waitForTransBlock=False, exitOnError=True)
    nodes[0].transferFunds(cluster.eosioAccount, contractAccount, "%s %s" % (contractQuantity, CORE_SYMBOL))
    nodes[0].stakeFunds(contractAccount, contractQuantity, cluster.eosioStakeAccount, waitForTransBlock=True, exitOnError=True)
    totalStaked += contractQuantity
    
    contractDir="contracts/integration_test"
    wasmFile="integration_test.wasm"
    abiFile="integration_test.abi"
    Print("Publish contract")
    trans=nodes[0].publishContract(contractAccount.name, contractDir, wasmFile, abiFile, waitForTransBlock=True)
    if trans is None:
        Utils.cmdError("%s set contract %s" % (ClientName, contractAccount.name))
        errorExit("Failed to publish contract.")

    pricelist = nodes[0].getPricelist()
    storagePrice = pricelist[3]
    print("storage price = ", storagePrice)
    
    enabled = nodes[0].stakeEnable(issuer=cluster.eosioAccount.name)
    nodes[0].waitForTransBlockIfNeeded(enabled[1], waitForTransBlock=True, exitOnError=True)
    
    leeway = 0.1
    canStore = int(amount / storagePrice)
    
    store(nodes[0], contractAccount, accounts[0], accounts[1], canStore + 1, False)
    print(pricelist)
    trans = store(nodes[0], contractAccount, accounts[0], accounts[1], canStore * (1.0 - leeway))
    nodes[0].waitForTransBlockIfNeeded(trans, waitForTransBlock=True, exitOnError=True)
    
    pre_usage = nodes[0].getAccountUsage(accounts[1].name)
    trans = modify(nodes[0], contractAccount, accounts[0], accounts[1], 34)
    nodes[0].waitForTransBlockIfNeeded(trans, waitForTransBlock=True, exitOnError=True)
    new_usage = nodes[0].getAccountUsage(accounts[1].name)
    usage0 = new_usage[2] - pre_usage[2]
    nodes[0].setSubjectiveRam(2, 1, 10)
    pre_usage = nodes[0].getAccountUsage(accounts[1].name)
    trans = modify(nodes[0], contractAccount, accounts[0], accounts[1], 43)
    nodes[0].waitForTransBlockIfNeeded(trans, waitForTransBlock=True, exitOnError=True)
    new_usage = nodes[0].getAccountUsage(accounts[1].name)
    usage1 = new_usage[2] - pre_usage[2]
    print("usage0 =", usage0, "usage1 =", usage1)
    usageRatio = usage1 / usage0
    if usageRatio < 4.5 or usageRatio > 5.5:
        errorExit("suspicious ratio of RAM consumed")
    
    nodes[0].setSubjectiveRam(2048, 1024, 10000)
    modify(nodes[0], contractAccount, accounts[0], accounts[1], 52, False)
    nodes[0].setSubjectiveRam(2048, 1024, 10)
    
    trans = modify(nodes[0], contractAccount, accounts[0], accounts[1], 52)
    nodes[0].waitForTransBlockIfNeeded(trans, waitForTransBlock=True, exitOnError=True)
    
    testSuccessful=True
    
finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, killEosInstances=killEosInstances, killWallet=killWallet, keepLogs=keepLogs, cleanRun=killAll, dumpErrorDetails=dumpErrorDetails)

exit(0)
