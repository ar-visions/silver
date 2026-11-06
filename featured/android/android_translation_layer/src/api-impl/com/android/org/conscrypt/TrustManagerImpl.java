package com.android.org.conscrypt;

import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import javax.net.ssl.X509TrustManager;

// the tls module's trust (the system CA bundle)
public final class TrustManagerImpl implements X509TrustManager {
	private static TrustManagerImpl shared;

	static synchronized TrustManagerImpl shared() {
		if (shared == null)
			shared = new TrustManagerImpl();
		return shared;
	}

	static byte[][] ders(X509Certificate[] chain) throws CertificateException {
		byte[][] out = new byte[chain.length][];
		for (int i = 0; i < chain.length; i++)
			out[i] = chain[i].getEncoded();
		return out;
	}

	static void check(X509Certificate[] chain, String host) throws CertificateException {
		if (chain == null || chain.length == 0)
			throw new CertificateException("no certificates");
		int flags = TlsNative.nVerifyChain(ders(chain), host);
		if (flags != 0)
			throw new CertificateException(flags < 0 ? "unreadable certificate" : TlsNative.nVerifyInfo(flags).trim());
	}

	@Override
	public void checkClientTrusted(X509Certificate[] chain, String authType) throws CertificateException {
		check(chain, null);
	}

	@Override
	public void checkServerTrusted(X509Certificate[] chain, String authType) throws CertificateException {
		check(chain, null);
	}

	// android.net.http.X509TrustManagerExtensions calls this
	public java.util.List<X509Certificate> checkServerTrusted(X509Certificate[] chain, String authType, String host) throws CertificateException {
		check(chain, host);
		ArrayList<X509Certificate> out = new ArrayList<>();
		for (X509Certificate c : chain)
			out.add(c);
		return out;
	}

	// the roots stay in the tls module
	@Override
	public X509Certificate[] getAcceptedIssuers() {
		return new X509Certificate[0];
	}
}
