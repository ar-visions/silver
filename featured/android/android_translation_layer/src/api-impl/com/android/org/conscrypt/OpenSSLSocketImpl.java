package com.android.org.conscrypt;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.SocketAddress;
import java.net.Socket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.nio.charset.StandardCharsets;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import javax.net.ssl.HandshakeCompletedEvent;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.SSLException;
import javax.net.ssl.SSLHandshakeException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.X509ExtendedTrustManager;
import javax.net.ssl.X509TrustManager;

/*
 * A TLS client socket over a plain one, the TLS done by our
 * tls module's TLSClient. Named as Android's platform socket so OkHttp drives
 * its hostname, tickets and ALPN by the methods it looks for.
 */
public class OpenSSLSocketImpl extends SSLSocket {
	private static final String[] PROTOCOLS = {"TLSv1.2", "TLSv1.3"};

	private final Socket raw;
	private String host;
	private final int port;
	private final boolean autoClose;
	private final X509TrustManager trust;
	private final Object hs = new Object();
	private final ArrayList<HandshakeCompletedListener> listeners = new ArrayList<>();
	private String[] protocols = PROTOCOLS.clone();
	private String[] suites;
	private byte[] alpnWire;
	private String alpnChosen;
	private int handle;
	private int users;
	private boolean handshaken;
	private boolean closed;
	private int writeTimeout;
	private TlsSession session;
	private InputStream in;
	private OutputStream out;

	OpenSSLSocketImpl(Socket raw, String host, int port, boolean autoClose, X509TrustManager trust) {
		this.raw = raw;
		this.host = host;
		this.port = port;
		this.autoClose = autoClose;
		this.trust = trust;
	}

	// the native context lives while a read or write uses it
	private synchronized int acquire() throws IOException {
		if (closed || handle == 0)
			throw new SocketException("Socket is closed");
		users++;
		return handle;
	}

	private synchronized void release() {
		users--;
		if (closed && users == 0 && handle != 0) {
			TlsNative.nClose(handle);
			handle = 0;
		}
	}

	// the wire form's names with commas between
	private String alpnList() {
		String[] names = getApplicationProtocols();
		return names.length == 0 ? null : String.join(",", names);
	}

	private static boolean has(String[] list, String s) {
		for (String x : list)
			if (x.equals(s))
				return true;
		return false;
	}

	private static String authType(String cipher, String protocol) {
		if ("TLSv1.3".equals(protocol))
			return "GENERIC";
		int a = cipher.indexOf('_');
		int b = cipher.indexOf("_WITH_");
		return (a >= 0 && b > a) ? cipher.substring(a + 1, b) : "UNKNOWN";
	}

	@Override
	public void startHandshake() throws IOException {
		synchronized (hs) {
			if (handshaken)
				return;
			if (closed)
				throw new SocketException("Socket is closed");
			if (!has(protocols, "TLSv1.2") && !has(protocols, "TLSv1.3"))
				throw new SSLHandshakeException("no TLS version enabled");
			int fd = TlsNative.nFd(raw);
			if (fd < 0)
				throw new SSLException("TLS needs a connected plain socket");
			boolean ours = trust == TrustManagerImpl.shared();
			int h = TlsNative.nWrap(fd, host, alpnList(), ours, raw.getSoTimeout());
			synchronized (this) {
				handle = h;
			}
			int r = TlsNative.nFailure(h);
			if (ours && r != 0 && TlsNative.nVerifyResult(h) != 0) {
				String why = TlsNative.nVerifyInfo(TlsNative.nVerifyResult(h)).trim();
				close();
				throw new SSLHandshakeException("server not trusted: " + why);
			}
			if (r == TlsNative.TIMED_OUT)
				throw new SocketTimeoutException("TLS handshake timed out");
			if (r != 0)
				throw new SSLHandshakeException(TlsNative.nErrorText(r));
			X509Certificate[] chain = chain(TlsNative.nPeerChain(h));
			String cipher = TlsNative.javaName(TlsNative.nCipher(h));
			String protocol = TlsNative.nVersion(h);
			alpnChosen = TlsNative.nAlpn(h);
			if (!ours)
				trusted(chain, cipher, protocol);
			session = new TlsSession(host, port, cipher, protocol, chain, true);
			handshaken = true;
		}
		HandshakeCompletedEvent ev = new HandshakeCompletedEvent(this, session);
		for (HandshakeCompletedListener l : new ArrayList<>(listeners))
			l.handshakeCompleted(ev);
	}

