package android.os;

// android.jar's shape, made by gen.py
public final class SharedMemory implements android.os.Parcelable, java.io.Closeable {
	// GEN member:android.os.SharedMemory:public static final android.os.Parcelable$Creator<android.os.SharedMemory> CREATOR;
	public static final android.os.Parcelable.Creator<android.os.SharedMemory> CREATOR = null;

	// GEN member:android.os.SharedMemory:public static android.os.SharedMemory create(java.lang.String, int) throws android.system.ErrnoException;
	public static android.os.SharedMemory create(java.lang.String arg0, int arg1) throws android.system.ErrnoException { return null; }

	// GEN member:android.os.SharedMemory:public static android.os.SharedMemory fromFileDescriptor(android.os.ParcelFileDescriptor);
	public static android.os.SharedMemory fromFileDescriptor(android.os.ParcelFileDescriptor arg0) { return null; }

	// GEN member:android.os.SharedMemory:public boolean setProtect(int);
	public boolean setProtect(int arg0) { return false; }

	// GEN member:android.os.SharedMemory:public int getSize();
	public int getSize() { return 0; }

	// GEN member:android.os.SharedMemory:public java.nio.ByteBuffer mapReadWrite() throws android.system.ErrnoException;
	public java.nio.ByteBuffer mapReadWrite() throws android.system.ErrnoException { return null; }

	// GEN member:android.os.SharedMemory:public java.nio.ByteBuffer mapReadOnly() throws android.system.ErrnoException;
	public java.nio.ByteBuffer mapReadOnly() throws android.system.ErrnoException { return null; }

	// GEN member:android.os.SharedMemory:public java.nio.ByteBuffer map(int, int, int) throws android.system.ErrnoException;
	public java.nio.ByteBuffer map(int arg0, int arg1, int arg2) throws android.system.ErrnoException { return null; }

	// GEN member:android.os.SharedMemory:public static void unmap(java.nio.ByteBuffer);
	public static void unmap(java.nio.ByteBuffer arg0) {}

	// GEN member:android.os.SharedMemory:public void close();
	public void close() {}

	// GEN member:android.os.SharedMemory:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.os.SharedMemory:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	protected SharedMemory() {}
}
