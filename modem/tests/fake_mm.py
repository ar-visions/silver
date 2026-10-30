#!/usr/bin/python3
# a stand-in ModemManager on a private bus: the client's tests
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


class Modem(Obj):
    def __init__(self):
        super().__init__(MODEM, {
            MM + '.Modem': {
                'State': dbus.Int32(8),
                'SignalQuality': dbus.Struct((dbus.UInt32(62),
                                              dbus.Boolean(True))),
                'OwnNumbers': dbus.Array(['+15551230000'], 's'),
                'Sim': dbus.ObjectPath(SIM),
                'UnlockRequired': dbus.UInt32(1)},
            MM + '.Modem.Modem3gpp': {
                'OperatorName': dbus.String('Orbiter Mobile'),
                'RegistrationState': dbus.UInt32(1)},
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


def main():
    global bus
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.bus.BusConnection(sys.argv[1])
    name = dbus.service.BusName(MM, bus)
    m = Modem()
    Root(m)
    Fake(m)
    GLib.MainLoop().run()


main()
