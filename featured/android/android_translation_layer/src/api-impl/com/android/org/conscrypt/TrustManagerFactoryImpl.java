package com.android.org.conscrypt;

import java.security.KeyStore;
import javax.net.ssl.ManagerFactoryParameters;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactorySpi;

public class TrustManagerFactoryImpl extends TrustManagerFactorySpi {
	@Override
	protected void engineInit(KeyStore ks) {
	}

	@Override
	protected void engineInit(ManagerFactoryParameters spec) {
	}

	@Override
	protected TrustManager[] engineGetTrustManagers() {
		return new TrustManager[] {TrustManagerImpl.shared()};
	}
}
