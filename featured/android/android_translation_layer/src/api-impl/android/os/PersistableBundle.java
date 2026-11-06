package android.os;

public class PersistableBundle extends BaseBundle implements android.os.Parcelable, java.lang.Cloneable {
	// GEN default constructor, kept
	public PersistableBundle() {}


	@Override
	public synchronized String toString() {
		return "PersistableBundle[" + mMap.toString() + "]";
	}

	// GEN member:android.os.PersistableBundle:public static final android.os.Parcelable$Creator<android.os.PersistableBundle> CREATOR;
	public static final android.os.Parcelable.Creator<android.os.PersistableBundle> CREATOR = null;

	// GEN member:android.os.PersistableBundle:public android.os.PersistableBundle getPersistableBundle(java.lang.String);
	public android.os.PersistableBundle getPersistableBundle(java.lang.String arg0) { return null; }

	// GEN member:android.os.PersistableBundle:public android.os.PersistableBundle(int);
	public PersistableBundle(int arg0) {}

	// GEN member:android.os.PersistableBundle:public void putPersistableBundle(java.lang.String, android.os.PersistableBundle);
	public void putPersistableBundle(java.lang.String arg0, android.os.PersistableBundle arg1) {}

	// GEN member:android.os.PersistableBundle:public android.os.PersistableBundle(android.os.PersistableBundle);
	public PersistableBundle(android.os.PersistableBundle arg0) {}

	// GEN member:android.os.PersistableBundle:public static final android.os.PersistableBundle EMPTY;
	public static final android.os.PersistableBundle EMPTY = null;
}
