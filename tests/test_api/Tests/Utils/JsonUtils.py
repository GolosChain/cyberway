import json

from testfixtures import *

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def jsonToPython(jsonString):
    jsonDocumemt = json.loads(jsonString)

    if type(jsonDocumemt) is dict:
        return Object(jsonDocumemt)
    if type(jsonDocumemt) is list:
        return Object.toPythonArray(jsonDocumemt)

class Object:
    def __init__(self, dictionary):
        for key, value in dictionary.items():
            if type(value) is dict:
                setattr(self, key, Object(value))
            elif type(value) is list:
                setattr(self, key, Object.toPythonArray(value))
            elif type(value) is str and (value == 'true' or value == 'false'):
                setattr(self, key, value == 'true')
            else:
                setattr(self, key, value)

    def __eq__(self, other):
        return Comparison(self) == other

    def __ne__(self, other):
        return not self.__eq__(other)

    @staticmethod
    def toPythonArray(jsonArray):
        array = []
        for val in jsonArray:
            array.append(Object(val))

        return array
