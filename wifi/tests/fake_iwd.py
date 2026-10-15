#!/usr/bin/python3
# a stand-in iwd on a private bus: the client's tests run on it
import sys
import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib

IWD = 'net.connman.iwd'
PROPS = 'org.freedesktop.DBus.Properties'
DEV = '/net/connman/iwd/0/4'
PASS = 'secret123'

# name, kind, signal (dBm x 100), known
NETS = [
    ('Home', 'psk', -4500, True),
    ('Cafe', 'open', -7000, False),
    ('Neighbor', 'psk', -8500, False),
]


def net_path(i):
    return DEV + '/' + str(i) + '_psk'


class Station(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, DEV)
        self.state = 'disconnected'
        self.connected = None
        self.scans = 0

    def props(self):
        p = {'State': dbus.String(self.state),
             'Scanning': dbus.Boolean(False)}
        if self.connected:
            p['ConnectedNetwork'] = dbus.ObjectPath(self.connected)
        return p

    def changed(self, iface, props):
        self.PropertiesChanged(iface, props, [])

    @dbus.service.signal(PROPS, signature='sa{sv}as')
    def PropertiesChanged(self, iface, changed, invalid):
        pass

    @dbus.service.method(IWD + '.Station', out_signature='a(on)')
    def GetOrderedNetworks(self):
        out = sorted(enumerate(NETS), key=lambda n: -n[1][2])
        return [(dbus.ObjectPath(net_path(i)), dbus.Int16(n[2]))
                for i, n in out]

    @dbus.service.method(IWD + '.Station')
    def Scan(self):
        self.scans += 1
        self.changed(IWD + '.Station', {'Scanning': dbus.Boolean(True)})
        GLib.timeout_add(100, lambda: self.changed(
            IWD + '.Station', {'Scanning': dbus.Boolean(False)}) and False)

    @dbus.service.method(IWD + '.Station')
    def Disconnect(self):
        self.state = 'disconnected'
        self.connected = None
        self.changed(IWD + '.Station', {'State': 'disconnected'})


class Network(dbus.service.Object):
    def __init__(self, bus, i, station, manager):
        super().__init__(bus, net_path(i))
        self.i = i
        self.station = station
        self.manager = manager
        self.bus = bus

    def props(self):
        name, kind, _, known = NETS[self.i]
        p = {'Name': dbus.String(name), 'Type': dbus.String(kind),
             'Connected': dbus.Boolean(
                 self.station.connected == net_path(self.i)),
             'Device': dbus.ObjectPath(DEV)}
        if known:
            p['KnownNetwork'] = dbus.ObjectPath(
                '/net/connman/iwd/known/' + str(self.i))
        return p

    @dbus.service.method(IWD + '.Network', in_signature='',
                         out_signature='', async_callbacks=('ok', 'err'))
    def Connect(self, ok, err):
        name, kind, _, known = NETS[self.i]
        st = self.station

        def joined():
            st.state = 'connected'
            st.connected = net_path(self.i)
            NETS[self.i] = (name, kind, NETS[self.i][2], True)
            st.changed(IWD + '.Station', st.props())
            ok()
        st.state = 'connecting'
        st.changed(IWD + '.Station', {'State': 'connecting'})
        if kind == 'open' or known:
            GLib.timeout_add(50, lambda: joined() and False)
            return
        agent = self.manager.agent
        if not agent:
            err(dbus.DBusException('no agent', name=IWD + '.NoAgent'))
            return
        proxy = self.bus.get_object(agent[0], agent[1], introspect=False)

        def got(word):
            if word == PASS:
                joined()
            else:
                st.state = 'disconnected'
                st.changed(IWD + '.Station', {'State': 'disconnected'})
                err(dbus.DBusException('wrong passphrase',
                                       name=IWD + '.Failed'))

        def failed(e):
            st.state = 'disconnected'
            err(dbus.DBusException(str(e), name=IWD + '.Aborted'))
        proxy.RequestPassphrase(dbus.ObjectPath(net_path(self.i)),
                                dbus_interface=IWD + '.Agent',
                                reply_handler=got, error_handler=failed)


class Manager(dbus.service.Object):
    def __init__(self, bus):
        super().__init__(bus, '/net/connman/iwd')
        self.agent = None

    @dbus.service.method(IWD + '.AgentManager', in_signature='o',
                         sender_keyword='sender')
    def RegisterAgent(self, path, sender=None):
        self.agent = (sender, path)


class Root(dbus.service.Object):
    def __init__(self, bus, station, nets):
        super().__init__(bus, '/')
        self.station = station
        self.nets = nets

    @dbus.service.method('org.freedesktop.DBus.ObjectManager',
                         out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        out = {DEV: {
            IWD + '.Device': {'Name': dbus.String('wlan0'),
                              'Address': dbus.String('00:11:22:33:44:55'),
                              'Powered': dbus.Boolean(True),
                              'Mode': dbus.String('station')},
            IWD + '.Station': self.station.props()}}
        for n in self.nets:
            out[net_path(n.i)] = {IWD + '.Network': n.props()}
        return out


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.bus.BusConnection(sys.argv[1])
    name = dbus.service.BusName(IWD, bus)
    manager = Manager(bus)
    station = Station(bus)
    nets = [Network(bus, i, station, manager) for i in range(len(NETS))]
    Root(bus, station, nets)
    print('fake iwd ready', flush=True)
    GLib.MainLoop().run()


main()
