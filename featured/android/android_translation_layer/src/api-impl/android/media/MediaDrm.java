package android.media;

// android.jar's shape, made by gen.py
public final class MediaDrm implements java.lang.AutoCloseable {
	// GEN member:android.media.MediaDrm:public static final int EVENT_KEY_EXPIRED = 3;
	public static final int EVENT_KEY_EXPIRED = 3;

	// GEN member:android.media.MediaDrm:public static final int EVENT_KEY_REQUIRED = 2;
	public static final int EVENT_KEY_REQUIRED = 2;

	// GEN member:android.media.MediaDrm:public static final int EVENT_PROVISION_REQUIRED = 1;
	public static final int EVENT_PROVISION_REQUIRED = 1;

	// GEN member:android.media.MediaDrm:public static final int EVENT_SESSION_RECLAIMED = 5;
	public static final int EVENT_SESSION_RECLAIMED = 5;

	// GEN member:android.media.MediaDrm:public static final int EVENT_VENDOR_DEFINED = 4;
	public static final int EVENT_VENDOR_DEFINED = 4;

	// GEN member:android.media.MediaDrm:public static final int HDCP_LEVEL_UNKNOWN = 0;
	public static final int HDCP_LEVEL_UNKNOWN = 0;

	// GEN member:android.media.MediaDrm:public static final int HDCP_NONE = 1;
	public static final int HDCP_NONE = 1;

	// GEN member:android.media.MediaDrm:public static final int HDCP_NO_DIGITAL_OUTPUT = 2147483647;
	public static final int HDCP_NO_DIGITAL_OUTPUT = 2147483647;

	// GEN member:android.media.MediaDrm:public static final int HDCP_V1 = 2;
	public static final int HDCP_V1 = 2;

	// GEN member:android.media.MediaDrm:public static final int HDCP_V2 = 3;
	public static final int HDCP_V2 = 3;

	// GEN member:android.media.MediaDrm:public static final int HDCP_V2_1 = 4;
	public static final int HDCP_V2_1 = 4;

	// GEN member:android.media.MediaDrm:public static final int HDCP_V2_2 = 5;
	public static final int HDCP_V2_2 = 5;

	// GEN member:android.media.MediaDrm:public static final int HDCP_V2_3 = 6;
	public static final int HDCP_V2_3 = 6;

	// GEN member:android.media.MediaDrm:public static final int KEY_TYPE_OFFLINE = 2;
	public static final int KEY_TYPE_OFFLINE = 2;

	// GEN member:android.media.MediaDrm:public static final int KEY_TYPE_RELEASE = 3;
	public static final int KEY_TYPE_RELEASE = 3;

	// GEN member:android.media.MediaDrm:public static final int KEY_TYPE_STREAMING = 1;
	public static final int KEY_TYPE_STREAMING = 1;

	// GEN member:android.media.MediaDrm:public static final int OFFLINE_LICENSE_STATE_RELEASED = 2;
	public static final int OFFLINE_LICENSE_STATE_RELEASED = 2;

	// GEN member:android.media.MediaDrm:public static final int OFFLINE_LICENSE_STATE_UNKNOWN = 0;
	public static final int OFFLINE_LICENSE_STATE_UNKNOWN = 0;

	// GEN member:android.media.MediaDrm:public static final int OFFLINE_LICENSE_STATE_USABLE = 1;
	public static final int OFFLINE_LICENSE_STATE_USABLE = 1;

	// GEN member:android.media.MediaDrm:public static final java.lang.String PROPERTY_ALGORITHMS = "algorithms";
	public static final java.lang.String PROPERTY_ALGORITHMS = "algorithms";

	// GEN member:android.media.MediaDrm:public static final java.lang.String PROPERTY_DESCRIPTION = "description";
	public static final java.lang.String PROPERTY_DESCRIPTION = "description";

	// GEN member:android.media.MediaDrm:public static final java.lang.String PROPERTY_DEVICE_UNIQUE_ID = "deviceUniqueId";
	public static final java.lang.String PROPERTY_DEVICE_UNIQUE_ID = "deviceUniqueId";

	// GEN member:android.media.MediaDrm:public static final java.lang.String PROPERTY_VENDOR = "vendor";
	public static final java.lang.String PROPERTY_VENDOR = "vendor";

