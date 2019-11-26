from Suite.Framework.NodeosApiTest import WalletTestSuite

from os import path

if __name__ == '__main__':
    currentDir = path.dirname(path.abspath(__file__))
    WalletTestSuite().execute(currentDir)
