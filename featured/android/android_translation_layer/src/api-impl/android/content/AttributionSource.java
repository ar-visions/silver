package android.content;

import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;

// who an access is for: this app, its uid and package
public final class AttributionSource implements Parcelable {
	private final int uid;
	private final String packageName;
	private final String attributionTag;

	public AttributionSource(int uid, String packageName, String attributionTag) {
		this.uid = uid;
		this.packageName = packageName;
		this.attributionTag = attributionTag;
	}

	public int getUid() { return uid; }
	public int getPid() { return Process.myPid(); }
	public String getPackageName() { return packageName; }
	public String getAttributionTag() { return attributionTag; }
	public AttributionSource getNext() { return null; }
	public boolean isTrusted(Context context) { return true; }
	public void enforceCallingUid() {}
	public boolean checkCallingUid() { return true; }

	public static AttributionSource myAttributionSource() {
		return new AttributionSource(Process.myUid(), null, null);
	}

	@Override
	public void writeToParcel(Parcel out, int flags) {
		out.writeInt(uid);
		out.writeString(packageName);
		out.writeString(attributionTag);
	}

	public static final Parcelable.Creator<AttributionSource> CREATOR =
		new Parcelable.Creator<AttributionSource>() {
			public AttributionSource createFromParcel(Parcel in) {
				return new AttributionSource(in.readInt(), in.readString(), in.readString());
			}

			public AttributionSource[] newArray(int size) {
				return new AttributionSource[size];
			}
		};

	public static final class Builder {
		private int uid;
		private String packageName;
		private String attributionTag;

		public Builder(int uid) { this.uid = uid; }
		public Builder setPackageName(String name) { packageName = name; return this; }
		public Builder setAttributionTag(String tag) { attributionTag = tag; return this; }
		public Builder setPid(int pid) { return this; }
		public Builder setNext(AttributionSource next) { return this; }
		public AttributionSource build() { return new AttributionSource(uid, packageName, attributionTag); }
	}
}
