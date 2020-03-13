import unittest
import argparse
import re
import atexit
import sys

from os import listdir
from os.path import isfile

from Suite.Tools.Nodeos import Nodeos
from Suite.Tools.Cleos import Cleos
from Suite.Tools.Wallet import Wallet
from Suite.Tools.ApiClient import ApiClient
from Suite.Tools.ContractsManager import ContractsManager
from Suite.Framework.eprint import eprint

class NodeosApiTest:
    instance = None

    def __init__(self):
        type(self).instance = self
        self.argParser = argparse.ArgumentParser(description='The nodeos api test suite')
        self.argParser.add_argument('--nodeos', dest='nodeos', help='A path to the nodeos')
        self.argParser.add_argument('--mongo', dest='mongo', default='127.0.0.1:27017', help='A maongo db address (default 127.0.0.1:27017)')
        self.argParser.add_argument('--nodeos-output', action='store_true', dest='nodeosOutput', help='Show the nodeos output')
        self.argParser.add_argument('--api-client-dialog', action='store_true', dest='apiClientDialog',
                                    help='Show an api client dialog with a nodeos')

        self.argParser.add_argument('--contracts', dest='contracts', default=[], nargs='+',
                                    help='Directory with contract used in test. Contract name will be got from the last path segment')

        self.argParser.add_argument('--skip', dest='skip', default=[], nargs='+',
                                    help='A list of tests to skip.')

    def execute(self, path):
        args, unknown = self.prepareExecutables()
        self.tests = self.getTests(path)
        return self.runUnitTests(unknown)

    def prepareExecutables(self):
        args, unknown = self.argParser.parse_known_args()
        self.skip = args.skip
        self.contractsManager = ContractsManager(args.contracts)
        self.apiClient = ApiClient("127.0.0.1", 8888, args.apiClientDialog)

        if args.nodeos == None:
            return args, unknown

        eprint("Starting a nodeos instance...")
        self.nodeos = Nodeos(args.nodeos, args.mongo, args.nodeosOutput)
        atexit.register(self.nodeos.stop)

        eprint("The nodeos is ready")
        return args, unknown

    def getTests(self, path):
        content = listdir(path)

        tests = []
        for file in content:
            if isfile(path + '/' + file) and re.fullmatch('[a-zA-Z0-9.]*Test.py', file):
                test = file[:-3]
                if self.skipTest(test):
                    eprint("Skipped test: " + test)
                else :
                    tests.append(test)
                    eprint("Found test: " + test)

        return tests

    def skipTest(self, test):
        return test in self.skip

    def runUnitTests(self, args):
        sys.argv = [sys.argv[0]]
        sys.argv.extend(args)

        suites = []
        for test in self.tests:
            try:
                suites.append((test, unittest.defaultTestLoader.loadTestsFromName(test)))
            except ModuleNotFoundError:
                eprint("Could not execute test: " + test)


        testRunner = unittest.TextTestRunner(resultclass=unittest.TextTestResult)
        successful = True
        for testName, suite in suites:
            eprint("Starting test: " + testName)
            result = testRunner.run(suite)

            successful = successful and result.wasSuccessful()

        return successful

class CleosTestSuite(NodeosApiTest):
    def __init__(self):
        NodeosApiTest.__init__(self)
        NodeosApiTest.instance = self
        self.argParser.description = 'The nodeos api test suite with the cleos client'
        self.argParser.add_argument('--cleos', dest='cleos', required=True, help='A path to the cleos')
        self.argParser.add_argument('--remote-host', dest='remoteHost', default='127.0.0.1:8888', help='A host to send cleos requests')
        self.argParser.add_argument('--cleos-dialog', action='store_true', dest='cleosDialog', help='Show the cleos commands and output')

    def prepareExecutables(self):
        args, unknown = NodeosApiTest.prepareExecutables(self)
        self.cleos = Cleos(args.cleos, args.remoteHost, args.cleosDialog)
        return args, unknown

class WalletTestSuite(CleosTestSuite):
    def __init__(self):
        CleosTestSuite.__init__(self)
        NodeosApiTest.instance = self
        self.argParser.description = 'The nodeos api test suite with the cleos client and keosd wallet support'

    def prepareExecutables(self):
        args, unknown = CleosTestSuite.prepareExecutables(self)
        eprint("Preparing a wallet...")
        self.wallet = Wallet(self.cleos)
        eprint("The wallet is ready")
        return args, unknown

