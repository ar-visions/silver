package android.app;

import android.os.Bundle;

public class RemoteInput implements android.os.Parcelable {

	public static class Builder {

		public Builder(String resultKey) {}

		public Builder setLabel(CharSequence label) { return this; }

		public Builder setChoices(CharSequence[] choices) { return this; }

		public Builder setAllowFreeFormInput(boolean allowFreeFormInput) { return this; }

		public Builder addExtras(Bundle extras) { return this; }

		public RemoteInput build() {
			return new RemoteInput();
		}
	
	// GEN member:android.app.RemoteInput$Builder:public android.app.RemoteInput$Builder setAllowDataType(java.lang.String, boolean);
	public android.app.RemoteInput.Builder setAllowDataType(java.lang.String arg0, boolean arg1) { return this; }

	// GEN member:android.app.RemoteInput$Builder:public android.app.RemoteInput$Builder setEditChoicesBeforeSending(int);
	public android.app.RemoteInput.Builder setEditChoicesBeforeSending(int arg0) { return this; }
}

	// GEN member:android.app.RemoteInput:public static android.os.Bundle getResultsFromIntent(android.content.Intent);
	public static android.os.Bundle getResultsFromIntent(android.content.Intent arg0) { return null; }

	// GEN member:android.app.RemoteInput:public static void addResultsToIntent(android.app.RemoteInput[], android.content.Intent, android.os.Bundle);
	public static void addResultsToIntent(android.app.RemoteInput[] arg0, android.content.Intent arg1, android.os.Bundle arg2) {}
}
