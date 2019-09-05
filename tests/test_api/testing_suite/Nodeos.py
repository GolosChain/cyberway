#!/usr/bin/python3
import subprocess
import shutil
import os


class Nodeos:
    workingDir = "./"
    dataDir = workingDir + "data.dir"

    def __init__(self, path, mongo, enableOutput):
        self.enableOutput = enableOutput
        self.clearWorkingDir()
        self.startNodeos(path, mongo)

    def clearWorkingDir(self):
        shutil.rmtree(self.dataDir, ignore_errors=True)

        configFilePath = self.workingDir + "config.ini"

        if os.path.exists(configFilePath):
            os.remove(configFilePath)

    def startNodeos(self, path, mongo):
        self.nodeos = subprocess.Popen([path, '--chaindb_address', "mongodb://" + mongo, "--contracts-console", "--config-dir", self.workingDir, "-d", self.dataDir, "-e", "--producer-name", "cyber", "--plugin", "eosio::chain_api_plugin"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        for line in self.nodeos.stdout:
            stringLine = line.decode("UTF-8")

            if self.enableOutput == True:
                print(stringLine)

            if "Produced block" in stringLine:
                return


    def stop(self):
        self.nodeos.wait()
        self.clearWorkingDir()
