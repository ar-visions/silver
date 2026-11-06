package android.drm;

// android.jar's shape, made by gen.py
public class DrmManagerClient implements java.lang.AutoCloseable {
	// GEN member:android.drm.DrmManagerClient:public static final int ERROR_NONE = 0;
	public static final int ERROR_NONE = 0;

	// GEN member:android.drm.DrmManagerClient:public static final int ERROR_UNKNOWN = -2000;
	public static final int ERROR_UNKNOWN = -2000;

	// GEN member:android.drm.DrmManagerClient:public android.drm.DrmManagerClient(android.content.Context);
	public DrmManagerClient(android.content.Context arg0) {}

	// GEN member:android.drm.DrmManagerClient:protected void finalize() throws java.lang.Throwable;
	protected void finalize() throws java.lang.Throwable {}

	// GEN member:android.drm.DrmManagerClient:public void close();
	public void close() {}

	// GEN member:android.drm.DrmManagerClient:public void release();
	public void release() {}

	// GEN member:android.drm.DrmManagerClient:public java.lang.String[] getAvailableDrmEngines();
	public java.lang.String[] getAvailableDrmEngines() { return new java.lang.String[0]; }

	// GEN member:android.drm.DrmManagerClient:public android.content.ContentValues getConstraints(java.lang.String, int);
	public android.content.ContentValues getConstraints(java.lang.String arg0, int arg1) { return null; }

	// GEN member:android.drm.DrmManagerClient:public android.content.ContentValues getMetadata(java.lang.String);
	public android.content.ContentValues getMetadata(java.lang.String arg0) { return null; }

	// GEN member:android.drm.DrmManagerClient:public android.content.ContentValues getConstraints(android.net.Uri, int);
	public android.content.ContentValues getConstraints(android.net.Uri arg0, int arg1) { return null; }

	// GEN member:android.drm.DrmManagerClient:public android.content.ContentValues getMetadata(android.net.Uri);
	public android.content.ContentValues getMetadata(android.net.Uri arg0) { return null; }

	// GEN member:android.drm.DrmManagerClient:public boolean canHandle(java.lang.String, java.lang.String);
	public boolean canHandle(java.lang.String arg0, java.lang.String arg1) { return false; }

	// GEN member:android.drm.DrmManagerClient:public boolean canHandle(android.net.Uri, java.lang.String);
	public boolean canHandle(android.net.Uri arg0, java.lang.String arg1) { return false; }

	// GEN member:android.drm.DrmManagerClient:public int getDrmObjectType(java.lang.String, java.lang.String);
	public int getDrmObjectType(java.lang.String arg0, java.lang.String arg1) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int getDrmObjectType(android.net.Uri, java.lang.String);
	public int getDrmObjectType(android.net.Uri arg0, java.lang.String arg1) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public java.lang.String getOriginalMimeType(java.lang.String);
	public java.lang.String getOriginalMimeType(java.lang.String arg0) { return null; }

	// GEN member:android.drm.DrmManagerClient:public java.lang.String getOriginalMimeType(android.net.Uri);
	public java.lang.String getOriginalMimeType(android.net.Uri arg0) { return null; }

	// GEN member:android.drm.DrmManagerClient:public int checkRightsStatus(java.lang.String);
	public int checkRightsStatus(java.lang.String arg0) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int checkRightsStatus(android.net.Uri);
	public int checkRightsStatus(android.net.Uri arg0) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int checkRightsStatus(java.lang.String, int);
	public int checkRightsStatus(java.lang.String arg0, int arg1) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int checkRightsStatus(android.net.Uri, int);
	public int checkRightsStatus(android.net.Uri arg0, int arg1) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int removeRights(java.lang.String);
	public int removeRights(java.lang.String arg0) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int removeRights(android.net.Uri);
	public int removeRights(android.net.Uri arg0) { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int removeAllRights();
	public int removeAllRights() { return 0; }

	// GEN member:android.drm.DrmManagerClient:public int openConvertSession(java.lang.String);
	public int openConvertSession(java.lang.String arg0) { return 0; }

	protected DrmManagerClient() {}
}
