package com.google.android.gms.common.internal.safeparcel;

import android.os.Parcelable;

// compile only: microG's own class is used at run time
public interface SafeParcelableCreatorAndWriter<T> extends Parcelable.Creator<T> {
}
