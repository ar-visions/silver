package android.util;

// Android's StatsEvent (a system API): a statsd atom, kept nowhere here
public final class StatsEvent {

	private final int atomId;

	private StatsEvent(int atomId) {
		this.atomId = atomId;
	}

	public static Builder newBuilder() {
		return new Builder();
	}

	public int getAtomId() {
		return atomId;
	}

	public byte[] getBytes() {
		return new byte[0];
	}

	public int getNumBytes() {
		return 0;
	}

	public void release() {}

	public static final class Builder {
		private int atomId;

		public Builder setAtomId(int atomId) {
			this.atomId = atomId;
			return this;
		}

		public Builder writeBoolean(boolean value) { return this; }
		public Builder writeInt(int value) { return this; }
		public Builder writeLong(long value) { return this; }
		public Builder writeFloat(float value) { return this; }
		public Builder writeString(String value) { return this; }
		public Builder writeByteArray(byte[] value) { return this; }
		public Builder writeAttributionChain(int[] uids, String[] tags) { return this; }
		public Builder writeKeyValuePairs(android.util.SparseIntArray intMap, android.util.SparseLongArray longMap, android.util.SparseArray<String> stringMap, android.util.SparseArray<Float> floatMap) { return this; }
		public Builder addBooleanAnnotation(byte annotationId, boolean value) { return this; }
		public Builder addIntAnnotation(byte annotationId, int value) { return this; }
		public Builder usePooledBuffer() { return this; }

		public StatsEvent build() {
			return new StatsEvent(atomId);
		}
	}
}
