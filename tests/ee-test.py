#!/usr/bin/python3

# Tests for Event Engine.
# Preconditions:
# - nodeos run with enabled event_engine_plugin
# - event_engine_plugin write events in `events.dump` which located (or symlinked) in current directory
# - `tech.test` account created and contains `ee.test` contract
# - `tech.test@active` should contain `test@cyber.code` authority
# - wallet unlocked (with keys for `tech.test` account)

import subprocess
import unittest
import time
import json

testAccount = 'tech.test'

def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def cleos(arguments):
    (exception, traceback) = (None, None)
    try:
        return subprocess.check_output("programs/cleos/cleos " + arguments, stderr=subprocess.STDOUT, shell=True, universal_newlines=True)
    except subprocess.CalledProcessError as e:
        import sys
        (exception, traceback) = (e, sys.exc_info()[2])
    
    msg = str(exception) + ' with output:\n' + exception.output
    raise Exception(msg).with_traceback(traceback)

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

class Load:
    def __init__(self, params, name):
        self.params = params
        self.name = name

    def __repr__(self):
        return 'Load "%s" (%s)' % (self.name, self.params.get(self.name))

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

class Missing:
    def __init__(self, *args):
        self.values = args

    def __repr__(self):
        return str(self.values)



def SendDeferredTrace(action, arg, sender_id, delay, replace, params):
    def_action_in_event = {
        'account':testAccount, 'name':action, 
        'authorization':AllItems({'actor':testAccount, 'permission':'active'}), 
        'data':{'arg':arg}
    }
    return {
        "receiver":testAccount, "code":testAccount, "action":"senddeferred",
        "auth":[{"actor":testAccount,"permission":"active"}],
        "args":{"action":action, "arg":arg, "senderid":sender_id, "delay":delay, "replace":replace},
        "events":AllItems(
            {   'code':'', 'event':'senddeferred',
                'args':{
                    "sender":testAccount, 'sender_id':sender_id,
                    'trx':{'delay_sec':delay, 'context_free_actions':Exactly([]), 'actions':AllItems(def_action_in_event)},
                    "trx_id":Save(params, 'def_trx_id'),
                    'packed_trx':Save(params, 'def_packed_trx')
                },
            }
        )
    }
def CancelDeferTrace(sender_id):
    return {
        "receiver":testAccount, "code":testAccount, "action":"canceldefer",
        "auth":[{"actor":testAccount,"permission":"active"}],
        "args":{"senderid":sender_id},
        "events":AllItems(
            {"code":"", "event":"canceldefer", "args":{"sender":testAccount, 'sender_id':sender_id}}
        )
    }
def ActionTrace(action, arg):
    return {
        'receiver':testAccount, 'code':testAccount, 'action':action, 
        'auth':[{'actor':testAccount, 'permission':'active'}], 
        'args':{'arg':arg}, 'events':[]
    }

