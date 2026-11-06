package android.app;

import android.graphics.drawable.Icon;

public class Person implements android.os.Parcelable {

	private CharSequence mName;
	private Icon mIcon;
	private String mUri;
	private String mKey;
	private boolean mIsBot;
	private boolean mIsImportant;

	private Person(Builder builder) {
		mName = builder.mName;
		mIcon = builder.mIcon;
		mUri = builder.mUri;
		mKey = builder.mKey;
		mIsBot = builder.mIsBot;
		mIsImportant = builder.mIsImportant;
	}

	public String getUri() {
		return mUri;
	}

	public CharSequence getName() {
		return mName;
	}

	public Icon getIcon() {
		return mIcon;
	}

	public String getKey() {
		return mKey;
	}

	public boolean isBot() {
		return mIsBot;
	}

	public boolean isImportant() {
		return mIsImportant;
	}

	public static class Builder {

		private CharSequence mName;
		private Icon mIcon;
		private String mUri;
		private String mKey;
		private boolean mIsBot;
		private boolean mIsImportant;

		public Builder() {
		}

		public Person build() {
			return new Person(this);
		}

		public Builder setName(CharSequence name) {
			mName = name;
			return this;
		}

		public Builder setIcon(Icon icon) {
			mIcon = icon;
			return this;
		}

		public Builder setUri(String uri) {
			mUri = uri;
			return this;
		}

		public Builder setKey(String key) {
			mKey = key;
			return this;
		}

		public Builder setImportant(boolean isImportant) {
			mIsImportant = isImportant;
			return this;
		}

		public Builder setBot(boolean isBot) {
			mIsBot = isBot;
			return this;
		}
	}
}
