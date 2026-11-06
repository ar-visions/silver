package com.android.org.conscrypt;

import java.net.Socket;
import java.security.KeyStore;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import javax.net.ssl.KeyManager;
import javax.net.ssl.KeyManagerFactorySpi;
import javax.net.ssl.ManagerFactoryParameters;
import javax.net.ssl.X509ExtendedKeyManager;

// no client certificates are offered: an X509 key manager with none
public class KeyManagerFactoryImpl extends KeyManagerFactorySpi {
	@Override
	protected void engineInit(KeyStore ks, char[] password) {
	}

	@Override
	protected void engineInit(ManagerFactoryParameters spec) {
	}

	@Override
	protected KeyManager[] engineGetKeyManagers() {
		return new KeyManager[] { new Empty() };
	}

	static class Empty extends X509ExtendedKeyManager {
		public String[] getClientAliases(String keyType, Principal[] issuers) { return null; }
		public String chooseClientAlias(String[] keyType, Principal[] issuers, Socket socket) { return null; }
		public String[] getServerAliases(String keyType, Principal[] issuers) { return null; }
		public String chooseServerAlias(String keyType, Principal[] issuers, Socket socket) { return null; }
		public X509Certificate[] getCertificateChain(String alias) { return null; }
		public PrivateKey getPrivateKey(String alias) { return null; }
	}
}
