import re
import unittest

def verifyOutputContains(output, value):
    for line in output:
        if value in line:
            return

    unittest.TestCase.assertTrue(unittest.TestCase(), False, "The value \'" + value + "\' not found in the cleos output")


def verifyOutputContainsRegex(output, regex):
    for line in output:
        if re.match(regex, line):
            return

    unittest.TestCase.assertTrue(unittest.TestCase(), False, "The regexp \'" + regex + "\' does not match the cleos output")


def verifyOutputNotContains(output, value):
    for line in output:
        unittest.TestCase.assertNotIn(unittest.TestCase(), value, line, "The value \'" + value + "\' should not be in the cleos output")