#!/usr/bin/python3

# Tests for Event Engine.
# Preconditions:
# - nodeos run with enabled event_engine_plugin
# - event_engine_plugin write events in `events.dump` which located (or symlinked) in current directory
# - `test` account created and contains `ee.test` contract
# - wallet unlocked (with keys for `test` account)

import subprocess
import unittest
import time
import json

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def cleos(arguments):
    return subprocess.check_output("programs/cleos/cleos " + arguments, shell=True, universal_newlines=True)

def pushAction(code, action, actor, args, *, delay=None, expiration=None):
    additional = ''
    if delay:
        additional += ' --delay-sec %d' % delay
    if expiration:
        additional += ' --expiration %d' % expiration
    return json.loads(cleos('push action -j %s %s %s -p %s %s' % (code, action, jsonArg(args), actor, additional)))



# Class for save element from events with `name` into `params` dictionary
class Save:
    def __init__(self, params, name):
        self.params = params
        self.name = name

    def __repr__(self):
        return 'Save "%s"' % self.name

# Compare array with set of args (without order)
class Unorder:
    def __init__(self, *args):
        self.values = args

    def __repr__(self):
        return str(self.values)

# Compare array with args (all items with specified order)
class AllItems:
    def __init__(self, *args):
        self.values = args

    def __repr__(self):
        return str(self.values)

# Compare template with events exactly
class Exactly:
    def __init__(self, value):
        self.value = value

    def __repr__(self):
        return str(self.value)



def sendDeferredTrace(action, arg, sender_id, delay, replace, params):
    def_action_in_event = {
        'account':'test', 'name':action, 
        'authorization':AllItems({'actor':'test', 'permission':'active'}), 
        'data':{'arg':arg}
    }
    return {
        "receiver":"test", "code":"test", "action":"senddeferred",
        "auth":[{"actor":"test","permission":"active"}],
        "args":{"action":action, "arg":arg, "senderid":sender_id, "delay":delay, "replace":replace},
        "events":AllItems(
            {   'code':'', 'event':'senddeferred',
                'args':{
                    "sender":"test", 'sender_id':sender_id,
                    'trx':{'delay_sec':delay, 'context_free_actions':Exactly([]), 'actions':AllItems(def_action_in_event)},
                    "trx_id":Save(params, 'def_trx_id'),
                    'packed_trx':Save(params, 'def_packed_trx')
                },
            }
        )
    }
def cancelDeferTrace(sender_id):
    return {
        "receiver":"test", "code":"test", "action":"canceldefer",
        "auth":[{"actor":"test","permission":"active"}],
        "args":{"senderid":sender_id},
        "events":AllItems(
            {"code":"", "event":"canceldefer", "args":{"sender":"test", 'sender_id':sender_id}}
        )
    }
def actionTrace(action, arg):
    return {
        'receiver':'test', 'code':'test', 'action':action, 
        'auth':[{'actor':'test', 'permission':'active'}], 
        'args':{'arg':arg}, 'events':[]
    }