	// GEN member:android.media.MediaDrm:public static final java.lang.String PROPERTY_VERSION = "version";
	public static final java.lang.String PROPERTY_VERSION = "version";

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_HW_SECURE_ALL = 5;
	public static final int SECURITY_LEVEL_HW_SECURE_ALL = 5;

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_HW_SECURE_CRYPTO = 3;
	public static final int SECURITY_LEVEL_HW_SECURE_CRYPTO = 3;

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_HW_SECURE_DECODE = 4;
	public static final int SECURITY_LEVEL_HW_SECURE_DECODE = 4;

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_SW_SECURE_CRYPTO = 1;
	public static final int SECURITY_LEVEL_SW_SECURE_CRYPTO = 1;

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_SW_SECURE_DECODE = 2;
	public static final int SECURITY_LEVEL_SW_SECURE_DECODE = 2;

	// GEN member:android.media.MediaDrm:public static final int SECURITY_LEVEL_UNKNOWN = 0;
	public static final int SECURITY_LEVEL_UNKNOWN = 0;

	// GEN member:android.media.MediaDrm:public static boolean isCryptoSchemeSupported(java.util.UUID);
	public static boolean isCryptoSchemeSupported(java.util.UUID arg0) { return false; }

	// GEN member:android.media.MediaDrm:public static boolean isCryptoSchemeSupported(java.util.UUID, java.lang.String);
	public static boolean isCryptoSchemeSupported(java.util.UUID arg0, java.lang.String arg1) { return false; }

	// GEN member:android.media.MediaDrm:public static boolean isCryptoSchemeSupported(java.util.UUID, java.lang.String, int);
	public static boolean isCryptoSchemeSupported(java.util.UUID arg0, java.lang.String arg1, int arg2) { return false; }

	// GEN member:android.media.MediaDrm:public static java.util.List<java.util.UUID> getSupportedCryptoSchemes();
	public static java.util.List<java.util.UUID> getSupportedCryptoSchemes() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaDrm:public void clearOnExpirationUpdateListener();
	public void clearOnExpirationUpdateListener() {}

	// GEN member:android.media.MediaDrm:public void clearOnKeyStatusChangeListener();
	public void clearOnKeyStatusChangeListener() {}

	// GEN member:android.media.MediaDrm:public void clearOnSessionLostStateListener();
	public void clearOnSessionLostStateListener() {}

	// GEN member:android.media.MediaDrm:public void clearOnEventListener();
	public void clearOnEventListener() {}

	// GEN member:android.media.MediaDrm:public void closeSession(byte[]);
	public void closeSession(byte[] arg0) {}

	// GEN member:android.media.MediaDrm:public native void restoreKeys(byte[], byte[]);
	public void restoreKeys(byte[] arg0, byte[] arg1) {}

	// GEN member:android.media.MediaDrm:public native void removeKeys(byte[]);
	public void removeKeys(byte[] arg0) {}

	// GEN member:android.media.MediaDrm:public native java.util.HashMap<java.lang.String, java.lang.String> queryKeyStatus(byte[]);
	public java.util.HashMap<java.lang.String, java.lang.String> queryKeyStatus(byte[] arg0) { return null; }

	// GEN member:android.media.MediaDrm:public android.media.MediaDrm$ProvisionRequest getProvisionRequest();
	public android.media.MediaDrm.ProvisionRequest getProvisionRequest() { return null; }

	// GEN member:android.media.MediaDrm:public native java.util.List<byte[]> getOfflineLicenseKeySetIds();
	public java.util.List<byte[]> getOfflineLicenseKeySetIds() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaDrm:public native void removeOfflineLicense(byte[]);
	public void removeOfflineLicense(byte[] arg0) {}

	// GEN member:android.media.MediaDrm:public native int getOfflineLicenseState(byte[]);
	public int getOfflineLicenseState(byte[] arg0) { return 0; }

	// GEN member:android.media.MediaDrm:public native java.util.List<byte[]> getSecureStops();
	public java.util.List<byte[]> getSecureStops() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaDrm:public native java.util.List<byte[]> getSecureStopIds();
	public java.util.List<byte[]> getSecureStopIds() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaDrm:public native byte[] getSecureStop(byte[]);
	public byte[] getSecureStop(byte[] arg0) { return new byte[0]; }

