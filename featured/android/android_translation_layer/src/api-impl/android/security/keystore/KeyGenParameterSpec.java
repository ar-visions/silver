package android.security.keystore;

import java.math.BigInteger;
import java.util.Date;
import javax.security.auth.x500.X500Principal;

public class KeyGenParameterSpec implements java.security.spec.AlgorithmParameterSpec {

	private String keystoreAlias;
	private int purposes;
	private int keySize;
	private String[] blockModes;
	private String[] encryptionPaddings;
	private boolean userAuthenticationRequired;

	public static class Builder {
		private KeyGenParameterSpec spec = new KeyGenParameterSpec();

		public Builder(String keystoreAlias, int purposes) {
			spec.keystoreAlias = keystoreAlias;
			spec.purposes = purposes;
		}

		public Builder setKeySize(int keySize) {
			spec.keySize = keySize;
			return this;
		}

		public Builder setBlockModes(String[] blockModes) {
			spec.blockModes = blockModes;
			return this;
		}

		public Builder setEncryptionPaddings(String[] encryptionPaddings) {
			spec.encryptionPaddings = encryptionPaddings;
			return this;
		}

		public Builder setUserAuthenticationRequired(boolean userAuthenticationRequired) {
			spec.userAuthenticationRequired = userAuthenticationRequired;
			return this;
		}

		public Builder setCertificateSerialNumber(BigInteger serialNumber) {
			return this;
		}

		public Builder setCertificateSubject(X500Principal subject) {
			return this;
		}

		public Builder setCertificateNotBefore(Date notBefore) {
			return this;
		}

		public Builder setCertificateNotAfter(Date notAfter) {
			return this;
		}

		public Builder setDigests(String[] digests) {
			return this;
		}

		public Builder setSignaturePaddings(String[] signaturePaddings) {
			return this;
		}

		public KeyGenParameterSpec build() {
			return spec;
		}
	
	// GEN member:android.security.keystore.KeyGenParameterSpec$Builder:public android.security.keystore.KeyGenParameterSpec$Builder setAlgorithmParameterSpec(java.security.spec.AlgorithmParameterSpec);
	public android.security.keystore.KeyGenParameterSpec.Builder setAlgorithmParameterSpec(java.security.spec.AlgorithmParameterSpec arg0) { return this; }

	// GEN member:android.security.keystore.KeyGenParameterSpec$Builder:public android.security.keystore.KeyGenParameterSpec$Builder setAttestationChallenge(byte[]);
	public android.security.keystore.KeyGenParameterSpec.Builder setAttestationChallenge(byte[] arg0) { return this; }

	// GEN member:android.security.keystore.KeyGenParameterSpec$Builder:public android.security.keystore.KeyGenParameterSpec$Builder setIsStrongBoxBacked(boolean);
	public android.security.keystore.KeyGenParameterSpec.Builder setIsStrongBoxBacked(boolean arg0) { return this; }

	// GEN member:android.security.keystore.KeyGenParameterSpec$Builder:public android.security.keystore.KeyGenParameterSpec$Builder setKeyValidityStart(java.util.Date);
	public android.security.keystore.KeyGenParameterSpec.Builder setKeyValidityStart(java.util.Date arg0) { return this; }
}

	public int getKeySize() {
		return keySize;
	}

	public String[] getBlockModes() {
		return blockModes;
	}

	public int getPurposes() {
		return purposes;
	}

	public String[] getEncryptionPaddings() {
		return encryptionPaddings;
	}

	public boolean isUserAuthenticationRequired() {
		return userAuthenticationRequired;
	}

	public String getKeystoreAlias() {
		return keystoreAlias;
	}
}
