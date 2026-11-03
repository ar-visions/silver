#!/usr/bin/python3
# a stand-in ModemManager on a private bus: the client's tests
import os
import sys
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

MM = 'org.freedesktop.ModemManager1'
ROOT = '/org/freedesktop/ModemManager1'
MODEM = ROOT + '/Modem/0'
SIM = ROOT + '/SIM/0'
PROPS = 'org.freedesktop.DBus.Properties'

bus = None
# the desktop's simulated phone: every text and MMS sent is
# answered by its recipient (--reply); --watch <pid> <bus pid>:
# the bus and this go when that process does
reply = '--reply' in sys.argv
the_modem = [None]
calls = {}
texts = {}
count = [0]


def new_path(kind):
    count[0] += 1
    return ROOT + '/' + kind + '/' + str(count[0])


class Obj(dbus.service.Object):
    def __init__(self, path, props):
        super().__init__(bus, path)
        self.path = path
        self.p = props

    @dbus.service.method(PROPS, in_signature='s', out_signature='a{sv}')
    def GetAll(self, iface):
        return self.p.get(iface, {})

    @dbus.service.method(PROPS, in_signature='ss', out_signature='v')
    def Get(self, iface, key):
        return self.p.get(iface, {})[key]

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, iface, changed, invalid):
        pass

    def set(self, iface, key, value):
        self.p[iface][key] = value
        self.PropertiesChanged(iface, {key: value}, [])


class Call(Obj):
    def __init__(self, number, incoming):
        path = new_path('Call')
        super().__init__(path, {MM + '.Call': {
            'Number': dbus.String(number),
            'State': dbus.Int32(3 if incoming else 0),
            'Direction': dbus.Int32(1 if incoming else 2)}})
        calls[path] = self

    def state(self, s):
        self.set(MM + '.Call', 'State', dbus.Int32(s))

    @dbus.service.method(MM + '.Call')
    def Start(self):
        self.state(1)
        GLib.timeout_add(100, lambda: self.state(2) and False)
        GLib.timeout_add(300, lambda: self.state(4) and False)

    @dbus.service.method(MM + '.Call')
    def Accept(self):
        self.state(4)

    @dbus.service.method(MM + '.Call')
    def Hangup(self):
        self.state(7)

    @dbus.service.method(MM + '.Call', in_signature='s')
    def SendDtmf(self, digits):
        pass


class Sms(Obj):
    def __init__(self, number, text, incoming):
        path = new_path('SMS')
        super().__init__(path, {MM + '.Sms': {
            'Number': dbus.String(number),
            'Text': dbus.String(text),
            'Timestamp': dbus.String('2026-10-30T09:15:00+00' if incoming
                                     else ''),
            'PduType': dbus.UInt32(1 if incoming else 2),
            'State': dbus.UInt32(3 if incoming else 1)}})
        texts[path] = self

    @dbus.service.method(MM + '.Sms')
    def Send(self):
        self.set(MM + '.Sms', 'State', dbus.UInt32(5))
        if reply:
            p = self.p[MM + '.Sms']
            number, text = str(p['Number']), str(p['Text'])
            GLib.timeout_add(1500, lambda: answer(number, text))


# a text back from the one written to
def answer(number, text):
    Sms(number, 'Got it: ' + text, True)
    the_modem[0].lists()
    return False


# a data link as the phone's: an IPv4 and an IPv6 address
class Bearer(Obj):
    def __init__(self, apn):
        path = new_path('Bearer')
        d = dbus.Dictionary
        super().__init__(path, {MM + '.Bearer': {
            'Connected': dbus.Boolean(True),
            'Interface': dbus.String('qmapmux0.0'),
            'Properties': d({'apn': dbus.String(apn)}, 'sv'),
            'Ip4Config': d({'method': dbus.UInt32(2),
                            'address': dbus.String('192.0.0.2'),
                            'prefix': dbus.UInt32(29),
                            'dns1': dbus.String('10.177.0.34'),
                            'mtu': dbus.UInt32(1500)}, 'sv'),
            'Ip6Config': d({'method': dbus.UInt32(2),
                            'address': dbus.String('2607:fb90::5'),
                            'prefix': dbus.UInt32(64),
                            'dns1': dbus.String('fd00:976a::9')},
                           'sv')}})


