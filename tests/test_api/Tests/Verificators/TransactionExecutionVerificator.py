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

def verifyTokenUnstaked(output, stakeHolder, quantity):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::withdraw[ ]+{\"account\":\"" + stakeHolder + "\",\"quantity\":\"" + quantity + "\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber <= cyber.stake::withdraw[ ]+{\"account\":\"" + stakeHolder + "\",\"quantity\":\"" + quantity + "\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"cyber.stake\",\"to\":\"" + stakeHolder + "\",\"quantity\":\"" + quantity + "\",\"memo\":\"unstaked tokens\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.token::transfer[ ]+{\"from\":\"cyber.stake\",\"to\":\"" + stakeHolder + "\",\"quantity\":\"" + quantity + "\",\"memo\":\"unstaked tokens\"")
    verifyOutputContainsRegex(output, "#[ ]+" + stakeHolder + " <= cyber.token::transfer[ ]+{\"from\":\"cyber.stake\",\"to\":\"" + stakeHolder + "\",\"quantity\":\"" + quantity + "\",\"memo\":\"unstaked tokens\"}")

def verifyStakeDelegated(output, grantor, receiver, quantity):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber <= cyber.stake::delegateuse[ ]+{\"grantor_name\":\"" + grantor + "\",\"recipient_name\":\""+ receiver + "\",\"quantity\":\""+ quantity +"\"}")

def verifyStakeTransfered(output, grantor, receiver, quantity):
    verifyTokenUnstaked(output, grantor, quantity)
    verifyTokensStaked(output, grantor, quantity, receiver)

def verifyStakeOpened(output, owner):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::open[ ]+{\"owner\":\"" + owner + "\",\"token_code\":\"CYBER\",\"ram_payer\":null}")


def verifyTokensStaked(output, stakeHolder, amount, receiver =""):
    verifyOutputContainsRegex(output, "#[ ]+cyber.token <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"" + receiver + "\"}")
    verifyOutputContainsRegex(output, "#[ ]+" + stakeHolder + " <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"" + receiver + "\"}")
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.token::transfer[ ]+{\"from\":\"" + stakeHolder + "\",\"to\":\"cyber.stake\",\"quantity\":\"" + amount + "\",\"memo\":\"" + receiver + "\"")


def verifyStakeEnabled(output):
    verifyOutputContainsRegex(output, "#[ ]+cyber.stake <= cyber.stake::enable[ ]+{\"token_code\":\"CYBER\"}")
