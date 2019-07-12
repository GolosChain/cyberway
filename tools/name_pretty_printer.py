# Pretty_printer for gdb name types.
# Execute: `source name_pretty_printer.py` to enable it.

import gdb

def name_to_string(value):
    charmap = ".12345abcdefghijklmnopqrstuvwxyz"
    name = ''
    tmp = value
    ch = charmap[int(tmp & 0xF)]
    if ch != '.':
        name = ch
    tmp /= 16
    for i in range(0,12):
        if tmp == 0:
            break
        ch = charmap[int(tmp & 0x1F)]
        if ch != '.' or name != '':
            name = ch + name
        tmp /= 32
    return "'"+name+"'"

class NamePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        if self.val.type.code == gdb.TYPE_CODE_REF:
            self.val = self.val.referenced_value()
        return name_to_string(self.val['value'])

class IntNamePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return name_to_string(self.val)

def check_name(val):
    type = val.type
    if type.code == gdb.TYPE_CODE_REF:
        type = type.target()

    if type.name == 'cyberway::chaindb::account_name_t' or type.name == 'cyberway::chaindb::table_name_t':
        return IntNamePrinter(val)

    type = type.unqualified().strip_typedefs()

    typename = type.tag
    if typename == 'eosio::chain::name':
        return NamePrinter(val) 

    return None

gdb.pretty_printers.insert(0, check_name)
