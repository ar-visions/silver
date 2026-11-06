package android.content.pm;

// the package's signers; no rotation, so the history is them
public final class SigningInfo implements android.os.Parcelable {
	private final Signature[] signers;

	public SigningInfo() {
		this(new Signature[0]);
	}

	public SigningInfo(Signature[] signers) {
		this.signers = signers;
	}

	public Signature[] getApkContentsSigners() {
		return signers.clone();
	}

	public boolean hasMultipleSigners() {
		return signers.length > 1;
	}

	public Signature[] getSigningCertificateHistory() {
		return signers.clone();
	}

	public boolean hasPastSigningCertificates() {
		return false;
	}
}
