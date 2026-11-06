package android.view;

import android.os.Parcel;
import android.os.Parcelable;

public abstract class AbsSavedState implements Parcelable {
	public static final AbsSavedState EMPTY_STATE = new AbsSavedState() {};

	private final Parcelable mSuperState;

	private AbsSavedState() {
		mSuperState = null;
	}

	protected AbsSavedState(Parcelable superState) {
		if (superState == null)
			throw new IllegalArgumentException("superState must not be null");
		mSuperState = superState != EMPTY_STATE ? superState : null;
	}

	protected AbsSavedState(Parcel source) {
		this(source, null);
	}

	protected AbsSavedState(Parcel source, ClassLoader loader) {
		Parcelable superState = source.readParcelable(loader);
		mSuperState = superState != null ? superState : EMPTY_STATE;
	}

	public final Parcelable getSuperState() {
		return mSuperState;
	}

	public int describeContents() {
		return 0;
	}

	public void writeToParcel(Parcel dest, int flags) {
		dest.writeParcelable(mSuperState, flags);
	}

	public static final Parcelable.Creator<AbsSavedState> CREATOR = new Parcelable.ClassLoaderCreator<AbsSavedState>() {
		public AbsSavedState createFromParcel(Parcel in, ClassLoader loader) {
			Parcelable superState = in.readParcelable(loader);
			if (superState != null)
				throw new IllegalStateException("superState must be null");
			return EMPTY_STATE;
		}

		@Override
		public AbsSavedState createFromParcel(Parcel in) {
			return createFromParcel(in, null);
		}

		@Override
		public AbsSavedState[] newArray(int size) {
			return new AbsSavedState[size];
		}
	};
}
