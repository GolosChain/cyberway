import os

from pathlib import Path

class Wallet:
    walletName = "test_suite_wallet"
    mainKeyName = "main_active_key"

    def __init__(self, cleos):
        self.cleos = cleos
        self.keys = {}

        self.createWallet()
        self.importKey("GLS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV", "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3", self.mainKeyName)

    def createWallet(self):
        self.removeWallet()
        walletCreateResult = self.cleos.exec("wallet create --to-console -n {walletName}".format(walletName = self.walletName))
        self.password = self.parseCreateWalletResult(walletCreateResult)

        walletUnlockResult = self.cleos.exec("wallet unlock --password {password} -n {walletName}".format(password = self.password, walletName = self.walletName))
        self.verifyUnlocked(walletUnlockResult)

    def removeWallet(self):
        home = str(Path.home())
        walletPath = home + "/eosio-wallet/" + self.walletName + ".wallet"

        if os.path.exists(walletPath):
            os.remove(walletPath)

    def parseCreateWalletResult(self, walletCreateResult):
        for i in range(len(walletCreateResult)):
            if "Without password imported keys will not be retrievable." in walletCreateResult[i]:
                return walletCreateResult[i + 1].replace('"', '')

        raise NameError("Could not create the test wallet: " + self.walletName)

    def verifyUnlocked(self, walletUnlockResult):
        for line in walletUnlockResult:
            if ("Unlocked:" in line and self.walletName in line) or ("Already unlocked" in line):
                return

        raise NameError("Could not unlock the wallet: " + self.walletName)

    def createKeys(self, keyName):
        createKeysResult = self.cleos.exec("create key --to-console")
        publicKey, privateKey = self.getKeysFromCreateResult(createKeysResult)

        return self.importKey(publicKey, privateKey, keyName)

    def importKey(self, publicKey, privateKey, keyName):
        importResult = self.cleos.exec("wallet import -n {walletName} --private-key {privateKey}".format(walletName = self.walletName, privateKey = privateKey))
        self.verifyKeyImported(importResult, publicKey)

        self.keys[keyName] = publicKey
        return publicKey

    def getKeysFromCreateResult(self, createKeysResult):
        privateKeyHeader = "Private key:"
        publicKeyHeader = "Public key:"
        privateKey = None
        publicKey = None
        for line in createKeysResult:
            if privateKeyHeader in line:
                privateKey = line[len(privateKeyHeader):].replace(' ', '')
            if publicKeyHeader in line:
                publicKey = line[len(publicKeyHeader):].replace(' ', '')

        if privateKey == None or publicKey == None:
            raise NameError('Could not create keys')

        return publicKey, privateKey

    def verifyKeyImported(self, importResult, publicKey):
        for line in importResult:
            if "imported private key for:" in line and publicKey in line:
                return

            raise NameError("Could not import private key for the public key: " + publicKey)

    def getKey(self, keyName):
        return self.keys[keyName]