class Modem(Obj):
    def __init__(self):
        super().__init__(MODEM, {
            MM + '.Modem': {
                'State': dbus.Int32(8),
                'SignalQuality': dbus.Struct((dbus.UInt32(62),
                                              dbus.Boolean(True))),
                'OwnNumbers': dbus.Array(['+15551230000'], 's'),
                'Sim': dbus.ObjectPath(SIM),
                'UnlockRequired': dbus.UInt32(1),
                'Bearers': dbus.Array([], 'o'),
                'Ports': dbus.Array([dbus.Struct((
                    dbus.String('rmnet_ipa0'), dbus.UInt32(2))),
                    dbus.Struct((dbus.String('qrtr0'),
                                 dbus.UInt32(6)))], '(su)')},
            MM + '.Modem.Modem3gpp': {
                'OperatorName': dbus.String('Mobile'),
                'RegistrationState': dbus.UInt32(1),
                'InitialEpsBearerSettings': dbus.Dictionary(
                    {'apn': dbus.String('fast.t-mobile.com')}, 'sv')},
            MM + '.Modem.Voice': {'Calls': dbus.Array([], 'o')},
            MM + '.Modem.Messaging': {'Messages': dbus.Array([], 'o')}})

    def lists(self):
        self.set(MM + '.Modem.Voice', 'Calls',
                 dbus.Array([dbus.ObjectPath(p) for p in calls], 'o'))
        self.set(MM + '.Modem.Messaging', 'Messages',
                 dbus.Array([dbus.ObjectPath(p) for p in texts], 'o'))

    @dbus.service.method(MM + '.Modem', in_signature='b')
    def Enable(self, on):
        pass

    @dbus.service.method(MM + '.Modem.Simple', in_signature='a{sv}',
                         out_signature='o')
    def Connect(self, props):
        b = Bearer(str(props['apn']))
        self.set(MM + '.Modem', 'Bearers',
                 dbus.Array([dbus.ObjectPath(b.path)], 'o'))
        return dbus.ObjectPath(b.path)

    @dbus.service.method(MM + '.Modem.Voice', in_signature='a{sv}',
                         out_signature='o')
    def CreateCall(self, props):
        c = Call(str(props['number']), False)
        self.lists()
        return dbus.ObjectPath(c.path)

    @dbus.service.method(MM + '.Modem.Voice', in_signature='o')
    def DeleteCall(self, path):
        c = calls.pop(str(path), None)
        if c:
            c.remove_from_connection()
        self.lists()

    @dbus.service.method(MM + '.Modem.Messaging', in_signature='a{sv}',
                         out_signature='o')
    def Create(self, props):
        s = Sms(str(props['number']), str(props['text']), False)
        self.lists()
        return dbus.ObjectPath(s.path)

    @dbus.service.method(MM + '.Modem.Messaging', in_signature='o')
    def Delete(self, path):
        s = texts.pop(str(path), None)
        if s:
            s.remove_from_connection()
        self.lists()


