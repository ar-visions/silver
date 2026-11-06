package com.google.android.gms.semanticlocation.internal;

import android.app.PendingIntent;
import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.RemoteException;
import com.google.android.gms.common.api.ApiMetadata;
import com.google.android.gms.common.api.Status;
import com.google.android.gms.common.api.internal.IStatusCallback;
import com.google.android.gms.semanticlocation.SemanticLocationEventRequest;

// microG 0.3.17's interface with code 6 as Play services has it
public interface ISemanticLocationService extends IInterface {
	String DESCRIPTOR = "com.google.android.gms.semanticlocation.internal.ISemanticLocationService";

	void registerSemanticLocationEvents(SemanticLocationParameters params, IStatusCallback callback, SemanticLocationEventRequest request, PendingIntent pendingIntent) throws RemoteException;

	void setIncognitoMode(SemanticLocationParameters params, IStatusCallback callback, boolean incognitoMode) throws RemoteException;

	void setIncognitoModeWithMetadata(SemanticLocationParameters params, IStatusCallback callback, boolean incognitoMode, ApiMetadata metadata) throws RemoteException;

	void unregisterSemanticLocationEvents(SemanticLocationParameters params, IStatusCallback callback, PendingIntent pendingIntent) throws RemoteException;

	abstract class Stub extends Binder implements ISemanticLocationService {
		static final int TRANSACTION_registerSemanticLocationEvents = 1;
		static final int TRANSACTION_unregisterSemanticLocationEvents = 2;
		static final int TRANSACTION_setIncognitoMode = 5;
		// the caller's ISemanticLocationCallbacks, not a status callback
		static final int TRANSACTION_getIncognitoMode = 6;
		static final String CALLBACKS = "com.google.android.gms.semanticlocation.internal.ISemanticLocationCallbacks";
		// ISemanticLocationCallbacks: (Status, boolean, ApiMetadata)
		static final int CALLBACK_BOOLEAN = 2;

		public Stub() {
			attachInterface(this, DESCRIPTOR);
		}

		public static ISemanticLocationService asInterface(IBinder obj) {
			if (obj == null)
				return null;
			IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
			if (iin instanceof ISemanticLocationService)
				return (ISemanticLocationService)iin;
			return null;
		}

		@Override
		public IBinder asBinder() {
			return this;
		}

		static <T> T typed(Parcel p, Parcelable.Creator<T> creator) {
			return p.readInt() != 0 ? creator.createFromParcel(p) : null;
		}

		// answers incognito off, as a Play services with no history
		static void answer(IBinder callbacks) throws RemoteException {
			if (callbacks == null)
				return;
			Parcel data = Parcel.obtain();
			try {
				data.writeInterfaceToken(CALLBACKS);
				data.writeInt(1);
				Status.SUCCESS.writeToParcel(data, 0);
				data.writeInt(0);
				data.writeInt(0);
				callbacks.transact(CALLBACK_BOOLEAN, data, null, IBinder.FLAG_ONEWAY);
			} finally {
				data.recycle();
			}
		}

		@Override
		protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
			if (code >= 1 && code <= 0xffffff)
				data.enforceInterface(DESCRIPTOR);
			if (code == INTERFACE_TRANSACTION) {
				reply.writeString(DESCRIPTOR);
				return true;
			}
			switch (code) {
			case TRANSACTION_registerSemanticLocationEvents: {
				SemanticLocationParameters params = typed(data, SemanticLocationParameters.CREATOR);
				IStatusCallback callback = IStatusCallback.Stub.asInterface(data.readStrongBinder());
				SemanticLocationEventRequest request = typed(data, SemanticLocationEventRequest.CREATOR);
				PendingIntent intent = typed(data, PendingIntent.CREATOR);
				registerSemanticLocationEvents(params, callback, request, intent);
				reply.writeNoException();
				return true;
			}
			case TRANSACTION_unregisterSemanticLocationEvents: {
				SemanticLocationParameters params = typed(data, SemanticLocationParameters.CREATOR);
				IStatusCallback callback = IStatusCallback.Stub.asInterface(data.readStrongBinder());
				PendingIntent intent = typed(data, PendingIntent.CREATOR);
				unregisterSemanticLocationEvents(params, callback, intent);
				reply.writeNoException();
				return true;
			}
			case TRANSACTION_setIncognitoMode: {
				SemanticLocationParameters params = typed(data, SemanticLocationParameters.CREATOR);
				IStatusCallback callback = IStatusCallback.Stub.asInterface(data.readStrongBinder());
				boolean incognito = data.readInt() != 0;
				setIncognitoMode(params, callback, incognito);
				reply.writeNoException();
				return true;
			}
			case TRANSACTION_getIncognitoMode: {
				typed(data, SemanticLocationParameters.CREATOR);
				IBinder callbacks = data.readStrongBinder();
				answer(callbacks);
				if (reply != null)
					reply.writeNoException();
				return true;
			}
			}
			return super.onTransact(code, data, reply, flags);
		}
	}
}
