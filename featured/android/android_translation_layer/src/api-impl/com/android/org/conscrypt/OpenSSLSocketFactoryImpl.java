package com.android.org.conscrypt;

import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.X509TrustManager;

public class OpenSSLSocketFactoryImpl extends SSLSocketFactory {
	final X509TrustManager trust;

	public OpenSSLSocketFactoryImpl() {
		this(TrustManagerImpl.shared());
	}

	OpenSSLSocketFactoryImpl(X509TrustManager trust) {
		this.trust = trust;
	}

	@Override
	public String[] getDefaultCipherSuites() {
		return TlsNative.cipherSuites();
	}

	@Override
	public String[] getSupportedCipherSuites() {
		return TlsNative.cipherSuites();
	}

	@Override
	public Socket createSocket() throws IOException {
		return new OpenSSLSocketImpl(new Socket(), null, -1, true, trust);
	}

	@Override
	public Socket createSocket(Socket s, String host, int port, boolean autoClose) throws IOException {
		return new OpenSSLSocketImpl(s, host, port, autoClose, trust);
	}

	@Override
	public Socket createSocket(String host, int port) throws IOException {
		return new OpenSSLSocketImpl(new Socket(host, port), host, port, true, trust);
	}

	@Override
	public Socket createSocket(String host, int port, InetAddress local, int localPort) throws IOException {
		return new OpenSSLSocketImpl(new Socket(host, port, local, localPort), host, port, true, trust);
	}

	@Override
	public Socket createSocket(InetAddress addr, int port) throws IOException {
		return new OpenSSLSocketImpl(new Socket(addr, port), addr.getHostName(), port, true, trust);
	}

	@Override
	public Socket createSocket(InetAddress addr, int port, InetAddress local, int localPort) throws IOException {
		return new OpenSSLSocketImpl(new Socket(addr, port, local, localPort), addr.getHostName(), port, true, trust);
	}
}