class EventEngineTester(unittest.TestCase):
    def setUp(self):
        self.eventsFd = open("events.dump", "r")
        self.resetEvents()

    def tearDown(self):
        self.eventsFd.close()

    def resetEvents(self):
        self.eventsFd.seek(0, 2)

    def checkContains(self, template, message):
        try:
            self.assertContains(template, message)
            return True
        except:
            return False

    def waitEvents(self, events, maxBlockNum):
        i = 0
        lines = []
        while i < len(events):
            (selector, predicat) = events[i]
            for line in self.eventsFd:
                lines.append(line)
                msg = json.loads(line)
                if self.checkContains(selector, msg):
                    self._assertContains('events[%d]'%i, predicat, msg)
                    print('Found message for %d event: %s' % (i, selector))
                    i += 1
                    lines = []
                    break
                if msg['msg_type'] == 'AcceptBlock' and msg['block_num'] >= maxBlockNum:
                    self.fail('Missed events at block %d: %s\n\nReaden lines:\n%s' % (maxBlockNum, events[i:], ''.join(lines)))
                time.sleep(1)

    def _assertContains(self, path, templ, value, msg=None):
        if templ is None:
            self.assertIs(value, None, '%s' % path)
        elif type(templ) is Exactly:
            self.assertEqual(value, templ.value, '%s' % path)
        elif type(templ) is Save:
            templ.params[templ.name] = value
        elif type(templ) is type({}):
            self.assertEqual(type(value), type({}), path)
            for key,val in templ.items():
                npath='%s["%s"]'%(path,key)
                self.assertIn(key, value, npath)
                self._assertContains(npath, val, value[key])
        elif type(templ) is type([]):
            self.assertEqual(type(value), type([]), path)
            i = 0
            j = 0
            while i < len(templ):
                npath='%s[%d]' % (path, i)
                errors = []
                while j < len(value):
                    try:
                        self._assertContains(npath, templ[i], value[j])
                        break
                    except AssertionError as err:
                        errors.append(str(err))
                        pass
                    j += 1
                if j == len(value):
                    self.fail("%s doesn't contains %s : %s\nChecked items:\n%s" % (value, templ[i], path, '\n'.join(errors)))
                i += 1
        elif type(templ) is Unorder or type(templ) is AllItems:
            self.assertEqual(type(value), type([]), path)
            if type(templ) is AllItems and len(templ.values) != len(value):
                self.fail("Different items count: template %d, actual %d : %s" % (len(templ.values), len(value), path))
            was = set()
            i = 0
            for t in templ.values:
                npath='%s[%d]' % (path, i)
                j = 0
                errors = []
                while j < len(value):
                    if j not in was:
                        try:
                            self._assertContains(npath, t, value[j])
                            was.add(j)
                            break
                        except AssertionError as err:
                            errors.append(str(err))
                            pass
                    j += 1
                if j == len(value):
                    self.fail("%s doesn't contains %s : %s\nChecked items:\n%s" % (value, t, path, '\n'.join(errors)))
                i += 1
        else:
            self.assertEqual(type(value), type(templ), '%s' % path)
            self.assertEqual(value, templ, '%s' % path)

    def assertContains(self, templ, value, msg=None):
        self._assertContains('', templ, value)

    def assertContainsSuccess(self, templ, value):
        try:
            self.assertContains(templ, value)
        except AssertionError:
            self.fail("assertContains(%s, %s) raises exception unexpectedly" % (templ, value))

    def assertContainsFail(self, templ, value):
        with self.assertRaises(self.failureException):
            self.assertContains(templ, value);

    def test_assertContains(self):
        self.assertContainsSuccess(None, None)
        self.assertContainsFail(None, 10)
        self.assertContainsFail(None, {})

        self.assertContainsSuccess(Exactly([1,2,3]), [1,2,3])
        self.assertContainsFail(Exactly([1,2,3]), [1,2])

        obj = {"field":1234, "another":"field", "array":[1,2,3,4], "sub":{"f":10,"g":False}}
        self.assertContainsSuccess({"field":1234}, obj)
        self.assertContainsFail({"field":123}, obj)
        self.assertContainsFail({"fiel":1234}, obj)
        self.assertContainsSuccess({"sub":{}}, obj)
        self.assertContainsSuccess({"sub":{"g":False}}, obj)
        self.assertContainsFail({"sub":{"g":True}}, obj)

        self.assertContainsSuccess({"array":[2,4]}, obj)
        self.assertContainsFail({"array":[4,2]}, obj)
        self.assertContainsFail({"array":[0]}, obj)
        self.assertContainsFail({"array":[5]}, obj)
        self.assertContainsSuccess({"array":Unorder(2,4)}, obj)
        self.assertContainsSuccess({"array":Unorder(4,2)}, obj)
        self.assertContainsFail({"array":Unorder(2,4,5)}, obj)
        self.assertContainsFail({"array":Unorder(2,2)}, obj)


        self.assertContainsSuccess(
            {'block_num': 7417, 'trxs': Unorder({'id': '8aa6e455d4c02b5f570ad638581771732ffec2623ee6532c597a843b659a8d7c', 'status': 'executed'}), 'msg_type': 'AcceptBlock'},
            {'validated': True, 'events': [], 'block_time': '2019-06-28T02:02:33.000', 'in_current_chain': True, 'trxs': [{'storage_kbytes': 0, 'cpu_usage_us': 403, 'ram_kbytes': 1, 'id': '8aa6e455d4c02b5f570ad638581771732ffec2623ee6532c597a843b659a8d7c', 'net_usage_words': 13, 'status': 'executed'}], 'previous': '00001cf8c5644fd99349d3a9c838c5764b43ab19f08d5963ef2a614bea369af6', 'id': '00001cf9f562c8892281558313a909c54ec4f411b152dfeab4886cb6b4c15ac2', 'block_num': 7417, 'msg_type': 'AcceptBlock'})

        params = {}
        self.assertContainsSuccess(
            {'id': Save(params, "onerror_trx_id"), 'msg_type': 'ApplyTrx', 'actions': [{'args': {'sent_trx': '8c7e195d09036077a32400000000050001000000000090b1ca000000000088544301000000000090b1ca00000000a8ed323208000000000000000000', 'sender_id': 12340}, 'data': '', 'receiver': 'test', 'events': [], 'code': 'cyber', 'auth': [{'actor': 'test', 'permission': 'active'}], 'action': 'onerror'}]},
            {'id': 'c31ef3cb7bd820bc8bc90f21eb5ef01af08763839f982048e3452ce5be8e61ba', 'block_time': '2019-07-01T03:31:30.000', 'msg_type': 'ApplyTrx', 'actions': [{'args': {'sent_trx': '8c7e195d09036077a32400000000050001000000000090b1ca000000000088544301000000000090b1ca00000000a8ed323208000000000000000000', 'sender_id': 12340}, 'data': '', 'receiver': 'test', 'events': [], 'code': 'cyber', 'auth': [{'actor': 'test', 'permission': 'active'}], 'action': 'onerror'}], 'block_num': 780})

        

    def test_type(self):
        none = None
        self.assertTrue(none is None)
        self.assertTrue(type("string") is str)
        self.assertTrue(type(1234) is int)
        self.assertTrue(type(123.0) is float)
        self.assertTrue(type(False) is bool)
        self.assertTrue(type([]) is type([120, 2340]))
        self.assertTrue(type(()) is type((1, 2, 3)))
        self.assertTrue(type({}) is type({"hello":"world"}))

        self.assertTrue(type([120, 2340]) is type([]))
        self.assertTrue(type((1, 2, 3)) is type(()))
        self.assertTrue(type({"hello":"world"}) is type({}))

        self.assertFalse(type([1,2,3]) is type(()))
        self.assertFalse(type([1,2,3]) is type({}))
        self.assertFalse(type((1,2,3)) is type({}))

        self.assertTrue(type(Exactly(None)) is type(Exactly("string")))
        self.assertFalse(type(Exactly(None)) is None)



    def test_userSendTransaction(self):
        (action, arg) = ('check', 10)
        result = pushAction('test', action, 'test', [arg])
        print("Pushed transaction with %s" % result['transaction_id'])
        self.assertEqual(result['processed']['receipt']['status'], 'executed')

        trx_id = result['transaction_id']
        block_num = result['processed']['block_num']
        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':trx_id},                          {'accepted':True, 'implicit':False, 'scheduled':False}),
              ({'msg_type':'ApplyTrx', 'id':trx_id},                           {'block_num':block_num, 'actions':AllItems(actionTrace(action,arg))}),
              ({'msg_type':'AcceptBlock', 'block_num':block_num},              {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
            ], block_num)

