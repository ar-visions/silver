package android.media.browse;

// android.jar's shape, made by gen.py
public final class MediaBrowser {
	// GEN member:android.media.browse.MediaBrowser:public android.media.browse.MediaBrowser(android.content.Context, android.content.ComponentName, android.media.browse.MediaBrowser$ConnectionCallback, android.os.Bundle);
	public MediaBrowser(android.content.Context arg0, android.content.ComponentName arg1, android.media.browse.MediaBrowser.ConnectionCallback arg2, android.os.Bundle arg3) {}

	// GEN member:android.media.browse.MediaBrowser:public void connect();
	public void connect() {}

	// GEN member:android.media.browse.MediaBrowser:public void disconnect();
	public void disconnect() {}

	// GEN member:android.media.browse.MediaBrowser:public boolean isConnected();
	public boolean isConnected() { return false; }

	// GEN member:android.media.browse.MediaBrowser:public android.content.ComponentName getServiceComponent();
	public android.content.ComponentName getServiceComponent() { return null; }

	// GEN member:android.media.browse.MediaBrowser:public java.lang.String getRoot();
	public java.lang.String getRoot() { return null; }

	// GEN member:android.media.browse.MediaBrowser:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.media.browse.MediaBrowser:public android.media.session.MediaSession$Token getSessionToken();
	public android.media.session.MediaSession.Token getSessionToken() { return null; }

	// GEN member:android.media.browse.MediaBrowser:public void subscribe(java.lang.String, android.media.browse.MediaBrowser$SubscriptionCallback);
	public void subscribe(java.lang.String arg0, android.media.browse.MediaBrowser.SubscriptionCallback arg1) {}

	// GEN member:android.media.browse.MediaBrowser:public void subscribe(java.lang.String, android.os.Bundle, android.media.browse.MediaBrowser$SubscriptionCallback);
	public void subscribe(java.lang.String arg0, android.os.Bundle arg1, android.media.browse.MediaBrowser.SubscriptionCallback arg2) {}

	// GEN member:android.media.browse.MediaBrowser:public void unsubscribe(java.lang.String);
	public void unsubscribe(java.lang.String arg0) {}

	// GEN member:android.media.browse.MediaBrowser:public void unsubscribe(java.lang.String, android.media.browse.MediaBrowser$SubscriptionCallback);
	public void unsubscribe(java.lang.String arg0, android.media.browse.MediaBrowser.SubscriptionCallback arg1) {}

	protected MediaBrowser() {}

	// GEN class:android.media.browse.MediaBrowser$ConnectionCallback
	public class ConnectionCallback {
		// GEN member:android.media.browse.MediaBrowser$ConnectionCallback:public android.media.browse.MediaBrowser$ConnectionCallback();
		public ConnectionCallback() {}
	
		// GEN member:android.media.browse.MediaBrowser$ConnectionCallback:public void onConnected();
		public void onConnected() {}
	
		// GEN member:android.media.browse.MediaBrowser$ConnectionCallback:public void onConnectionSuspended();
		public void onConnectionSuspended() {}
	
		// GEN member:android.media.browse.MediaBrowser$ConnectionCallback:public void onConnectionFailed();
		public void onConnectionFailed() {}
	}


	// GEN class:android.media.browse.MediaBrowser$MediaItem
	public class MediaItem implements android.os.Parcelable {
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public static final int FLAG_BROWSABLE = 1;
		public static final int FLAG_BROWSABLE = 1;
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public static final int FLAG_PLAYABLE = 2;
		public static final int FLAG_PLAYABLE = 2;
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public android.media.browse.MediaBrowser$MediaItem(android.media.MediaDescription, int);
		public MediaItem(android.media.MediaDescription arg0, int arg1) {}
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public int getFlags();
		public int getFlags() { return 0; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public boolean isBrowsable();
		public boolean isBrowsable() { return false; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public boolean isPlayable();
		public boolean isPlayable() { return false; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public android.media.MediaDescription getDescription();
		public android.media.MediaDescription getDescription() { return null; }
	
		// GEN member:android.media.browse.MediaBrowser$MediaItem:public java.lang.String getMediaId();
		public java.lang.String getMediaId() { return null; }
	
		protected MediaItem() {}
	}


	// GEN class:android.media.browse.MediaBrowser$SubscriptionCallback
	public abstract class SubscriptionCallback {
		// GEN member:android.media.browse.MediaBrowser$SubscriptionCallback:public android.media.browse.MediaBrowser$SubscriptionCallback();
		public SubscriptionCallback() {}
	
		// GEN member:android.media.browse.MediaBrowser$SubscriptionCallback:public void onChildrenLoaded(java.lang.String, java.util.List<android.media.browse.MediaBrowser$MediaItem>);
		public void onChildrenLoaded(java.lang.String arg0, java.util.List<android.media.browse.MediaBrowser.MediaItem> arg1) {}
	
		// GEN member:android.media.browse.MediaBrowser$SubscriptionCallback:public void onChildrenLoaded(java.lang.String, java.util.List<android.media.browse.MediaBrowser$MediaItem>, android.os.Bundle);
		public void onChildrenLoaded(java.lang.String arg0, java.util.List<android.media.browse.MediaBrowser.MediaItem> arg1, android.os.Bundle arg2) {}
	
		// GEN member:android.media.browse.MediaBrowser$SubscriptionCallback:public void onError(java.lang.String);
		public void onError(java.lang.String arg0) {}
	
		// GEN member:android.media.browse.MediaBrowser$SubscriptionCallback:public void onError(java.lang.String, android.os.Bundle);
		public void onError(java.lang.String arg0, android.os.Bundle arg1) {}
	}

}
