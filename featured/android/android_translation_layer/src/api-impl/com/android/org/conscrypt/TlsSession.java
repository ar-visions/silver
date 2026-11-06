package com.android.org.conscrypt;

import java.security.Principal;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.util.HashMap;
import javax.net.ssl.SSLPeerUnverifiedException;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSessionBindingEvent;
import javax.net.ssl.SSLSessionBindingListener;
import javax.net.ssl.SSLSessionContext;

final class TlsSession implements SSLSession {
	final String host;
	final int port;
	final String cipher;
	final String protocol;
	final X509Certificate[] peer;
	private final long created = System.currentTimeMillis();
	private final HashMap<String, Object> values = new HashMap<>();
	private boolean valid;

	TlsSession(String host, int port, String cipher, String protocol, X509Certificate[] peer, boolean valid) {
		this.host = host;
		this.port = port;
		this.cipher = cipher;
		this.protocol = protocol;
		this.peer = peer;
		this.valid = valid;
	}

	static TlsSession none(String host, int port) {
		return new TlsSession(host, port, "SSL_NULL_WITH_NULL_NULL", "NONE", null, false);
	}

	public byte[] getId() {
		return new byte[0];
	}

	public SSLSessionContext getSessionContext() {
		return null;
	}

	public long getCreationTime() {
		return created;
	}

	public long getLastAccessedTime() {
		return created;
	}

	public void invalidate() {
		valid = false;
	}

	public boolean isValid() {
		return valid;
	}

	public void putValue(String name, Object value) {
		values.put(name, value);
		if (value instanceof SSLSessionBindingListener)
			((SSLSessionBindingListener)value).valueBound(new SSLSessionBindingEvent(this, name));
	}

	public Object getValue(String name) {
		return values.get(name);
	}

	public void removeValue(String name) {
		Object v = values.remove(name);
		if (v instanceof SSLSessionBindingListener)
			((SSLSessionBindingListener)v).valueUnbound(new SSLSessionBindingEvent(this, name));
	}

	public String[] getValueNames() {
		return values.keySet().toArray(new String[0]);
	}

	public Certificate[] getPeerCertificates() throws SSLPeerUnverifiedException {
		if (peer == null || peer.length == 0)
			throw new SSLPeerUnverifiedException("no peer certificates");
		return peer.clone();
	}

	public Certificate[] getLocalCertificates() {
		return null;
	}

	public javax.security.cert.X509Certificate[] getPeerCertificateChain() throws SSLPeerUnverifiedException {
		throw new SSLPeerUnverifiedException("use getPeerCertificates");
	}

	public Principal getPeerPrincipal() throws SSLPeerUnverifiedException {
		if (peer == null || peer.length == 0)
			throw new SSLPeerUnverifiedException("no peer certificates");
		return peer[0].getSubjectX500Principal();
	}

	public Principal getLocalPrincipal() {
		return null;
	}

	public String getCipherSuite() {
		return cipher;
	}

	public String getProtocol() {
		return protocol;
	}

	public String getPeerHost() {
		return host;
	}

	public int getPeerPort() {
		return port;
	}

	public int getPacketBufferSize() {
		return 16709;
	}

	public int getApplicationBufferSize() {
		return 16384;
	}
}