	// GEN member:android.media.MediaDrm:public native void releaseSecureStops(byte[]);
	public void releaseSecureStops(byte[] arg0) {}

	// GEN member:android.media.MediaDrm:public native void removeSecureStop(byte[]);
	public void removeSecureStop(byte[] arg0) {}

	// GEN member:android.media.MediaDrm:public native void removeAllSecureStops();
	public void removeAllSecureStops() {}

	// GEN member:android.media.MediaDrm:public void releaseAllSecureStops();
	public void releaseAllSecureStops() {}

	// GEN member:android.media.MediaDrm:public native int getConnectedHdcpLevel();
	public int getConnectedHdcpLevel() { return 0; }

	// GEN member:android.media.MediaDrm:public native int getMaxHdcpLevel();
	public int getMaxHdcpLevel() { return 0; }

	// GEN member:android.media.MediaDrm:public native int getOpenSessionCount();
	public int getOpenSessionCount() { return 0; }

	// GEN member:android.media.MediaDrm:public native int getMaxSessionCount();
	public int getMaxSessionCount() { return 0; }

	// GEN member:android.media.MediaDrm:public static int getMaxSecurityLevel();
	public static int getMaxSecurityLevel() { return 0; }

	// GEN member:android.media.MediaDrm:public native int getSecurityLevel(byte[]);
	public int getSecurityLevel(byte[] arg0) { return 0; }

	// GEN member:android.media.MediaDrm:public native java.lang.String getPropertyString(java.lang.String);
	public java.lang.String getPropertyString(java.lang.String arg0) { return null; }

	// GEN member:android.media.MediaDrm:public native void setPropertyString(java.lang.String, java.lang.String);
	public void setPropertyString(java.lang.String arg0, java.lang.String arg1) {}

	// GEN member:android.media.MediaDrm:public native byte[] getPropertyByteArray(java.lang.String);
	public byte[] getPropertyByteArray(java.lang.String arg0) { return new byte[0]; }

	// GEN member:android.media.MediaDrm:public native void setPropertyByteArray(java.lang.String, byte[]);
	public void setPropertyByteArray(java.lang.String arg0, byte[] arg1) {}

	// GEN member:android.media.MediaDrm:public android.os.PersistableBundle getMetrics();
	public android.os.PersistableBundle getMetrics() { return null; }

	// GEN member:android.media.MediaDrm:public boolean requiresSecureDecoder(java.lang.String);
	public boolean requiresSecureDecoder(java.lang.String arg0) { return false; }

	// GEN member:android.media.MediaDrm:public native boolean requiresSecureDecoder(java.lang.String, int);
	public boolean requiresSecureDecoder(java.lang.String arg0, int arg1) { return false; }

	// GEN member:android.media.MediaDrm:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.media.MediaDrm:public void close();
	public void close() {}

	// GEN member:android.media.MediaDrm:public void release();
	public void release() {}

	// GEN member:android.media.MediaDrm:public android.media.MediaDrm$PlaybackComponent getPlaybackComponent(byte[]);
	public android.media.MediaDrm.PlaybackComponent getPlaybackComponent(byte[] arg0) { return null; }

	protected MediaDrm() {}

	// GEN class:android.media.MediaDrm$KeyRequest
	public final class KeyRequest {
		// GEN member:android.media.MediaDrm$KeyRequest:public static final int REQUEST_TYPE_INITIAL = 0;
		public static final int REQUEST_TYPE_INITIAL = 0;
	
		// GEN member:android.media.MediaDrm$KeyRequest:public static final int REQUEST_TYPE_NONE = 3;
		public static final int REQUEST_TYPE_NONE = 3;
	
		// GEN member:android.media.MediaDrm$KeyRequest:public static final int REQUEST_TYPE_RELEASE = 2;
		public static final int REQUEST_TYPE_RELEASE = 2;
	
		// GEN member:android.media.MediaDrm$KeyRequest:public static final int REQUEST_TYPE_RENEWAL = 1;
		public static final int REQUEST_TYPE_RENEWAL = 1;
	
