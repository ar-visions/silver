package android.media;

import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;

public class MediaDescription implements android.os.Parcelable {

	public Uri iconUri;
	public CharSequence title;
	public CharSequence subtitle;

	public static class Builder {

		MediaDescription description = new MediaDescription();

		public Builder setMediaId(String mediaId) { return this; }

		public Builder setTitle(CharSequence title) {
			description.title = title;
			return this;
		}

		public Builder setSubtitle(CharSequence subtitle) {
			description.subtitle = subtitle;
			return this;
		}

		public Builder setDescription(CharSequence description) { return this; }

		public Builder setIconBitmap(Bitmap iconBitmap) { return this; }

		public Builder setIconUri(Uri iconUri) {
			description.iconUri = iconUri;
			return this;
		}

		public Builder setExtras(Bundle extras) { return this; }

		public Builder setMediaUri(Uri mediaUri) { return this; }

		public MediaDescription build() {
			return description;
		}
	}

	// GEN member:android.media.MediaDescription:public java.lang.CharSequence getDescription();
	public java.lang.CharSequence getDescription() { return null; }

	// GEN member:android.media.MediaDescription:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.media.MediaDescription:public android.graphics.Bitmap getIconBitmap();
	public android.graphics.Bitmap getIconBitmap() { return null; }

	// GEN member:android.media.MediaDescription:public android.net.Uri getIconUri();
	public android.net.Uri getIconUri() { return null; }

	// GEN member:android.media.MediaDescription:public java.lang.String getMediaId();
	public java.lang.String getMediaId() { return null; }

	// GEN member:android.media.MediaDescription:public android.net.Uri getMediaUri();
	public android.net.Uri getMediaUri() { return null; }

	// GEN member:android.media.MediaDescription:public java.lang.CharSequence getSubtitle();
	public java.lang.CharSequence getSubtitle() { return null; }

	// GEN member:android.media.MediaDescription:public java.lang.CharSequence getTitle();
	public java.lang.CharSequence getTitle() { return null; }

	// GEN member:android.media.MediaDescription:public static final android.os.Parcelable$Creator<android.media.MediaDescription> CREATOR;
	public static final android.os.Parcelable.Creator<android.media.MediaDescription> CREATOR = null;
}