	private static X509Certificate[] chain(byte[][] ders) throws SSLException {
		try {
			CertificateFactory f = CertificateFactory.getInstance("X.509");
			X509Certificate[] out = new X509Certificate[ders.length];
			for (int i = 0; i < ders.length; i++)
				out[i] = (X509Certificate)f.generateCertificate(new ByteArrayInputStream(ders[i]));
			return out;
		} catch (CertificateException e) {
			throw new SSLException("peer certificate unreadable", e);
		}
	}

	// the app's own trust manager judges the chain
	private void trusted(X509Certificate[] chain, String cipher, String protocol) throws IOException {
		try {
			if (trust instanceof X509ExtendedTrustManager)
				((X509ExtendedTrustManager)trust).checkServerTrusted(chain, authType(cipher, protocol), this);
			else
				trust.checkServerTrusted(chain, authType(cipher, protocol));
		} catch (CertificateException e) {
			close();
			SSLHandshakeException x = new SSLHandshakeException("server not trusted: " + e.getMessage());
			x.initCause(e);
			throw x;
		}
	}

	@Override
	public SSLSession getSession() {
		try {
			startHandshake();
		} catch (IOException e) {
			return TlsSession.none(host, port);
		}
		return session;
	}

	@Override
	public SSLSession getHandshakeSession() {
		return session;
	}

	private final class TlsIn extends InputStream {
		@Override
		public int read() throws IOException {
			byte[] one = new byte[1];
			int n = read(one, 0, 1);
			return n <= 0 ? -1 : (one[0] & 0xff);
		}

		@Override
		public int read(byte[] b, int off, int len) throws IOException {
			if (len == 0)
				return 0;
			startHandshake();
			int h;
			try {
				h = acquire();
			} catch (SocketException e) {
				return -1;
			}
			try {
				int r = TlsNative.nRead(h, b, off, len, raw.getSoTimeout());
				if (r > 0)
					return r;
				if (r == 0 || r == TlsNative.CLOSE_NOTIFY)
					return -1;
				if (r == TlsNative.TIMED_OUT)
					throw new SocketTimeoutException("Read timed out");
				return isClosed() ? -1 : fail(r);
			} finally {
				release();
			}
		}

		@Override
		public int available() throws IOException {
			if (!handshaken)
				return 0;
			try {
				int h = acquire();
				try {
					return TlsNative.nPending(h);
				} finally {
					release();
				}
			} catch (SocketException e) {
				return 0;
			}
		}

		@Override
		public void close() throws IOException {
			OpenSSLSocketImpl.this.close();
		}
	}

	private final class TlsOut extends OutputStream {
		@Override
		public void write(int b) throws IOException {
			write(new byte[] {(byte)b}, 0, 1);
		}

		@Override
		public void write(byte[] b, int off, int len) throws IOException {
			if (len == 0)
				return;
			startHandshake();
			int h = acquire();
			try {
				int r = TlsNative.nWrite(h, b, off, len, writeTimeout);
				if (r == TlsNative.TIMED_OUT)
					throw new SocketTimeoutException("Write timed out");
				if (r != 0)
					fail(r);
			} finally {
				release();
			}
		}

		@Override
		public void close() throws IOException {
			OpenSSLSocketImpl.this.close();
		}
	}

	private static int fail(int code) throws IOException {
		throw new SSLException(TlsNative.nErrorText(code));
	}

	@Override
	public synchronized InputStream getInputStream() throws IOException {
		if (closed)
			throw new SocketException("Socket is closed");
		if (in == null)
			in = new TlsIn();
		return in;
	}

	@Override
	public synchronized OutputStream getOutputStream() throws IOException {
		if (closed)
			throw new SocketException("Socket is closed");
		if (out == null)
			out = new TlsOut();
		return out;
	}

