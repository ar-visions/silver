package android.app.blob;

// android.jar's shape, made by gen.py
public class BlobStoreManager {
	// GEN member:android.app.blob.BlobStoreManager:public long createSession(android.app.blob.BlobHandle) throws java.io.IOException;
	public long createSession(android.app.blob.BlobHandle arg0) throws java.io.IOException { return 0; }

	// GEN member:android.app.blob.BlobStoreManager:public android.app.blob.BlobStoreManager$Session openSession(long) throws java.io.IOException;
	public android.app.blob.BlobStoreManager.Session openSession(long arg0) throws java.io.IOException { return null; }

	// GEN member:android.app.blob.BlobStoreManager:public void abandonSession(long) throws java.io.IOException;
	public void abandonSession(long arg0) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public android.os.ParcelFileDescriptor openBlob(android.app.blob.BlobHandle) throws java.io.IOException;
	public android.os.ParcelFileDescriptor openBlob(android.app.blob.BlobHandle arg0) throws java.io.IOException { return null; }

	// GEN member:android.app.blob.BlobStoreManager:public void acquireLease(android.app.blob.BlobHandle, int, long) throws java.io.IOException;
	public void acquireLease(android.app.blob.BlobHandle arg0, int arg1, long arg2) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public void acquireLease(android.app.blob.BlobHandle, java.lang.CharSequence, long) throws java.io.IOException;
	public void acquireLease(android.app.blob.BlobHandle arg0, java.lang.CharSequence arg1, long arg2) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public void acquireLease(android.app.blob.BlobHandle, int) throws java.io.IOException;
	public void acquireLease(android.app.blob.BlobHandle arg0, int arg1) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public void acquireLease(android.app.blob.BlobHandle, java.lang.CharSequence) throws java.io.IOException;
	public void acquireLease(android.app.blob.BlobHandle arg0, java.lang.CharSequence arg1) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public void releaseLease(android.app.blob.BlobHandle) throws java.io.IOException;
	public void releaseLease(android.app.blob.BlobHandle arg0) throws java.io.IOException {}

	// GEN member:android.app.blob.BlobStoreManager:public long getRemainingLeaseQuotaBytes();
	public long getRemainingLeaseQuotaBytes() { return 0; }

	// GEN member:android.app.blob.BlobStoreManager:public java.util.List<android.app.blob.BlobHandle> getLeasedBlobs() throws java.io.IOException;
	public java.util.List<android.app.blob.BlobHandle> getLeasedBlobs() throws java.io.IOException { return new java.util.ArrayList<>(); }

	protected BlobStoreManager() {}

	// GEN class:android.app.blob.BlobStoreManager$Session
	public class Session implements java.io.Closeable {
		// GEN member:android.app.blob.BlobStoreManager$Session:public android.os.ParcelFileDescriptor openWrite(long, long) throws java.io.IOException;
		public android.os.ParcelFileDescriptor openWrite(long arg0, long arg1) throws java.io.IOException { return null; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public android.os.ParcelFileDescriptor openRead() throws java.io.IOException;
		public android.os.ParcelFileDescriptor openRead() throws java.io.IOException { return null; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public long getSize() throws java.io.IOException;
		public long getSize() throws java.io.IOException { return 0; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void close() throws java.io.IOException;
		public void close() throws java.io.IOException {}
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void abandon() throws java.io.IOException;
		public void abandon() throws java.io.IOException {}
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void allowPackageAccess(java.lang.String, byte[]) throws java.io.IOException;
		public void allowPackageAccess(java.lang.String arg0, byte[] arg1) throws java.io.IOException {}
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public boolean isPackageAccessAllowed(java.lang.String, byte[]) throws java.io.IOException;
		public boolean isPackageAccessAllowed(java.lang.String arg0, byte[] arg1) throws java.io.IOException { return false; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void allowSameSignatureAccess() throws java.io.IOException;
		public void allowSameSignatureAccess() throws java.io.IOException {}
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public boolean isSameSignatureAccessAllowed() throws java.io.IOException;
		public boolean isSameSignatureAccessAllowed() throws java.io.IOException { return false; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void allowPublicAccess() throws java.io.IOException;
		public void allowPublicAccess() throws java.io.IOException {}
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public boolean isPublicAccessAllowed() throws java.io.IOException;
		public boolean isPublicAccessAllowed() throws java.io.IOException { return false; }
	
		// GEN member:android.app.blob.BlobStoreManager$Session:public void commit(java.util.concurrent.Executor, java.util.function.Consumer<java.lang.Integer>) throws java.io.IOException;
		public void commit(java.util.concurrent.Executor arg0, java.util.function.Consumer<java.lang.Integer> arg1) throws java.io.IOException {}
	
		protected Session() {}
	}

}
