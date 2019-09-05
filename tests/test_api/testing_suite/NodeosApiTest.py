import sys
import unittest
import argparse

from testing_suite.Nodeos import Nodeos
from testing_suite.Cleos import Cleos
from testing_suite.Wallet import Wallet
from testing_suite.ApiClient import ApiClient
from testing_suite.ContractsManager import ContractsManager

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
                                    help='Contracts neccessary for testing. Syntax: --contract-dir <contract name used by test> <path>.')


    def prepareExecutables(self):
        print("Starting a nodeos instance...")
        args, unknown = self.argParser.parse_known_args()

        if args.nodeos == None:
            return

        self.nodeos = Nodeos(args.nodeos, args.mongo, args.nodeosOutput)

        if len(args.contracts) % 2 != 0:
            raise NameError("Wrong --contract-dir parameter format: must be --contract dir <name> <path> [<name> <path>]")

        self.contractsManager = ContractsManager(args.contracts)
        self.apiClient = ApiClient("127.0.0.1", 8888, args.apiClientDialog)
        print("The nodeos is ready")
        return args, unknown

    def execute(self):
        args, unknown = self.prepareExecutables()
        self.startUnitTests(unknown)

    def startUnitTests(self, args):
        sys.argv = [sys.argv[0]]
        sys.argv.extend(args)
        unittest.main()
        self.nodeos.stop()


class CleosTestSuite(NodeosApiTest):
    def __init__(self):
        NodeosApiTest.__init__(self)
        NodeosApiTest.instance = self
        self.argParser.description = 'The nodeos api test suite with the cleos client'
        self.argParser.add_argument(dest='cleos' , help='A path to the cleos')
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
        print("Preparing a wallet...")
        self.wallet = Wallet(self.cleos)
        print("The wallet is ready")
        return args, unknown