	@Override
	public void close() throws IOException {
		int h;
		synchronized (this) {
			if (closed)
				return;
			closed = true;
			h = handle;
			users++;
		}
		try {
		} finally {
			release();
			if (autoClose)
				raw.close();
		}
	}

	// ---- versions, suites, roles -------------------------------------

	@Override
	public String[] getSupportedCipherSuites() {
		return TlsNative.cipherSuites();
	}

	@Override
	public String[] getEnabledCipherSuites() {
		return suites != null ? suites.clone() : TlsNative.cipherSuites();
	}

	@Override
	public void setEnabledCipherSuites(String[] list) {
		if (list == null)
			throw new IllegalArgumentException("suites == null");
		suites = list.clone();
	}

	@Override
	public String[] getSupportedProtocols() {
		return PROTOCOLS.clone();
	}

	@Override
	public String[] getEnabledProtocols() {
		return protocols.clone();
	}

	@Override
	public void setEnabledProtocols(String[] list) {
		if (list == null)
			throw new IllegalArgumentException("protocols == null");
		ArrayList<String> kept = new ArrayList<>();
		for (String p : list)
			if (has(PROTOCOLS, p))
				kept.add(p);
		protocols = kept.toArray(new String[0]);
	}

	@Override
	public void addHandshakeCompletedListener(HandshakeCompletedListener l) {
		listeners.add(l);
	}

	@Override
	public void removeHandshakeCompletedListener(HandshakeCompletedListener l) {
		listeners.remove(l);
	}

	@Override
	public void setUseClientMode(boolean mode) {
		if (!mode)
			throw new IllegalArgumentException("client mode only");
	}

	@Override
	public boolean getUseClientMode() {
		return true;
	}

	@Override
	public void setNeedClientAuth(boolean need) {
	}

	@Override
	public boolean getNeedClientAuth() {
		return false;
	}

	@Override
	public void setWantClientAuth(boolean want) {
	}

	@Override
	public boolean getWantClientAuth() {
		return false;
	}

	@Override
	public void setEnableSessionCreation(boolean flag) {
	}

	@Override
	public boolean getEnableSessionCreation() {
		return true;
	}

	// ---- what OkHttp and Conscrypt users call ------------------------

	public void setUseSessionTickets(boolean use) {
	}

	public void setHostname(String hostname) {
		host = hostname;
	}

	public String getHostname() {
		return host;
	}

	public void setSoWriteTimeout(int ms) {
		writeTimeout = ms;
	}

	public int getSoWriteTimeout() {
		return writeTimeout;
	}

	public void setHandshakeTimeout(int ms) {
	}

	public void setChannelIdEnabled(boolean enabled) {
	}

	public byte[] getChannelId() {
		return null;
	}

	public void setNpnProtocols(byte[] list) {
	}

	public byte[] getNpnSelectedProtocol() {
		return null;
	}

	// the wire form: each name after its length byte
	public void setAlpnProtocols(byte[] wire) {
		alpnWire = wire != null ? wire.clone() : null;
	}

	public void setAlpnProtocols(String[] names) {
		setApplicationProtocols(names);
	}

	public void setApplicationProtocols(String[] names) {
		if (names == null || names.length == 0) {
			alpnWire = null;
			return;
		}
		int n = 0;
		for (String s : names)
			n += 1 + s.getBytes(StandardCharsets.UTF_8).length;
		byte[] wire = new byte[n];
		int at = 0;
		for (String s : names) {
			byte[] b = s.getBytes(StandardCharsets.UTF_8);
			wire[at++] = (byte)b.length;
			System.arraycopy(b, 0, wire, at, b.length);
			at += b.length;
		}
		alpnWire = wire;
	}

	public String[] getApplicationProtocols() {
		ArrayList<String> out = new ArrayList<>();
		if (alpnWire != null)
			for (int i = 0; i < alpnWire.length; i += (alpnWire[i] & 0xff) + 1)
				out.add(new String(alpnWire, i + 1, alpnWire[i] & 0xff, StandardCharsets.UTF_8));
		return out.toArray(new String[0]);
	}

