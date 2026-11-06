package android.content;

import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.OutputStream;

public class ContentProviderClient implements AutoCloseable {
	private final ContentProvider contentProvider;

	ContentProviderClient(ContentProvider contentProvider) {
		this.contentProvider = contentProvider;
	}

	public ContentProvider getLocalContentProvider() {
		return contentProvider;
	}

	public Cursor query(Uri url, String[] projection, String selection, String[] selectionArgs, String sortOrder) throws RemoteException {
		return contentProvider.query(url, projection, selection, selectionArgs, sortOrder);
	}

	public String getType(Uri url) throws RemoteException {
		return contentProvider.getType(url);
	}

	public Uri insert(Uri url, ContentValues initialValues) throws RemoteException {
		return contentProvider.insert(url, initialValues);
	}

	public int delete(Uri url, String selection, String[] selectionArgs) throws RemoteException {
		return contentProvider.delete(url, selection, selectionArgs);
	}

	public int update(Uri url, ContentValues values, String selection, String[] selectionArgs) throws RemoteException {
		return contentProvider.update(url, values, selection, selectionArgs);
	}

	public ParcelFileDescriptor openFile(Uri url, String mode) throws RemoteException, FileNotFoundException {
		return contentProvider.openFile(url, mode);
	}

	public AssetFileDescriptor openAssetFile(Uri url, String mode) throws RemoteException, FileNotFoundException {
		return contentProvider.openAssetFile(url, mode);
	}

	public InputStream openInputStream(Uri url) throws FileNotFoundException, RemoteException {
		ParcelFileDescriptor fd = openFile(url, "r");
		return fd != null ? new ParcelFileDescriptor.AutoCloseInputStream(fd) : null;
	}

	public OutputStream openOutputStream(Uri url) throws FileNotFoundException, RemoteException {
		ParcelFileDescriptor fd = openFile(url, "w");
		return fd != null ? new ParcelFileDescriptor.AutoCloseOutputStream(fd) : null;
	}

	public Bundle call(String method, String arg, Bundle extras) throws RemoteException {
		return contentProvider.call(method, arg, extras);
	}

	public Bundle call(String authority, String method, String arg, Bundle extras) throws RemoteException {
		return contentProvider.call(authority, method, arg, extras);
	}

	public boolean release() {
		return true;
	}

	@Override
	public void close() {
		release();
	}

	// GEN member:android.content.ContentProviderClient:public android.database.Cursor query(android.net.Uri, java.lang.String[], java.lang.String, java.lang.String[], java.lang.String, android.os.CancellationSignal) throws android.os.RemoteException;
	public android.database.Cursor query(android.net.Uri arg0, java.lang.String[] arg1, java.lang.String arg2, java.lang.String[] arg3, java.lang.String arg4, android.os.CancellationSignal arg5) throws android.os.RemoteException { return null; }
}
