#!/usr/bin/python3
# a stand-in BlueZ on a private bus: the client's tests run on it
import sys
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

BLUEZ = 'org.bluez'
PROPS = 'org.freedesktop.DBus.Properties'
OM = 'org.freedesktop.DBus.ObjectManager'
HCI = '/org/bluez/hci0'
CODE = 123456

# name, address, icon, paired, connected, rssi
DEVS = [
    ('Headphones', '11:22:33:44:55:01', 'audio-headset', False, False, -50),
    ('Car', '11:22:33:44:55:02', 'audio-card', True, True, -70),
    ('Keyboard', '11:22:33:44:55:03', 'input-keyboard', True, False, -60),
]


def dev_path(addr):
    return HCI + '/dev_' + addr.replace(':', '_')


class Device(dbus.service.Object):
    def __init__(self, bus, root, d):
        self.root = root
        self.name, self.addr, self.icon, self.paired, self.connected, \
            self.rssi = d
        self.path = dev_path(self.addr)
        super().__init__(bus, self.path)

    def props(self):
        return {'Alias': dbus.String(self.name),
                'Address': dbus.String(self.addr),
                'Icon': dbus.String(self.icon),
                'Paired': dbus.Boolean(self.paired),
                'Connected': dbus.Boolean(self.connected),
                'RSSI': dbus.Int16(self.rssi)}

    def changed(self):
        self.PropertiesChanged(BLUEZ + '.Device1', self.props(), [])

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, iface, changed, invalid):
        pass

    # asks the agent to compare the code; its answer decides
    @dbus.service.method(BLUEZ + '.Device1', async_callbacks=('ok', 'bad'))
    def Pair(self, ok, bad):
        agent = self.root.agent
        if not agent:
            bad(dbus.DBusException('no agent',
                name=BLUEZ + '.Error.AuthenticationFailed'))
            return
        proxy = self.root.bus.get_object(agent[0], agent[1])
        ask = dbus.Interface(proxy, BLUEZ + '.Agent1')

        def yes():
            self.paired = True
            self.changed()
            ok()

        def no(e):
            bad(dbus.DBusException('rejected',
                name=BLUEZ + '.Error.AuthenticationRejected'))
        ask.RequestConfirmation(dbus.ObjectPath(self.path),
                                dbus.UInt32(CODE), reply_handler=yes,
                                error_handler=no, timeout=30)

    @dbus.service.method(BLUEZ + '.Device1')
    def Connect(self):
        if not self.paired:
            raise dbus.DBusException('not paired', name=BLUEZ + '.Error.Failed')
        self.connected = True
        self.changed()

    @dbus.service.method(BLUEZ + '.Device1')
    def Disconnect(self):
        self.connected = False
        self.changed()


class Adapter(dbus.service.Object):
    def __init__(self, bus, root):
        super().__init__(bus, HCI)
        self.root = root
        self.powered = True
        self.discovering = False

    def props(self):
        return {'Powered': dbus.Boolean(self.powered),
                'Discovering': dbus.Boolean(self.discovering),
                'Address': dbus.String('AA:BB:CC:DD:EE:FF')}

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, iface, changed, invalid):
        pass

    @dbus.service.method(PROPS, in_signature='ssv')
    def Set(self, iface, name, value):
        if name == 'Powered':
            self.powered = bool(value)
            self.PropertiesChanged(BLUEZ + '.Adapter1', self.props(), [])

    @dbus.service.method(BLUEZ + '.Adapter1')
    def StartDiscovery(self):
        self.discovering = True
        self.PropertiesChanged(BLUEZ + '.Adapter1', self.props(), [])

    @dbus.service.method(BLUEZ + '.Adapter1')
    def StopDiscovery(self):
        self.discovering = False
        self.PropertiesChanged(BLUEZ + '.Adapter1', self.props(), [])

    @dbus.service.method(BLUEZ + '.Adapter1', in_signature='o')
    def RemoveDevice(self, path):
        for d in self.root.devs:
            if d.path == path:
                d.paired = False
                d.connected = False
                d.changed()


class Root(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, '/')
        self.bus = bus
        self.agent = None
        self.adapter = Adapter(bus, self)
        self.devs = [Device(bus, self, d) for d in DEVS]
        self.manager = Manager(bus, self)

    @dbus.service.method(OM, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        out = {dbus.ObjectPath(HCI): {BLUEZ + '.Adapter1':
                                      self.adapter.props()}}
        for d in self.devs:
            out[dbus.ObjectPath(d.path)] = {BLUEZ + '.Device1': d.props()}
        return out


class Manager(dbus.service.Object):
    def __init__(self, bus, root):
        super().__init__(bus, '/org/bluez')
        self.root = root

    @dbus.service.method(BLUEZ + '.AgentManager1', in_signature='os',
                         sender_keyword='sender')
    def RegisterAgent(self, path, cap, sender=None):
        self.root.agent = (sender, path)

    @dbus.service.method(BLUEZ + '.AgentManager1', in_signature='o')
    def RequestDefaultAgent(self, path):
        pass


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.bus.BusConnection(sys.argv[1])
    name = dbus.service.BusName(BLUEZ, bus)
    root = Root(bus)
    GLib.MainLoop().run()


main()