#    def test_userSendDummyTransaction(self):
#        result = pushAction('test', 'dummy', 'test', [5])
#        print("Pushed transaction with %s" % result['transaction_id'])
#        self.assertEqual(result['processed']['receipt']['status'], 'executed')
#
#        trx_id = result['transaction_id']
#        block_num = result['processed']['block_num']
#        self.waitEvents(
#            [ ({'msg_type':'AcceptTrx', 'id':trx_id},                          {'accepted':True, 'implicit':False, 'scheduled':False}),
#              ({'msg_type':'ApplyTrx', 'id':trx_id},                           {'block_num':block_num, 'actions':AllItems(checkTrace(arg))}),
#              ({'msg_type':'AcceptBlock', 'block_num':block_num},              {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
#            ], block_num)

    def test_userScheduleTransaction(self):
        cases = (('Failure execucted', 'check', 0, 'hard_fail'),
                 ('Success executed',  'check', 1, 'executed'),
                 #('Expired',           'dummy', 1, 'expired'),
                )
        for (name, action, arg, status) in cases:
          with self.subTest(name=name):
            result = pushAction('test', action, 'test', [arg], delay=10)
            print("Pushed transaction with id %s" % result['transaction_id'])
            self.assertEqual(result['processed']['receipt']['status'], 'delayed')
    
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            exec_block_num = block_num + (10+2)//3
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':Exactly([])}),
                  ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'delayed'})}),
                  ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':True}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':exec_block_num, 'actions':[actionTrace(action, arg)]}),
                  ({'msg_type':'AcceptBlock', 'trxs':[{'id':trx_id}]},         {'trxs':Unorder({'id':trx_id, 'status':status})}),
                ], exec_block_num + 600//3+1)

    def test_contractScheduleTransaction(self):
        cases = (('Failure execucted', 'check', 0, 5, 60, 12340, 'soft_fail'),
                 ('Success executed',  'check', 1, 5, 60, 12341, 'executed'))
        for (name, action, arg, delay, expiration, sender_id, status) in cases:
          with self.subTest(name=name):
            result = pushAction('test', 'senddeferred', 'test', [action, arg, sender_id, delay, False, expiration])
            print("Pushed transaction with %s" % result['transaction_id'])
            print(json.dumps(result, indent=4))
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
    
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            exec_block_num = block_num + (delay+2)//3
    
            params = {}
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':AllItems(sendDeferredTrace(action, arg, sender_id, delay, False, params))}),
                  ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
                ], block_num)
    
            def_trx_id = params['def_trx_id']
            def_packed_trx = params['def_packed_trx']
    
            if status == 'executed':
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':exec_block_num, 'actions':AllItems(actionTrace(action, arg))}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':def_trx_id, 'status':status})})
                    ], exec_block_num)
            elif status == 'soft_fail':
                onerror_id = {'receiver':'test', 'code':'cyber', 'action':'onerror', 'args':{'sender_id':sender_id}}
                onerror_action = {
                    'receiver':'test', 'code':'cyber', 'action':'onerror', 
                    'auth':AllItems({'actor':'test', 'permission':'active'}), 
                    'args':{'sender_id':sender_id, 'sent_trx':def_packed_trx}, 
                    'events':Exactly([])}
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True}),
                      ({'msg_type':'ApplyTrx', 'actions':[onerror_id]},        {'id':Save(params,'onerror_trx_id'), 'actions':AllItems(onerror_action)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':def_trx_id, 'status':status})})
                    ], exec_block_num)

    def test_contractCancelTransaction(self):
        (action, arg, delay, sender_id, expiration) = ('check', 10, 20, 123411, 60)
        result = pushAction('test', 'senddeferred', 'test', [action, arg, sender_id, delay, False, expiration])
        print(json.dumps(result, indent=4))
        self.assertEqual(result['processed']['receipt']['status'], 'executed')
        trx_id = result['transaction_id']
        block_num = result['processed']['block_num']
        print("Pushed transaction with %s in block %d" % (trx_id, block_num))

        params = {}
        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False}),
              ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':AllItems(sendDeferredTrace(action, arg, sender_id, delay, False, params))}),
              ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
            ], block_num)

        result = pushAction('test', 'canceldefer', 'test', [sender_id])
        self.assertEqual(result['processed']['receipt']['status'], 'executed')
        cancel_trx_id = result['transaction_id']
        cancel_block_num = result['processed']['block_num']
        print("Pushed transaction with %s in block %d" % (cancel_trx_id, cancel_block_num))

        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':cancel_trx_id},                   {'accepted':True, 'implicit':False, 'scheduled':False}),
              ({'msg_type':'ApplyTrx', 'id':cancel_trx_id},                    {'block_num':cancel_block_num, 'actions':[cancelDeferTrace(sender_id)]}),
              ({'msg_type':'AcceptBlock', 'block_num':cancel_block_num},       {'trxs':Unorder({'id':cancel_trx_id, 'status':'executed'})})
            ], cancel_block_num)


    def test_contractReplaceTransaction(self):
        cases = (('Failure execucted', 10, 0, 60, 6, 60, 12340, 'soft_fail'),
                 ('Success executed',  11, 5, 60, 6, 60, 12341, 'executed'))
        for (name, arg, rep_arg, delay, rep_delay, expiration, sender_id, status) in cases:
          with self.subTest(name=name):
            action = 'check'
            print("Send original scheduled transaction")
            result = pushAction('test', 'senddeferred', 'test', [action, arg, sender_id, delay, False, expiration])
            print(json.dumps(result, indent=4))
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            print("Pushed transaction with %s in block %d" % (trx_id, block_num))
    
            params = {}
            rep_params = {}
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':[sendDeferredTrace(action, arg, sender_id, delay, False, params)]}),
                  ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
                ], block_num)
    
            print("Replace scheduled transaction")
            result = pushAction('test', 'senddeferred', 'test', [action, rep_arg, sender_id, rep_delay, True, expiration])
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
            rep_trx_id = result['transaction_id']
            rep_block_num = result['processed']['block_num']
            exec_block_num = rep_block_num + (rep_delay+2)//3
            print("Pushed transaction with %s in block %d" % (trx_id, block_num))
    
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':rep_trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False}),
                  ({'msg_type':'ApplyTrx', 'id':rep_trx_id},                       {'block_num':rep_block_num, 'actions':[sendDeferredTrace(action, rep_arg, sender_id, rep_delay, True, rep_params)]}),
                  ({'msg_type':'AcceptBlock', 'block_num':rep_block_num},          {'trxs':Unorder({'id':rep_trx_id, 'status':'executed'})})
                ], rep_block_num)
    
            print("Wait execution of scheduled transaction")
            self.assertEqual(rep_params['def_trx_id'], params['def_trx_id'])
            self.assertNotEqual(rep_params['def_packed_trx'], params['def_packed_trx'])

            if status == 'executed':
                self.waitEvents(
                    [ #({'msg_type':'AcceptTrx', 'id':rep_params['def_trx_id']},        {'accepted':True, 'implicit':False, 'scheduled':True}),
                      ({'msg_type':'ApplyTrx', 'id':rep_params['def_trx_id']},         {'block_num':exec_block_num, 'actions':AllItems(actionTrace(action, rep_arg))}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num},         {'trxs':Unorder({'id':rep_params['def_trx_id'], 'status':'executed'})})
                    ], exec_block_num)
            elif status == 'soft_fail':
                onerror_id = {'receiver':'test', 'code':'cyber', 'action':'onerror', 'args':{'sender_id':sender_id}}
                onerror_action = {
                    'receiver':'test', 'code':'cyber', 'action':'onerror', 
                    'auth':AllItems({'actor':'test', 'permission':'active'}), 
                    'args':{'sender_id':sender_id, 'sent_trx':rep_params['def_packed_trx']}, 
                    'events':Exactly([])}
                self.waitEvents(
                    [ #({'msg_type':'AcceptTrx', 'id':rep_params['def_trx_id']},{'accepted':True, 'implicit':False, 'scheduled':True}),
                      ({'msg_type':'ApplyTrx', 'actions':[onerror_id]},        {'id':Save(params,'onerror_trx_id'), 'actions':AllItems(onerror_action)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':rep_params['def_trx_id'], 'status':status})})
                    ], exec_block_num)



if __name__ == '__main__':
    unittest.main()
