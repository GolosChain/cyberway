#!/usr/bin/env python3

from testUtils import Utils
import testUtils
from Cluster import Cluster
from WalletMgr import WalletMgr
from Node import Node
from TestHelper import TestHelper

import decimal
import math
import re
import time

###############################################################
# nodeos_voting_test
# --dump-error-details <Upon error print etc/eosio/node_*/config.ini and var/lib/node_*/stderr.log to stdout>
# --keep-logs <Don't delete var/lib/node_* folders upon test completion>
###############################################################

top_producers = 20
reserve_producers = 1
totalProducers = top_producers + reserve_producers
schedulePeriodFactor = 4

class ProducerToNode:
    map={}

    @staticmethod
    def populate(node, num):
        for prod in node.producers:
            ProducerToNode.map[prod]=num
            Utils.Print("Producer=%s for nodeNum=%s" % (prod,num))

def isValidBlockProducer(prodsActive, blockNum, node):
    blockProducer=node.getBlockProducerByNum(blockNum)
    if blockProducer not in prodsActive:
        return False
    return prodsActive[blockProducer]

def validBlockProducer(prodsActive, prodsSeen, blockNum, node, reserveMax):
    blockProducer=node.getBlockProducerByNum(blockNum)
    if blockProducer not in prodsActive:
        Utils.cmdError("unexpected block producer %s at blockNum=%s" % (blockProducer,blockNum))
        Utils.errorExit("Failed because of invalid block producer")
    if not prodsActive[blockProducer]:
        reserveMax -= 1
        if reserveMax < 0:
            Utils.cmdError("block producer %s for blockNum=%s not elected, belongs to node %s" % (blockProducer, blockNum, ProducerToNode.map[blockProducer]))
            Utils.errorExit("Failed because of incorrect block producer")
        else:
            Utils.Print("found reserve block producer %s for blockNum=%s; max:%i" % (blockProducer, blockNum, reserveMax))
        return reserveMax
    if blockProducer in prodsSeen:
        Utils.cmdError("block producer %s for blockNum=%s already produced in this round" % (blockProducer, blockNum))
        Utils.errorExit("Failed because of incorrect block producer")
    prodsSeen[blockProducer]=True
    return reserveMax

def setActiveProducers(prodsActive, activeProducers):
    for prod in prodsActive:
        prodsActive[prod]=prod in activeProducers

def syncWithShuffle(node, blockNum):
    block = node.getBlockStateByNum(blockNum)
    prevSlot = block["block_header_state"]["scheduled_shuffle_slot"]
    slot = prevSlot
    Utils.Print("Syncing with shuffle: block=%i; slot=%i" % (blockNum, slot))
    while slot == prevSlot:
        blockNum += 1
        block = node.getBlockStateByNum(blockNum)
        slot = block["block_header_state"]["scheduled_shuffle_slot"]
    Utils.Print("Synced with shuffle: block=%i; slot=%i" % (blockNum, slot))
    return blockNum + 1     # schedule will be applied starting from the next block

def getScheduleProducers(schedule):
    prods = []
    for i in schedule["producers"]:
        prods.append(i["producer_name"])
    return prods

