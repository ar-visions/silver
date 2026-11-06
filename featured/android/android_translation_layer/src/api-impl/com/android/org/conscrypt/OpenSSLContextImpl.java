package com.android.org.conscrypt;

import java.security.KeyManagementException;
import java.security.SecureRandom;
import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLContextSpi;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLServerSocketFactory;
import javax.net.ssl.SSLSessionContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

// ready with no init: SSLContext.getDefault() takes it so
public class OpenSSLContextImpl extends SSLContextSpi {
	private X509TrustManager trust = TrustManagerImpl.shared();

	@Override
	protected void engineInit(KeyManager[] km, TrustManager[] tm, SecureRandom sr) throws KeyManagementException {
		trust = TrustManagerImpl.shared();
		if (tm != null)
			for (TrustManager t : tm)
				if (t instanceof X509TrustManager) {
					trust = (X509TrustManager)t;
					break;
				}
	}

	@Override
	protected SSLSocketFactory engineGetSocketFactory() {
		return new OpenSSLSocketFactoryImpl(trust);
	}

	@Override
	protected SSLServerSocketFactory engineGetServerSocketFactory() {
		throw new UnsupportedOperationException("no TLS servers");
	}

	@Override
	protected SSLEngine engineCreateSSLEngine() {
		throw new UnsupportedOperationException("no SSLEngine: sockets only");
	}

	@Override
	protected SSLEngine engineCreateSSLEngine(String host, int port) {
		throw new UnsupportedOperationException("no SSLEngine: sockets only");
	}

	@Override
	protected SSLSessionContext engineGetServerSessionContext() {
		return null;
	}

	@Override
	protected SSLSessionContext engineGetClientSessionContext() {
		return null;
	}
}
