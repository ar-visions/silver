package android.os;

import android.atl.ATLLoadedApp;
import android.content.Context;

public class Binder implements IBinder {
	// GEN default constructor, kept
	public Binder() {}


	private String mDescriptor;
	private IInterface mOwner;

	public void attachInterface(IInterface owner, String descriptor) {
		mOwner = owner;
		mDescriptor = descriptor;
	}

	public String getInterfaceDescriptor() {
		return mDescriptor;
	}

	public IInterface getOwner() {
		return mOwner;
	}

	public static void flushPendingCommands() {}

	public static long clearCallingIdentity() { return 0; }

	public static void restoreCallingIdentity(long identityToken) {}

	@Override
	public IInterface queryLocalInterface(String descriptor) {
		return (mDescriptor != null && mDescriptor.equals(descriptor)) ? mOwner : null;
	}

	@Override
	public final boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
		if (data != null)
			data.setDataPosition(0);
		// Android's execTransact gives onTransact a reply Parcel always
		Parcel answer = reply != null ? reply : Parcel.obtain();
		boolean r = true;
		try {
			r = onTransact(code, data, answer, flags);
		} catch (RuntimeException e) {
			// as execTransact: the error goes back in the reply
			if ((flags & FLAG_ONEWAY) != 0 || reply == null) {
				System.err.println("Binder: " + getClass().getName() + " threw " + e);
			} else {
				reply.setDataPosition(0);
				try {
					reply.writeException(e);
				} catch (Exception unknown) {
					System.err.println("Binder: " + getClass().getName() + " threw " + e);
				}
			}
		}
		if (reply != null)
			reply.setDataPosition(0);
		else
			answer.recycle();
		return r;
	}

	protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException { return false; }

	public static int getCallingUid() { return ATLLoadedApp.getPrimaryApplication().pkg.applicationInfo.uid; }

	// one process: every call comes from this process
	public static int getCallingPid() { return Process.myPid(); }

	// GEN member:android.os.Binder:public android.os.Binder(java.lang.String);
	public Binder(java.lang.String descriptor) { mDescriptor = descriptor; }
}