		// GEN member:android.media.MediaDrm$KeyRequest:public static final int REQUEST_TYPE_UPDATE = 4;
		public static final int REQUEST_TYPE_UPDATE = 4;
	
		// GEN member:android.media.MediaDrm$KeyRequest:public byte[] getData();
		public byte[] getData() { return new byte[0]; }
	
		// GEN member:android.media.MediaDrm$KeyRequest:public java.lang.String getDefaultUrl();
		public java.lang.String getDefaultUrl() { return null; }
	
		// GEN member:android.media.MediaDrm$KeyRequest:public int getRequestType();
		public int getRequestType() { return 0; }
	
		protected KeyRequest() {}
	}


	// GEN class:android.media.MediaDrm$MediaDrmStateException
	public final class MediaDrmStateException extends java.lang.IllegalStateException implements android.media.MediaDrmThrowable {
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public int getErrorCode();
		public int getErrorCode() { return 0; }
	
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public int getVendorError();
		public int getVendorError() { return 0; }
	
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public int getOemError();
		public int getOemError() { return 0; }
	
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public int getErrorContext();
		public int getErrorContext() { return 0; }
	
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public boolean isTransient();
		public boolean isTransient() { return false; }
	
		// GEN member:android.media.MediaDrm$MediaDrmStateException:public java.lang.String getDiagnosticInfo();
		public java.lang.String getDiagnosticInfo() { return null; }
	
		protected MediaDrmStateException() {}
	}


	// GEN class:android.media.MediaDrm$PlaybackComponent
	public final class PlaybackComponent {
		// GEN member:android.media.MediaDrm$PlaybackComponent:public void setLogSessionId(android.media.metrics.LogSessionId);
		public void setLogSessionId(android.media.metrics.LogSessionId arg0) {}
	
		// GEN member:android.media.MediaDrm$PlaybackComponent:public android.media.metrics.LogSessionId getLogSessionId();
		public android.media.metrics.LogSessionId getLogSessionId() { return null; }
	
		protected PlaybackComponent() {}
	}


	// GEN class:android.media.MediaDrm$ProvisionRequest
	public final class ProvisionRequest {
		// GEN member:android.media.MediaDrm$ProvisionRequest:public byte[] getData();
		public byte[] getData() { return new byte[0]; }
	
		// GEN member:android.media.MediaDrm$ProvisionRequest:public java.lang.String getDefaultUrl();
		public java.lang.String getDefaultUrl() { return null; }
	
		protected ProvisionRequest() {}
	}


	// GEN class:android.media.MediaDrm$OnEventListener
	public interface OnEventListener {
		// GEN member:android.media.MediaDrm$OnEventListener:public abstract void onEvent(android.media.MediaDrm, byte[], int, int, byte[]);
		default void onEvent(android.media.MediaDrm arg0, byte[] arg1, int arg2, int arg3, byte[] arg4) {}
	}


	// GEN member:android.media.MediaDrm:public android.media.MediaDrm$KeyRequest getKeyRequest(byte[], byte[], java.lang.String, int, java.util.HashMap<java.lang.String, java.lang.String>) throws android.media.NotProvisionedException;
	public android.media.MediaDrm.KeyRequest getKeyRequest(byte[] arg0, byte[] arg1, java.lang.String arg2, int arg3, java.util.HashMap<java.lang.String, java.lang.String> arg4) throws android.media.NotProvisionedException { return null; }

	// GEN member:android.media.MediaDrm:public byte[] openSession() throws android.media.NotProvisionedException, android.media.ResourceBusyException;
	public byte[] openSession() throws android.media.NotProvisionedException, android.media.ResourceBusyException { return new byte[0]; }

	// GEN member:android.media.MediaDrm:public native byte[] provideKeyResponse(byte[], byte[]) throws android.media.DeniedByServerException, android.media.NotProvisionedException;
	public byte[] provideKeyResponse(byte[] arg0, byte[] arg1) throws android.media.DeniedByServerException, android.media.NotProvisionedException { return new byte[0]; }

	// GEN member:android.media.MediaDrm:public void provideProvisionResponse(byte[]) throws android.media.DeniedByServerException;
	public void provideProvisionResponse(byte[] arg0) throws android.media.DeniedByServerException {}
}
