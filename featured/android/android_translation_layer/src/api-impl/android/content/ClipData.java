package android.content;

import android.net.Uri;

public class ClipData implements android.os.Parcelable {

	public static class Item {

		public Item(Uri uri) {}

		public Item(CharSequence text, String mimeType, Intent intent, Uri uri) {}
	
	// GEN member:android.content.ClipData$Item:public java.lang.CharSequence coerceToStyledText(android.content.Context);
	public java.lang.CharSequence coerceToStyledText(android.content.Context arg0) { return null; }

	// GEN member:android.content.ClipData$Item:public java.lang.CharSequence coerceToText(android.content.Context);
	public java.lang.CharSequence coerceToText(android.content.Context arg0) { return null; }

	// GEN member:android.content.ClipData$Item:public java.lang.CharSequence getText();
	public java.lang.CharSequence getText() { return null; }

	// GEN member:android.content.ClipData$Item:public android.net.Uri getUri();
	public android.net.Uri getUri() { return null; }
}

	String text;

	public ClipData(ClipDescription description, Item item) {}

	public ClipData(CharSequence text, String[] mimeTypes, Item item) {}

	public static ClipData newPlainText(CharSequence label, CharSequence text) {
		ClipData clip = new ClipData(new ClipDescription(label, null), null);
		clip.text = text.toString();
		return clip;
	}

	public static ClipData newRawUri(CharSequence label, Uri uri) {
		ClipData clip = new ClipData(new ClipDescription(label, null), new Item(uri));
		clip.text = uri.toString();
		return clip;
	}

	public static ClipData newUri(ContentResolver resolver, CharSequence label, Uri uri) {
		ClipData clip = new ClipData(new ClipDescription(label, null), new Item(uri));
		clip.text = uri.toString();
		return clip;
	}

	public void addItem(ContentResolver resolver, Item item) {
	}

	public Item getItemAt(int index) {
		return null;
	}

	public int getItemCount() {
		return 0;
	}

	// GEN member:android.content.ClipData:public android.content.ClipDescription getDescription();
	public android.content.ClipDescription getDescription() { return null; }

	// GEN member:android.content.ClipData:public static android.content.ClipData newHtmlText(java.lang.CharSequence, java.lang.CharSequence, java.lang.String);
	public static android.content.ClipData newHtmlText(java.lang.CharSequence arg0, java.lang.CharSequence arg1, java.lang.String arg2) { return null; }

	// GEN member:android.content.ClipData:public static android.content.ClipData newIntent(java.lang.CharSequence, android.content.Intent);
	public static android.content.ClipData newIntent(java.lang.CharSequence arg0, android.content.Intent arg1) { return null; }

	// GEN member:android.content.ClipData:public void addItem(android.content.ClipData$Item);
	public void addItem(android.content.ClipData.Item arg0) {}
}
