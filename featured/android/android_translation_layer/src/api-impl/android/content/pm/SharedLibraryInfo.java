package android.content.pm;

// android.jar's shape, made by gen.py
public final class SharedLibraryInfo implements android.os.Parcelable {
	// GEN member:android.content.pm.SharedLibraryInfo:public static final android.os.Parcelable$Creator<android.content.pm.SharedLibraryInfo> CREATOR;
	public static final android.os.Parcelable.Creator<android.content.pm.SharedLibraryInfo> CREATOR = null;

	// GEN member:android.content.pm.SharedLibraryInfo:public static final int TYPE_BUILTIN = 0;
	public static final int TYPE_BUILTIN = 0;

	// GEN member:android.content.pm.SharedLibraryInfo:public static final int TYPE_DYNAMIC = 1;
	public static final int TYPE_DYNAMIC = 1;

	// GEN member:android.content.pm.SharedLibraryInfo:public static final int TYPE_SDK_PACKAGE = 3;
	public static final int TYPE_SDK_PACKAGE = 3;

	// GEN member:android.content.pm.SharedLibraryInfo:public static final int TYPE_STATIC = 2;
	public static final int TYPE_STATIC = 2;

	// GEN member:android.content.pm.SharedLibraryInfo:public static final int VERSION_UNDEFINED = -1;
	public static final int VERSION_UNDEFINED = -1;

	// GEN member:android.content.pm.SharedLibraryInfo:public int getType();
	public int getType() { return 0; }

	// GEN member:android.content.pm.SharedLibraryInfo:public java.lang.String getName();
	public java.lang.String getName() { return null; }

	// GEN member:android.content.pm.SharedLibraryInfo:public int getVersion();
	public int getVersion() { return 0; }

	// GEN member:android.content.pm.SharedLibraryInfo:public long getLongVersion();
	public long getLongVersion() { return 0; }

	// GEN member:android.content.pm.SharedLibraryInfo:public android.content.pm.VersionedPackage getDeclaringPackage();
	public android.content.pm.VersionedPackage getDeclaringPackage() { return null; }

	// GEN member:android.content.pm.SharedLibraryInfo:public java.util.List<android.content.pm.VersionedPackage> getDependentPackages();
	public java.util.List<android.content.pm.VersionedPackage> getDependentPackages() { return new java.util.ArrayList<>(); }

	// GEN member:android.content.pm.SharedLibraryInfo:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.content.pm.SharedLibraryInfo:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.content.pm.SharedLibraryInfo:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected SharedLibraryInfo() {}
}
