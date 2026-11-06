package com.google.android.gms.common.api.internal;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;

// compile only: microG's own class is used at run time
public interface IStatusCallback extends IInterface {
	abstract class Stub extends Binder implements IStatusCallback {
		public static IStatusCallback asInterface(IBinder obj) {
			return null;
		}
	}
}