class Root(dbus.service.Object):
    def __init__(self, modem):
        super().__init__(bus, ROOT)
        self.modem = modem

    @dbus.service.method('org.freedesktop.DBus.ObjectManager',
                         out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        return {dbus.ObjectPath(MODEM): self.modem.p}


# the tests' own calls: a call or a text coming in
class Fake(dbus.service.Object):
    def __init__(self, modem):
        super().__init__(bus, '/org/orbiter/Fake')
        self.modem = modem

    @dbus.service.method(MM, in_signature='s')
    def Ring(self, number):
        Call(number, True)
        self.modem.lists()

    @dbus.service.method(MM, in_signature='ss')
    def Text(self, number, text):
        Sms(number, text, True)
        self.modem.lists()


MMS = 'org.ofono.mms'
MMS_ROOT = '/org/ofono/mms'
SERVICE = MMS_ROOT + '/modemmanager'
mms_bus = None
mms = {}
settings = {}


class MmsMessage(dbus.service.Object):
    def __init__(self, props):
        count[0] += 1
        self.path = SERVICE + '/' + str(count[0])
        super().__init__(mms_bus, self.path)
        self.p = props
        mms[self.path] = self

    @dbus.service.method(MMS + '.Message')
    def Delete(self):
        mms.pop(self.path, None)
        self.remove_from_connection()
        service.MessageRemoved(dbus.ObjectPath(self.path))

    @dbus.service.signal(MMS + '.Message', signature='sv')
    def PropertyChanged(self, name, value):
        pass

    def status(self, s):
        self.p['Status'] = dbus.String(s)
        self.PropertyChanged('Status', dbus.String(s))


# one stored file holding every part, as mmsd-tng keeps them
def store(parts):
    count[0] += 1
    f = '/tmp/fake-mms-%d-%d' % (count[0], id(parts))
    rows = []
    with open(f, 'wb') as o:
        for i, (kind, data) in enumerate(parts):
            rows.append(dbus.Struct((dbus.String('part%d' % i),
                                     dbus.String(kind), dbus.String(f),
                                     dbus.UInt64(o.tell()),
                                     dbus.UInt64(len(data)))))
            o.write(data)
    return dbus.Array(rows, '(ssstt)')


def props_of(status, sender, to, parts):
    return {'Status': dbus.String(status),
            'Date': dbus.String('2026-10-30T10:00:00-0700'),
            'Sender': dbus.String(sender),
            'Modem Number': dbus.String('+15551230000'),
            'Recipients': dbus.Array(to, 's'),
            'Attachments': store(parts)}


class MmsService(dbus.service.Object):
    def __init__(self):
        super().__init__(mms_bus, SERVICE)

    @dbus.service.method(MMS + '.Service', out_signature='a(oa{sv})')
    def GetMessages(self):
        return [(dbus.ObjectPath(p), m.p) for p, m in mms.items()]

    @dbus.service.method(MMS + '.Service', in_signature='asa{sv}a(sss)',
                         out_signature='o')
    def SendMessage(self, to, options, files):
        parts = []
        for (pid, kind, name) in files:
            with open(str(name), 'rb') as f:
                parts.append((str(kind), f.read()))
        m = MmsMessage(props_of('draft', '', [str(t) for t in to],
                                parts))
        self.MessageAdded(dbus.ObjectPath(m.path), m.p)
        GLib.timeout_add(200, lambda: m.status('sent') and False)
        if reply and to:
            them = [str(t) for t in to]
            GLib.timeout_add(1500, lambda: mms_answer(them, parts))
        return dbus.ObjectPath(m.path)

    @dbus.service.signal(MMS + '.Service', signature='oa{sv}')
    def MessageAdded(self, path, props):
        pass

    @dbus.service.signal(MMS + '.Service', signature='o')
    def MessageRemoved(self, path):
        pass


class MmsRoot(dbus.service.Object):
    def __init__(self):
        super().__init__(mms_bus, MMS_ROOT)

    @dbus.service.method(MMS + '.Manager', out_signature='a(oa{sv})')
    def GetServices(self):
        return [(dbus.ObjectPath(SERVICE), {})]

    @dbus.service.method(MMS + '.ModemManager', in_signature='a{sv}')
    def ChangeAllSettings(self, s):
        settings.update({str(k): str(v) for k, v in s.items()})

    @dbus.service.method(MMS + '.ModemManager')
    def ProcessMessageQueue(self):
        pass

    @dbus.service.method(MMS + '.ModemManager', out_signature='a{sv}')
    def ViewSettings(self):
        return dbus.Dictionary(settings, 'sv')

    # the tests' own: a picture message coming in
    @dbus.service.method(MMS + '.Fake', in_signature='sasss')
    def Picture(self, sender, to, text, picture):
        png = (b'\x89PNG\r\n\x1a\n' + b'\0' * 24)
        if picture:
            with open(picture, 'rb') as f:
                png = f.read()
        m = MmsMessage(props_of('received', sender,
                                [str(t) for t in to],
                                [('text/plain;charset=utf-8',
                                  text.encode()),
                                 ('image/png', png)]))
        service.MessageAdded(dbus.ObjectPath(m.path), m.p)


service = None


# the same parts back from the first recipient, to the group
def mms_answer(them, parts):
    to = ['+15551230000'] + them[1:]
    m = MmsMessage(props_of('received', them[0], to, parts))
    service.MessageAdded(dbus.ObjectPath(m.path), m.p)
    return False


def watch(pid, bus_pid):
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        try:
            os.kill(bus_pid, 15)
        except OSError:
            pass
        loop.quit()
        return False


loop = None


def main():
    global bus
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.bus.BusConnection(sys.argv[1])
    name = dbus.service.BusName(MM, bus)
    m = Modem()
    the_modem[0] = m
    Obj(SIM, {MM + '.Sim': {'OperatorName': dbus.String('Google Fi')}})
    Root(m)
    Fake(m)
    global mms_bus, service
    mms_bus = dbus.bus.BusConnection(sys.argv[1])
    mms_name = dbus.service.BusName(MMS, mms_bus)
    service = MmsService()
    MmsRoot()
    global loop
    loop = GLib.MainLoop()
    if '--watch' in sys.argv:
        i = sys.argv.index('--watch')
        pid, bus_pid = int(sys.argv[i + 1]), int(sys.argv[i + 2])
        GLib.timeout_add_seconds(2, lambda: watch(pid, bus_pid))
    loop.run()


main()