	public byte[] getAlpnSelectedProtocol() {
		return alpnChosen != null ? alpnChosen.getBytes(StandardCharsets.UTF_8) : null;
	}

	public String getApplicationProtocol() {
		return handshaken ? (alpnChosen != null ? alpnChosen : "") : null;
	}

	public String getHandshakeApplicationProtocol() {
		return getApplicationProtocol();
	}

	// ---- the plain socket's own state --------------------------------

	@Override
	public void connect(SocketAddress endpoint) throws IOException {
		raw.connect(endpoint);
	}

	@Override
	public void connect(SocketAddress endpoint, int timeout) throws IOException {
		raw.connect(endpoint, timeout);
	}

	@Override
	public void bind(SocketAddress local) throws IOException {
		raw.bind(local);
	}

	@Override
	public InetAddress getInetAddress() {
		return raw.getInetAddress();
	}

	@Override
	public InetAddress getLocalAddress() {
		return raw.getLocalAddress();
	}

	@Override
	public int getPort() {
		return raw.getPort();
	}

	@Override
	public int getLocalPort() {
		return raw.getLocalPort();
	}

	@Override
	public SocketAddress getRemoteSocketAddress() {
		return raw.getRemoteSocketAddress();
	}

	@Override
	public SocketAddress getLocalSocketAddress() {
		return raw.getLocalSocketAddress();
	}

	@Override
	public void setTcpNoDelay(boolean on) throws SocketException {
		raw.setTcpNoDelay(on);
	}

	@Override
	public boolean getTcpNoDelay() throws SocketException {
		return raw.getTcpNoDelay();
	}

	@Override
	public void setSoLinger(boolean on, int linger) throws SocketException {
		raw.setSoLinger(on, linger);
	}

	@Override
	public int getSoLinger() throws SocketException {
		return raw.getSoLinger();
	}

	@Override
	public synchronized void setSoTimeout(int ms) throws SocketException {
		raw.setSoTimeout(ms);
	}

	@Override
	public synchronized int getSoTimeout() throws SocketException {
		return raw.getSoTimeout();
	}

	@Override
	public synchronized void setSendBufferSize(int size) throws SocketException {
		raw.setSendBufferSize(size);
	}

	@Override
	public synchronized int getSendBufferSize() throws SocketException {
		return raw.getSendBufferSize();
	}

	@Override
	public synchronized void setReceiveBufferSize(int size) throws SocketException {
		raw.setReceiveBufferSize(size);
	}

	@Override
	public synchronized int getReceiveBufferSize() throws SocketException {
		return raw.getReceiveBufferSize();
	}

	@Override
	public void setKeepAlive(boolean on) throws SocketException {
		raw.setKeepAlive(on);
	}

	@Override
	public boolean getKeepAlive() throws SocketException {
		return raw.getKeepAlive();
	}

	@Override
	public boolean isConnected() {
		return raw.isConnected();
	}

	@Override
	public boolean isBound() {
		return raw.isBound();
	}

	@Override
	public synchronized boolean isClosed() {
		return closed || raw.isClosed();
	}

	@Override
	public boolean isInputShutdown() {
		return raw.isInputShutdown();
	}

	@Override
	public boolean isOutputShutdown() {
		return raw.isOutputShutdown();
	}

	@Override
	public void shutdownInput() throws IOException {
		throw new UnsupportedOperationException("not on a TLS socket");
	}

	@Override
	public void shutdownOutput() throws IOException {
		throw new UnsupportedOperationException("not on a TLS socket");
	}

	@Override
	public String toString() {
		return "TLS socket [" + host + ":" + port + " " + raw + "]";
	}

	@Override
	public javax.net.ssl.SSLParameters getSSLParameters() {
		javax.net.ssl.SSLParameters p = super.getSSLParameters();
		p.setApplicationProtocols(getApplicationProtocols());
		return p;
	}

	@Override
	public void setSSLParameters(javax.net.ssl.SSLParameters p) {
		super.setSSLParameters(p);
		String[] names = p.getApplicationProtocols();
		if (names != null && names.length > 0)
			setApplicationProtocols(names);
	}
}
