from Suite.Framework.NodeosApiTest import WalletTestSuite

from os import path
import sys

if __name__ == '__main__':
    currentDir = path.dirname(path.abspath(__file__))
    sys.exit(not WalletTestSuite().execute(currentDir))
