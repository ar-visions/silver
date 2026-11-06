package android.net.http;

// android.jar's shape, made by gen.py
public abstract class NetworkException extends android.net.http.HttpException {
	// GEN member:android.net.http.NetworkException:public static final int ERROR_ADDRESS_UNREACHABLE = 9;
	public static final int ERROR_ADDRESS_UNREACHABLE = 9;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_CONNECTION_CLOSED = 5;
	public static final int ERROR_CONNECTION_CLOSED = 5;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_CONNECTION_REFUSED = 7;
	public static final int ERROR_CONNECTION_REFUSED = 7;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_CONNECTION_RESET = 8;
	public static final int ERROR_CONNECTION_RESET = 8;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_CONNECTION_TIMED_OUT = 6;
	public static final int ERROR_CONNECTION_TIMED_OUT = 6;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_HOSTNAME_NOT_RESOLVED = 1;
	public static final int ERROR_HOSTNAME_NOT_RESOLVED = 1;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_INTERNET_DISCONNECTED = 2;
	public static final int ERROR_INTERNET_DISCONNECTED = 2;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_NETWORK_CHANGED = 3;
	public static final int ERROR_NETWORK_CHANGED = 3;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_OTHER = 11;
	public static final int ERROR_OTHER = 11;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_QUIC_PROTOCOL_FAILED = 10;
	public static final int ERROR_QUIC_PROTOCOL_FAILED = 10;

	// GEN member:android.net.http.NetworkException:public static final int ERROR_TIMED_OUT = 4;
	public static final int ERROR_TIMED_OUT = 4;

	// GEN member:android.net.http.NetworkException:public android.net.http.NetworkException(java.lang.String, java.lang.Throwable);
	public NetworkException(java.lang.String arg0, java.lang.Throwable arg1) {}

	// GEN member:android.net.http.NetworkException:public abstract int getErrorCode();
	public abstract int getErrorCode();

	// GEN member:android.net.http.NetworkException:public abstract boolean isImmediatelyRetryable();
	public abstract boolean isImmediatelyRetryable();

	protected NetworkException() {}
}
