package android.os.storage;

import android.content.Context;

public class StorageVolume implements android.os.Parcelable {
	public boolean isPrimary() { return true; }
	public String getPath() { return ""; }
	public String getDescription(Context context) { return ""; }

	// GEN member:android.os.storage.StorageVolume:public java.lang.String getState();
	public java.lang.String getState() { return null; }

	// GEN member:android.os.storage.StorageVolume:public java.lang.String getUuid();
	public java.lang.String getUuid() { return null; }
}
