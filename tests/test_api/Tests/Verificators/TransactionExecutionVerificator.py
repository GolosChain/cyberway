from Tests.Verificators.OutputBaseVerificator import *

def verifyAccountCreated(output):
    verifyOutputContains(output, "cyber <= cyber::newaccount")


def verifyContractWasSet(output):
    verifyOutputContains(output, "cyber <= cyber::setcode")
    verifyOutputContains(output, "cyber <= cyber::setabi")

def verifyTokenIssued(output, receiver, amount):
    verifyOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::issue[ ]+{\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
    verifyOutputContainsRegex(output, "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"cyber\",\"to\":\"" + receiver + "\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")


def verifyStakeDelegated(output, grantor, receiver, quantity):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")

def verifyStakeOpened(output, owner):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::open[ ]+{\"owner\":\"" + owner + "\",\"token_code\":\"CYBER\",\"ram_payer\":null}")


def verifyTokensStaked(output, stakeHolder, amount):
    verifyOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
    verifyOutputContainsRegex(output, "#[ ]+alice <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"\"")


def verifyStakeEnabled(output):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::enable[ ]+{\"token_symbol\":\"4,CYBER\"}")
