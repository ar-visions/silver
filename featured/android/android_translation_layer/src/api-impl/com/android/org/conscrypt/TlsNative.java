package com.android.org.conscrypt;

import java.net.Socket;

// our tls module (TLSClient) under Java's TLS sockets
final class TlsNative {
	// mbedtls's codes the socket reads
	static final int TIMED_OUT = -0x6800;
	static final int CLOSE_NOTIFY = -0x7880;

	static native int nWrap(int fd, String host, String alpn, boolean verify, int timeoutMs);
	static native int nFailure(int id);
	static native int nRead(int id, byte[] b, int off, int len, int timeoutMs);
	static native int nWrite(int id, byte[] b, int off, int len, int timeoutMs);
	static native int nPending(int id);
	static native void nClose(int id);
	static native int nVerifyResult(int id);
	static native String nVerifyInfo(int flags);
	static native String nErrorText(int code);
	static native byte[][] nPeerChain(int id);
	static native String nCipher(int id);
	static native String nVersion(int id);
	static native String nAlpn(int id);
	static native int nVerifyChain(byte[][] ders, String host);
	static native String[] nCipherNames();
	static native int nFd(Socket s);

	// mbedtls's names as Java's: TLS1-3-AES-128-GCM-SHA256 -> TLS_AES_128_GCM_SHA256
	static String javaName(String mbed) {
		if (mbed == null)
			return "SSL_NULL_WITH_NULL_NULL";
		if (mbed.startsWith("TLS1-3-"))
			return "TLS_" + mbed.substring(7).replace('-', '_');
		return mbed.replace('-', '_');
	}

	private static String[] ciphers;

	static synchronized String[] cipherSuites() {
		if (ciphers == null) {
			String[] names = nCipherNames();
			ciphers = new String[names.length];
			for (int i = 0; i < names.length; i++)
				ciphers[i] = javaName(names[i]);
		}
		return ciphers.clone();
	}
}
