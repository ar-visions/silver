package android.net;

public class NetworkRequest implements android.os.Parcelable {

	public class Builder {

		public NetworkRequest build() {
			return new NetworkRequest();
		}

		public Builder addTransportType(int transport) {
			return this;
		}

		public Builder removeCapability(int capability) {
			return this;
		}

		public Builder addCapability(int capability) {
			return this;
		}
	}

	public int[] getCapabilities() {
		return new int[0];
	}

	public int[] getTransportTypes() {
		return new int[0];
	}

	public boolean hasCapability(int capability) {
		return false;
	}

	public boolean hasTransport(int transport) {
		return false;
	}

	public boolean canBeSatisfiedBy(NetworkCapabilities nc) {
		return true;
	}

	// GEN member:android.net.NetworkRequest:public android.net.NetworkSpecifier getNetworkSpecifier();
	public android.net.NetworkSpecifier getNetworkSpecifier() { return null; }
}