def waitActiveSchedule(node, prodsActive):
    Utils.Print("Wait producers to be scheduled")

    waitProds = set()
    for k,v in prodsActive.items():
        if v:
            waitProds.add(k)
    Utils.Print("wait: %s" %  ','.join(map(str, waitProds)))
    assert(len(waitProds) == top_producers)

    blockNum = node.getHeadBlockNum()
    maxBlocks = (schedulePeriodFactor + 1) * totalProducers * 2 + ((totalProducers*2//3)+1)*2
    found = None
    prevActive = prevPending = None
    prevVersion = prevPendingVersion = None
    prevSlot = None
    slotSize = -1
    synced = False
    while maxBlocks > 0:
        block = node.getBlockStateByNum(blockNum)["block_header_state"]
        slot = block["scheduled_shuffle_slot"]
        synced = slot != prevSlot
        if synced:
            Utils.Print("Slot changed: %i/%i; (previous lasted %i blocks)" % (blockNum, slot, slotSize))
            prevSlot = slot
            slotSize = 0
        slotSize += 1

        activeS = block["active_schedule"]
        active = getScheduleProducers(activeS)
        if found != "A":
            if found != "P":
                pending = getScheduleProducers(block["pending_schedule"])
                if set(pending).intersection(waitProds) == waitProds:
                    found = "P"
                    Utils.Print("Found in pending schedule; %i/%i" % (blockNum, slot))
            if set(active).intersection(waitProds) == waitProds:
                found = "A"
                Utils.Print("Found in active schedule; %i/%i" % (blockNum, slot))
                break

        if activeS["version"] != prevVersion:
            prevVersion = activeS["version"]
            prevActive = active
            Utils.Print("Changed active version: %i/%i/%i; [%s]" % (prevVersion, blockNum, slot, ','.join(map(str, active))))
        elif set(prevActive) != set(active):
            Utils.Print("Schedule changed without version change (%i/%i):\n[%s]" % (
                blockNum, slot, ','.join(map(str, active))))
            prevActive = active
        elif prevActive != active:
            Utils.Print("Schedule shuffled (%i/%i):\n[%s]" % (blockNum, slot, ','.join(map(str, active))))
            prevActive = active

        blockNum += 1
        maxBlocks -= 1
    if found != "A":
        Utils.errorExit("Failed to find producers in active schedule")
    return (blockNum, synced)

def verifyProductionRounds(trans, node, prodsActive, rounds):
    temp = Utils.Debug
    Utils.Debug = False

    blockNum, synced = waitActiveSchedule(node, prodsActive)
    if synced:
        blockNum += 1
    else:
        blockNum = syncWithShuffle(node, blockNum)

    Utils.Print("Verify %s complete rounds of all producers producing" % (rounds))
    for i in range(0, rounds):
        prodsSeen = {}
        reserveMax = reserve_producers
        for j in range(0, totalProducers):
            reserveMax = validBlockProducer(prodsActive, prodsSeen, blockNum, node, reserveMax)
            blockNum += 1

        # make sure that we have seen all `top_producers` producers
        prodsSeenKeys=prodsSeen.keys()
        if len(prodsSeenKeys)!=top_producers:
            Utils.cmdError("only saw %s producers of expected %i. At blockNum %s only the following producers were seen: %s" % (
                len(prodsSeenKeys), top_producers, blockNum, ",".join(prodsSeenKeys)))
            Utils.errorExit("Failed because of missing block producers")

    Utils.Debug = temp


def publishContract(node, contract):
    contractDir = "contracts/%s" % (contract)
    wasmFile = "%s.wasm" % (contract)
    abiFile = "%s.abi" % (contract)
    Utils.Print("Publish %s contract" % (contract))
    trans = node.publishContract(contract, contractDir, wasmFile, abiFile, waitForTransBlock=True)
    if trans is None:
        Utils.errorExit("ERROR: Failed to publish contract %s." % contract)


Print=Utils.Print
errorExit=Utils.errorExit

from core_symbol import CORE_SYMBOL

args = TestHelper.parse_args({"--prod-count","--mongodb","--dump-error-details","--keep-logs","-v","--leave-running","--clean-run",
                              "--p2p-plugin","--wallet-port"})
Utils.Debug=args.v
enableMongo=args.mongodb
totalNodes=4
cluster=Cluster(walletd=True, enableMongo=enableMongo)
dumpErrorDetails=args.dump_error_details
keepLogs=args.keep_logs
dontKill=args.leave_running
prodCount=args.prod_count
killAll=args.clean_run
p2pPlugin=args.p2p_plugin
walletPort=args.wallet_port

walletMgr=WalletMgr(True, port=walletPort)
testSuccessful=False
killEosInstances=not dontKill
killWallet=not dontKill

WalletdName=Utils.EosWalletName
ClientName="cleos"

try:
    TestHelper.printSystemInfo("BEGIN")
    cluster.setWalletMgr(walletMgr)

    cluster.killall(allInstances=killAll)
    cluster.cleanup()
    Print("Stand up cluster")
    if cluster.launch(prodCount=prodCount, onlyBios=False, pnodes=totalNodes, totalNodes=totalNodes, totalProducers=totalNodes*top_producers, p2pPlugin=p2pPlugin, useBiosBootFile=False) is False:
        Utils.cmdError("launcher")
        Utils.errorExit("Failed to stand up eos cluster.")

    Print("Validating system accounts after bootstrap")
    cluster.validateAccounts(None)

    accounts=cluster.createAccountKeys(5)
    if accounts is None:
        Utils.errorExit("FAILURE - create keys")
    accounts[0].name="tester111111"
    accounts[1].name="tester222222"
    accounts[2].name="tester333333"
    accounts[3].name="tester444444"
    accounts[4].name="tester555555"

    testWalletName="test"

    Print("Creating wallet \"%s\"." % (testWalletName))
    testWallet=walletMgr.create(testWalletName, [cluster.eosioAccount,accounts[0],accounts[1],accounts[2],accounts[3],accounts[4]])

    for _, account in cluster.defProducerAccounts.items():
        walletMgr.importKey(account, testWallet, ignoreDupKeyWarning=True)

    Print("Wallet \"%s\" password=%s." % (testWalletName, testWallet.password.encode("utf-8")))

    node = cluster.getNode(0)
    publishContract(node, "cyber.govern")
    contract="cyber.stake"
    publishContract(node, contract)

    Utils.Print("push create action to %s contract" % contract)
    action = "create"
    data = "{\"token_symbol\":\"4,%s\",\"max_proxies\":[30,10,3,1],\"payout_step_length\":604800,\"payout_steps_num\":52,\"min_own_staked_for_election\":0}" % CORE_SYMBOL
    opts = "--permission %s@active" % cluster.eosioAccount.name
    trans = node.pushMessage(contract, action, data, opts)
    if trans is None or not trans[0]:
        Utils.errorExit("ERROR: Failed to push create action to %s contract." % contract)
    node.trxTrackWait(trans[1], True, True)

    for i in range(0, totalNodes):
        node=cluster.getNode(i)
        node.producers=Cluster.parseProducers(i)
        for prod in node.producers:
            trans=node.stakeOpen(cluster.defProducerAccounts[prod].name)
    node.trxTrackWait(trans[1], True, True)

    for i in range(0, totalNodes):
        node=cluster.getNode(i)
        node.producers=Cluster.parseProducers(i)
        for prod in node.producers:
            acc = cluster.defProducerAccounts[prod]
            trans=node.regproducer(acc, "http::/mysite.com", 0, exitOnError=True)
    node.trxTrackWait(trans, True, True)

    node0=cluster.getNode(0)
    node1=cluster.getNode(1)
    node2=cluster.getNode(2)
    node3=cluster.getNode(3)

    node=node0
    fund_sum = 100000000.0
    stake_sum = 40000000.0
    # create accounts via eosio as otherwise a bid is needed
    for account in accounts:
        Print("Create new account %s via %s" % (account.name, cluster.eosioAccount.name))
        trans=node.createInitializeAccount(account, cluster.eosioAccount, stakedDeposit=0, waitForTransBlock=False, stakeNet=1000, stakeCPU=1000, buyRAM=1000, exitOnError=True)
        transferAmount="%.4f %s" % (fund_sum, CORE_SYMBOL)
        Print("Transfer funds %s from account %s to %s" % (transferAmount, cluster.eosioAccount.name, account.name))
        node.transferFunds(cluster.eosioAccount, account, transferAmount, "test transfer")
        #trans=node.delegatebw(account, 20000000.0000, 20000000.0000, waitForTransBlock=True, exitOnError=True)
        node.stakeFunds(account, "%.4f" % stake_sum, cluster.eosioStakeAccount, waitForTransBlock=True, exitOnError=True)
        node.setProxyLevel(account, 1, exitOnError=True)

    # containers for tracking producers
    prodsActive={}
    for i in range(0, 4):
        node=cluster.getNode(i)
        ProducerToNode.populate(node, i)
        for prod in node.producers:
            prodsActive[prod]=False

    #first account will vote for node0 producers, all others will vote for node1 producers
    node=node0
    for account in accounts:
        trans=node.voteProds(account.name, node.producers, stake_sum, waitForTransBlock=True)
        node=node1

    # node.stakeEnable(issuer=cluster.eosioAccount.name)    # not required for this test

    setActiveProducers(prodsActive, node1.producers)

    verifyProductionRounds(trans, node2, prodsActive, 2)

    # remove votes. do as separate loop because pairing depending transactions (recall+delegate) is less stable
    node=node0
    for account in accounts:
        trans=node.unvoteProds(account.name, node.producers)
        node=node1

    # test shifting all `top_producers` away from one node to another
    # first account will vote for node2 producers, all others will vote for node3 producers
    node1
    for account in accounts:
        trans=node.voteProds(account.name, node.producers, stake_sum, waitForTransBlock=True)
        node=node2

    setActiveProducers(prodsActive, node2.producers)

    verifyProductionRounds(trans, node1, prodsActive, 2)

    testSuccessful=True
finally:
    TestHelper.shutdown(cluster, walletMgr, testSuccessful=testSuccessful, killEosInstances=killEosInstances, killWallet=killWallet, keepLogs=keepLogs, cleanRun=killAll, dumpErrorDetails=dumpErrorDetails)

exit(0)
