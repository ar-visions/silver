package android.content;

public class LocusId implements android.os.Parcelable {

	private final String mId;

	public LocusId(String id) {
		mId = id;
	}

	public String getId() {
		return mId;
	}

	@Override
	public int hashCode() {
		return mId != null ? mId.hashCode() : 0;
	}

	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (!(obj instanceof LocusId))
			return false;
		LocusId other = (LocusId)obj;
		return mId == null ? other.mId == null : mId.equals(other.mId);
	}
}
