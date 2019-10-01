import subprocess
import shlex

class Cleos:
    def __init__(self, path, remoteHost, printDialog):
        self.path = path
        self.remoteHost = remoteHost
        self.printDialog = printDialog

    def exec(self, *params):
        execTuple = (self.path, '-u', "http://" + self.remoteHost, *params)

        if self.printDialog == True:
            self.showCommand(execTuple)

        cleos = subprocess.Popen(execTuple, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

        output = []
        for line in cleos.stdout:
            stringLine = line.decode("UTF-8")

            if self.printDialog == True:
                print(stringLine)

            output.append(stringLine)

        cleos.stdout.close()
        cleos.wait()

        return output

    def showCommand(self, execTuple):
        printString = ""
        for element in execTuple:
            printString += element + " "

        print(printString)