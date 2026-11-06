package com.android.org.conscrypt;

import java.security.Provider;

// TLS by mbedtls, under Android's platform provider names
public final class OpenSSLProvider extends Provider {
	public OpenSSLProvider() {
		super("AndroidOpenSSL", 1.0, "TLS on mbedtls");
		String ctx = OpenSSLContextImpl.class.getName();
		put("SSLContext.TLS", ctx);
		put("SSLContext.SSL", ctx);
		put("SSLContext.TLSv1.2", ctx);
		put("SSLContext.TLSv1.3", ctx);
		put("SSLContext.Default", ctx);
		put("Alg.Alias.SSLContext.DEFAULT", "Default");
		String tmf = TrustManagerFactoryImpl.class.getName();
		put("TrustManagerFactory.PKIX", tmf);
		put("Alg.Alias.TrustManagerFactory.X509", "PKIX");
		put("Alg.Alias.TrustManagerFactory.X.509", "PKIX");
		put("Alg.Alias.TrustManagerFactory.SunX509", "PKIX");
		String kmf = KeyManagerFactoryImpl.class.getName();
		put("KeyManagerFactory.PKIX", kmf);
		put("Alg.Alias.KeyManagerFactory.X509", "PKIX");
		put("Alg.Alias.KeyManagerFactory.SunX509", "PKIX");
	}
}
