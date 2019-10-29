import sys
import json

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

def jsonArg(a):
    return " '" + json.dumps(a) + "' "