def ActionData(action, args):
    return {
        'account':testAccount, 'name':action, 
        'authorization':[{'actor':testAccount, 'permission':'active'}], 
        'data':args
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
                if msg['msg_type'] == 'AcceptBlock' and msg['block_num'] > maxBlockNum:
                    self.fail('Missed events at block %d: %s\n\nReaden lines:\n%s' % (maxBlockNum, events[i:], ''.join(lines)))
                if self.checkContains(selector, msg):
                    self._assertContains('events[%d]'%i, predicat, msg)
                    print('Found message for %d event: %s' % (i, selector))
                    i += 1
                    lines = []
                    break
            time.sleep(1)

    def _assertContains(self, path, templ, value, msg=None):
        if templ is None:
            self.assertIs(value, None, '%s' % path)
        elif type(templ) is Exactly:
            self.assertEqual(value, templ.value, '%s' % path)
        elif type(templ) is Save:
            templ.params[templ.name] = value
        elif type(templ) is Load:
            self._assertContains(path, templ.params[templ.name], value, msg=msg)
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
        elif type(templ) is Missing:
            self.assertEqual(type(value), type([]), path)
            i = 0
            for t in templ.values:
                npath='%s[%d]' % (path, i)
                for v in value:
                    if self.checkContains(t, v):
                        self.fail("%s contains %s : %s" % (value, t, npath))
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
        self.assertContainsSuccess({"array":Missing(5)}, obj)
        self.assertContainsFail({"array":Missing(5,4)}, obj)
        self.assertContainsSuccess({"a":Missing({"f":5})}, {"a":[None, 10, "Self", {"f":4}]})
        self.assertContainsFail({"a":Missing({"f":5})}, {"a":[None, 10, "Self", {"f":4}, {"f":5}]})


        self.assertContainsSuccess(
            {'block_num': 7417, 'trxs': Unorder({'id': '8aa6e455d4c02b5f570ad638581771732ffec2623ee6532c597a843b659a8d7c', 'status': 'executed'}), 'msg_type': 'AcceptBlock'},
            {'validated': True, 'events': [], 'block_time': '2019-06-28T02:02:33.000', 'in_current_chain': True, 'trxs': [{'storage_kbytes': 0, 'cpu_usage_us': 403, 'ram_kbytes': 1, 'id': '8aa6e455d4c02b5f570ad638581771732ffec2623ee6532c597a843b659a8d7c', 'net_usage_words': 13, 'status': 'executed'}], 'previous': '00001cf8c5644fd99349d3a9c838c5764b43ab19f08d5963ef2a614bea369af6', 'id': '00001cf9f562c8892281558313a909c54ec4f411b152dfeab4886cb6b4c15ac2', 'block_num': 7417, 'msg_type': 'AcceptBlock'})

        params = {}
        self.assertContainsSuccess(
            {'id': Save(params, "onerror_trx_id"), 'msg_type': 'ApplyTrx', 'actions': [{'args': {'sent_trx': '8c7e195d09036077a32400000000050001000000000090b1ca000000000088544301000000000090b1ca00000000a8ed323208000000000000000000', 'sender_id': 12340}, 'data': '', 'receiver': testAccount, 'events': [], 'code': 'cyber', 'auth': [{'actor': testAccount, 'permission': 'active'}], 'action': 'onerror'}]},
            {'id': 'c31ef3cb7bd820bc8bc90f21eb5ef01af08763839f982048e3452ce5be8e61ba', 'block_time': '2019-07-01T03:31:30.000', 'msg_type': 'ApplyTrx', 'actions': [{'args': {'sent_trx': '8c7e195d09036077a32400000000050001000000000090b1ca000000000088544301000000000090b1ca00000000a8ed323208000000000000000000', 'sender_id': 12340}, 'data': '', 'receiver': testAccount, 'events': [], 'code': 'cyber', 'auth': [{'actor': testAccount, 'permission': 'active'}], 'action': 'onerror'}], 'block_num': 780})

        

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
        result = pushAction(testAccount, action, testAccount, [arg])
        print("Pushed transaction with %s" % result['transaction_id'])
        self.assertEqual(result['processed']['receipt']['status'], 'executed')

        trx_id = result['transaction_id']
        block_num = result['processed']['block_num']

        actionTrx = {'actions':AllItems(ActionData(action,{'arg':arg}))}
        actionTrace = ActionTrace(action,arg)
        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':trx_id},                          {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':actionTrx}),
              ({'msg_type':'ApplyTrx', 'id':trx_id},                           {'block_num':block_num, 'actions':AllItems(actionTrace)}),
              ({'msg_type':'AcceptBlock', 'block_num':block_num},              {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
            ], block_num)

    def test_userSendFailTransaction(self):
        cases = (('Failure executed', 'check', 0, 'eosio_assert_message assertion failure'),
                 ('Long executed', 'dummy', 5, 'Transaction subjectively exceeded the current usage limit imposed on the transaction'),
                )
        for (name, action, arg, regex) in cases:
          with self.subTest(name = name):
            info = json.loads(cleos('get info'))
            with self.assertRaisesRegex(Exception, regex):
               result = pushAction(testAccount, action, testAccount, [arg])
    
            # It's need to add check that:
            #    a) events contains AcceptTrx/ApplyTrx about our transaction
            #    b) block doesn't contains trx
            params = {}
            actionTrx = {'actions':AllItems(ActionData(action,{'arg':arg}))}
            actionTrace = ActionTrace(action,arg)
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'accepted':False},                 {'id':Save(params,'trx_id'), 'trx':actionTrx}),
                  ({'msg_type':'ApplyTrx', 'id':Load(params,'trx_id')},        {'block_num':Save(params,'block_num'), 'actions':AllItems(actionTrace)}),
                  ({'msg_type':'AcceptBlock', 'block_num':Load(params,'block_num')}, {'trxs':Missing({'id':Load(params,'trx_id')})}),
                ], info['head_block_num'] + 5)
            self.assertLess(params['block_num'], info['head_block_num']+5)

    def test_userScheduleTransaction(self):
        gproperty = json.loads(cleos('get table "" "" gproperty'))['rows'][0]
        def_trx_expiration = gproperty['configuration']['deferred_trx_expiration_window']
        self.assertLessEqual(def_trx_expiration, 300, 'Too much deferred transaction expiration window')

        cases = (('Failure execucted', 'check', 0, 3, 'hard_fail'),
                 ('Success executed',  'check', 1, 3, 'executed'),
                 ('Expired',           'dummy', 1, 3, 'expired'),
                )
        for (name, action, arg, delay, status) in cases:
          with self.subTest(name=name):
            result = pushAction(testAccount, action, testAccount, [arg], delay=delay)
            print("Pushed transaction with id %s" % result['transaction_id'])
            self.assertEqual(result['processed']['receipt']['status'], 'delayed')
    
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            exec_block_num = block_num + (delay+2)//3
            exp_block_num = exec_block_num + def_trx_expiration//3 + 15
            print("Wait for %d" % exp_block_num)

            actionTrx = {'actions':AllItems(ActionData(action,{'arg':arg}))}
            actionTrace = ActionTrace(action, arg)

            if status == 'executed' or status == 'hard_fail':
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':block_num, 'actions':Exactly([])}),
                      ({'msg_type':'AcceptBlock', 'block_num':block_num},      {'trxs':Unorder({'id':trx_id, 'status':'delayed'})}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':exec_block_num, 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'trxs':[{'id':trx_id}]},     {'trxs':Unorder({'id':trx_id, 'status':status})}),
                    ], exp_block_num)

            elif status == 'expired':
                params = {}
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':block_num, 'actions':Exactly([])}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':Save(params,'try1'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':Load(params,'try1')}, {'trxs':Missing({'id':trx_id})}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':Save(params,'try2'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':Load(params,'try2')}, {'trxs':Missing({'id':trx_id})}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':Save(params,'try3'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':Load(params,'try3')}, {'trxs':Missing({'id':trx_id})}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':Save(params,'try4'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':Load(params,'try4')}, {'trxs':Missing({'id':trx_id})}),

                      ({'msg_type':'AcceptTrx', 'id':trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':trx_id},                   {'block_num':Save(params,'trx_block_num'), 'actions':Exactly([])}),
                      ({'msg_type':'AcceptBlock', 'block_num':Load(params,'trx_block_num')}, {'trxs':Unorder({'id':trx_id, 'status': 'expired'})}),
                    ], exp_block_num)
            else:
                self.fail('Unexpected status %s' % status)

    # There are several cases when different transactions fails:
    # - transaction takes too long (transaction placed in blacklist in current node and try reexec 4 times)
    # - transaction failed due not enough staked token (transaction placed in blacklist, reexec 4 times, doesn't check))
    # - transaction failed due assert (transaction placed in blacklist, no reexec)
    # - transaction takes too long time at the end of block (transaction rescheduled at next block, doesn't check)
    def test_contractScheduleTransaction(self):
        info = json.loads(cleos('get info'))
        gproperty = json.loads(cleos('get table "" "" gproperty'))['rows'][0]
        def_trx_expiration = gproperty['configuration']['deferred_trx_expiration_window']
        self.assertLessEqual(def_trx_expiration, 300, 'Too much deferred transaction expiration window')

        cases = (('Executed with assert',      'check', 0, 5, 60, 0, 'soft_fail'),
                 ('Success executed',          'check', 1, 5, 60, 1, 'executed'),
                 ('Transaction took too long', 'dummy', 1, 5, 60, 2, 'expired'),
                )
        for (name, action, arg, delay, expiration, sender_id, status) in cases:
          with self.subTest(name=name):
            sender_id += info['head_block_num']
            result = pushAction(testAccount, 'senddeferred', testAccount, [action, arg, sender_id, delay, False, expiration])
            print("Pushed transaction with %s" % result['transaction_id'])
            #print(json.dumps(result, indent=4))
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
    
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            exec_block_num = block_num + (delay+2)//3
    
            params = {}
            scheduleTrx = {'actions':AllItems(ActionData('senddeferred',{'action':action,'arg':arg,'senderid':sender_id,'delay':delay,'replace':False,'expiration':expiration}))}
            scheduleTrace = SendDeferredTrace(action, arg, sender_id, delay, False, params)

            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':scheduleTrx}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':AllItems(scheduleTrace)}),
                  ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
                ], block_num)
    
            def_trx_id = params['def_trx_id']
            def_packed_trx = params['def_packed_trx']
            print("Deferred transaction with %s" % def_trx_id)

            actionTrx = {'actions':AllItems(ActionData(action,{'arg':arg}))}
            actionTrace = ActionTrace(action, arg)
    
            if status == 'executed':
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':exec_block_num, 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':def_trx_id, 'status':status})})
                    ], exec_block_num)

            elif status == 'soft_fail':
                onerror_id = {'receiver':testAccount, 'code':'cyber', 'action':'onerror', 'args':{'sender_id':sender_id}}
                onerror_action = {
                    'receiver':testAccount, 'code':'cyber', 'action':'onerror', 
                    'auth':AllItems({'actor':testAccount, 'permission':'active'}), 
                    'args':{'sender_id':sender_id, 'sent_trx':def_packed_trx}, 
                    'events':Exactly([])}
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'actions':[onerror_id]},        {'id':Save(params,'onerror_trx_id'), 'actions':AllItems(onerror_action)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':def_trx_id, 'status':status})})
                    ], exec_block_num)

            elif status == 'expired':
                exp_block_num = block_num + (delay + def_trx_expiration + 2)//3
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':Save(params,'try1'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':Save(params,'try2'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':Save(params,'try3'), 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':Save(params,'try4'), 'actions':AllItems(actionTrace)}),
        
                      ({'msg_type':'AcceptTrx', 'id':def_trx_id},              {'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':def_trx_id},               {'block_num':Save(params,'trx_block_num'), 'actions':Exactly([])}),
                      ({'msg_type':'AcceptBlock', 'trxs':Unorder({'id':def_trx_id, 'status': 'expired'})}, {'block_num':Save(params,'exp_block_num')}),
                    ], exp_block_num + 15)
                self.assertEqual(params['trx_block_num'], params['exp_block_num'])
                self.assertGreater(params['exp_block_num'], exp_block_num)
            else:
                self.fail('Unexpected status %s' % status)

    def test_contractCancelTransaction(self):
        (action, arg, delay, sender_id, expiration) = ('check', 10, 20, 123411, 60)
        result = pushAction(testAccount, 'senddeferred', testAccount, [action, arg, sender_id, delay, False, expiration])
        #print(json.dumps(result, indent=4))
        self.assertEqual(result['processed']['receipt']['status'], 'executed')
        trx_id = result['transaction_id']
        block_num = result['processed']['block_num']
        print("Pushed transaction with %s in block %d" % (trx_id, block_num))

        params = {}
        scheduleTrx = {'actions':AllItems(ActionData('senddeferred',{'action':action,'arg':arg,'senderid':sender_id,'delay':delay,'replace':False,'expiration':expiration}))}
        scheduleTrace = SendDeferredTrace(action, arg, sender_id, delay, False, params)
        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':scheduleTrx}),
              ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':AllItems(scheduleTrace)}),
              ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
            ], block_num)

        result = pushAction(testAccount, 'canceldefer', testAccount, [sender_id])
        self.assertEqual(result['processed']['receipt']['status'], 'executed')
        cancel_trx_id = result['transaction_id']
        cancel_block_num = result['processed']['block_num']
        print("Pushed transaction with %s in block %d" % (cancel_trx_id, cancel_block_num))

        cancelTrx = {'actions':AllItems(ActionData('canceldefer',{'senderid':sender_id}))}
        cancelTrace = CancelDeferTrace(sender_id)
        self.waitEvents(
            [ ({'msg_type':'AcceptTrx', 'id':cancel_trx_id},                   {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':cancelTrx}),
              ({'msg_type':'ApplyTrx', 'id':cancel_trx_id},                    {'block_num':cancel_block_num, 'actions':AllItems(cancelTrace)}),
              ({'msg_type':'AcceptBlock', 'block_num':cancel_block_num},       {'trxs':Unorder({'id':cancel_trx_id, 'status':'executed'})})
            ], cancel_block_num)


    def test_contractReplaceTransaction(self):
        info = json.loads(cleos('get info'))
        cases = (('Failure execucted', 10, 0, 60, 6, 60, 0, 'soft_fail'),
                 ('Success executed',  11, 5, 60, 6, 60, 1, 'executed'))
        for (name, arg, rep_arg, delay, rep_delay, expiration, sender_id, status) in cases:
          with self.subTest(name=name):
            action = 'check'
            sender_id += info['head_block_num']
            print("Send original scheduled transaction")
            result = pushAction(testAccount, 'senddeferred', testAccount, [action, arg, sender_id, delay, False, expiration])
            #print(json.dumps(result, indent=4))
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
            trx_id = result['transaction_id']
            block_num = result['processed']['block_num']
            print("Pushed transaction with %s in block %d" % (trx_id, block_num))
    
            params = {}
            rep_params = {}
            scheduleTrx = {'actions':AllItems(ActionData('senddeferred',{'action':action,'arg':arg,'senderid':sender_id,'delay':delay,'replace':False,'expiration':expiration}))}
            scheduleTrace = SendDeferredTrace(action, arg, sender_id, delay, False, params)
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':trx_id},                      {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':scheduleTrx}),
                  ({'msg_type':'ApplyTrx', 'id':trx_id},                       {'block_num':block_num, 'actions':[scheduleTrace]}),
                  ({'msg_type':'AcceptBlock', 'block_num':block_num},          {'trxs':Unorder({'id':trx_id, 'status':'executed'})})
                ], block_num)

            print("Deferred transaction has %s id" % params['def_trx_id'])
    
            print("Replace scheduled transaction")
            result = pushAction(testAccount, 'senddeferred', testAccount, [action, rep_arg, sender_id, rep_delay, True, expiration])
            self.assertEqual(result['processed']['receipt']['status'], 'executed')
            rep_trx_id = result['transaction_id']
            rep_block_num = result['processed']['block_num']
            exec_block_num = rep_block_num + (rep_delay+2)//3
            print("Pushed transaction with %s in block %d" % (trx_id, block_num))
    
            rescheduleTrx = {'actions':AllItems(ActionData('senddeferred',{'action':action,'arg':rep_arg,'senderid':sender_id,'delay':rep_delay,'replace':True,'expiration':expiration}))}
            rescheduleTrace = SendDeferredTrace(action, rep_arg, sender_id, rep_delay, True, rep_params)
            self.waitEvents(
                [ ({'msg_type':'AcceptTrx', 'id':rep_trx_id},                  {'accepted':True, 'implicit':False, 'scheduled':False, 'trx':rescheduleTrx}),
                  ({'msg_type':'ApplyTrx', 'id':rep_trx_id},                   {'block_num':rep_block_num, 'actions':AllItems(rescheduleTrace)}),
                  ({'msg_type':'AcceptBlock', 'block_num':rep_block_num},      {'trxs':Unorder({'id':rep_trx_id, 'status':'executed'})})
                ], rep_block_num)
    
            print("Replace deferred transaction has %s id" % rep_params['def_trx_id'])
    
            print("Wait execution of scheduled transaction")
            self.assertNotEqual(rep_params['def_trx_id'], params['def_trx_id'])
            self.assertNotEqual(rep_params['def_packed_trx'], params['def_packed_trx'])

            actionTrx = {'actions':AllItems(ActionData(action,{'arg':rep_arg}))}
            actionTrace = ActionTrace(action, rep_arg)

            if status == 'executed':
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':rep_params['def_trx_id']},{'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'id':rep_params['def_trx_id']}, {'block_num':exec_block_num, 'actions':AllItems(actionTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':rep_params['def_trx_id'], 'status':'executed'})})
                    ], exec_block_num)

            elif status == 'soft_fail':
                onerror_id = {'receiver':testAccount, 'code':'cyber', 'action':'onerror', 'args':{'sender_id':sender_id}}
                onerrorTrace = {
                    'receiver':testAccount, 'code':'cyber', 'action':'onerror', 
                    'auth':AllItems({'actor':testAccount, 'permission':'active'}), 
                    'args':{'sender_id':sender_id, 'sent_trx':rep_params['def_packed_trx']}, 
                    'events':Exactly([])}
                self.waitEvents(
                    [ ({'msg_type':'AcceptTrx', 'id':rep_params['def_trx_id']},{'accepted':True, 'implicit':False, 'scheduled':True, 'trx':actionTrx}),
                      ({'msg_type':'ApplyTrx', 'actions':[onerror_id]},        {'id':Save(params,'onerror_trx_id'), 'actions':AllItems(onerrorTrace)}),
                      ({'msg_type':'AcceptBlock', 'block_num':exec_block_num}, {'trxs':Unorder({'id':rep_params['def_trx_id'], 'status':status})})
                    ], exec_block_num)



if __name__ == '__main__':
    unittest.main()
