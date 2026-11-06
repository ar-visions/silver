package android.app;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.media.AudioAttributes;
import android.media.session.MediaSession;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcelable;
import android.widget.RemoteViews;
import java.util.ArrayList;
import java.util.List;

public class Notification implements Parcelable {

	public static final AudioAttributes AUDIO_ATTRIBUTES_DEFAULT = new AudioAttributes();

	public long when;

	public int audioStreamType;

	public int icon;

	public PendingIntent deleteIntent;

	public CharSequence tickerText;

	public int iconLevel;

	public RemoteViews bigContentView;

	public RemoteViews contentView;

	public RemoteViews headsUpContentView;

	public long[] vibrate;

	public int ledARGB;

	public int ledOnMS;

	public int ledOffMS;

	public int flags;

	public int defaults;

	public Uri sound;

	public AudioAttributes audioAttributes;

	public Bundle extras;

	public String category;

	String text;
	String title;
	List<Action> actions = new ArrayList<Action>();
	PendingIntent intent;
	String iconPath;
	boolean ongoing;
	Style style;

	public String toString() {
		return "Notification [" + title + ", " + text + ", " + actions + "]";
	}

	public String getGroup() { return null; }

	public static class Builder {
		private final Context mContext;
		private Notification notification;

		public Builder(Context context) {
			mContext = context;
			notification = new Notification();
		}

		public Builder(Context context, String tag) {
			this(context);
		}

		public Builder setWhen(long when) { return this; }

		public Builder setSmallIcon(int icon, int level) {
			notification.iconPath = mContext.getString(icon);
			return this;
		}

		public Builder setContent(RemoteViews contentView) { return this; }

		public Builder setTicker(CharSequence tickerText, RemoteViews contentView) { return this; }

		public Builder setVibrate(long[] pattern) { return this; }

		public Builder setLights(int argb, int onMs, int offMs) { return this; }

		public Builder setOngoing(boolean ongoing) {
			notification.ongoing = ongoing;
			return this;
		}

		public Builder setOnlyAlertOnce(boolean onlyAlertOnce) { return this; }

		public Builder setAutoCancel(boolean autoCancel) { return this; }

		public Builder setDefaults(int defaults) { return this; }

		public Builder setContentTitle(CharSequence title) {
			notification.title = title != null ? title.toString() : null;
			return this;
		}

		public Builder setContentText(CharSequence text) {
			notification.text = text != null ? text.toString() : null;
			return this;
		}

		public Builder setContentInfo(CharSequence info) { return this; }

		public Builder setContentIntent(PendingIntent intent) {
			notification.intent = intent;
			return this;
		}

		public Builder setDeleteIntent(PendingIntent intent) { return this; }

		public Builder setFullScreenIntent(PendingIntent intent, boolean highPriority) { return this; }

		public Builder setLargeIcon(Bitmap icon) { return this; }

		public Builder setNumber(int number) { return this; }

		public Builder setProgress(int max, int progress, boolean indeterminate) { return this; }

		public Builder setSubText(CharSequence subText) { return this; }

		public Builder setUsesChronometer(boolean useChronometer) { return this; }

		public Builder setPriority(int priority) { return this; }

		public Builder setShowWhen(boolean showWhen) { return this; }

		public Builder setLocalOnly(boolean localOnly) { return this; }

		public Builder setGroup(String group) { return this; }

		public Builder setSortKey(String sortKey) { return this; }

		public Builder setGroupSummary(boolean isGroupSummary) { return this; }

		public Builder setCategory(String category) { return this; }

		public Builder setColor(int argb) { return this; }

		public Builder setVisibility(int visibility) { return this; }

		public Builder setPublicVersion(Notification notification) { return this; }

		public Builder setSound(Uri sound, AudioAttributes audioAttributes) { return this; }

		public Builder addAction(Action action) {
			notification.actions.add(action);
			return this;
		}

		public Builder setStyle(Style style) {
			notification.style = style;
			if (style instanceof MediaStyle) {
				notification.ongoing = true;
			}
			return this;
		}

		public Builder setExtras(Bundle extras) { return this; }

		public Builder setLargeIcon(Icon icon) { return this; }

		public Builder setRemoteInputHistory(CharSequence[] history) { return this; }

		public Builder setBadgeIconType(int iconType) { return this; }

		public Builder setSettingsText(CharSequence settingsText) { return this; }

		public Builder setShortcutId(String shortcutId) { return this; }

		public Builder setTimeoutAfter(long timeout) { return this; }

		public Builder setGroupAlertBehavior(int groupAlertBehavior) { return this; }

		public Builder setSound(Uri sound) { return this; }

		public Builder setSmallIcon(int icon) { return this.setSmallIcon(icon, -1); }

		public Builder setTicker(CharSequence tickerText) { return this; }

		public Builder addExtras(Bundle bundle) { return this; }

		public Notification build() {
			return notification;
		}
	
	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder addPerson(android.app.Person);
	public android.app.Notification.Builder addPerson(android.app.Person arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder addPerson(java.lang.String);
	public android.app.Notification.Builder addPerson(java.lang.String arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setAllowSystemGeneratedContextualActions(boolean);
	public android.app.Notification.Builder setAllowSystemGeneratedContextualActions(boolean arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setBubbleMetadata(android.app.Notification$BubbleMetadata);
	public android.app.Notification.Builder setBubbleMetadata(android.app.Notification.BubbleMetadata arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setColorized(boolean);
	public android.app.Notification.Builder setColorized(boolean arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setCustomBigContentView(android.widget.RemoteViews);
	public android.app.Notification.Builder setCustomBigContentView(android.widget.RemoteViews arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setCustomContentView(android.widget.RemoteViews);
	public android.app.Notification.Builder setCustomContentView(android.widget.RemoteViews arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setCustomHeadsUpContentView(android.widget.RemoteViews);
	public android.app.Notification.Builder setCustomHeadsUpContentView(android.widget.RemoteViews arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setForegroundServiceBehavior(int);
	public android.app.Notification.Builder setForegroundServiceBehavior(int arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setLocusId(android.content.LocusId);
	public android.app.Notification.Builder setLocusId(android.content.LocusId arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setSmallIcon(android.graphics.drawable.Icon);
	public android.app.Notification.Builder setSmallIcon(android.graphics.drawable.Icon arg0) { return this; }

	// GEN member:android.app.Notification$Builder:public android.app.Notification$Builder setChannelId(java.lang.String);
	public android.app.Notification.Builder setChannelId(java.lang.String arg0) { return this; }
}

	public static class Action implements android.os.Parcelable {

		int icon;
		String title;
		PendingIntent intent;

		public String toString() {
			return "Action [" + icon + ", " + title + ", " + intent + "]";
		}

		public static final class Builder {

			private Action action;
			public Builder(int icon, CharSequence title, PendingIntent intent) {
				action = new Action();
				action.icon = icon;
				action.title = String.valueOf(title);
				action.intent = intent;
			}

			public Builder(Icon icon, CharSequence title, PendingIntent intent) {
				this(0, title, intent);
			}

			public Builder addExtras(Bundle extras) { return this; }

			public Builder addRemoteInput(RemoteInput remoteInput) { return this; }

			public Builder setAllowGeneratedReplies(boolean allowGeneratedReplies) { return this; }

			public Action build() {
				return action;
			}
		
	// GEN member:android.app.Notification$Action$Builder:public android.app.Notification$Action$Builder setAuthenticationRequired(boolean);
	public android.app.Notification.Action.Builder setAuthenticationRequired(boolean arg0) { return this; }

	// GEN member:android.app.Notification$Action$Builder:public android.app.Notification$Action$Builder setContextual(boolean);
	public android.app.Notification.Action.Builder setContextual(boolean arg0) { return this; }

	// GEN member:android.app.Notification$Action$Builder:public android.app.Notification$Action$Builder setSemanticAction(int);
	public android.app.Notification.Action.Builder setSemanticAction(int arg0) { return this; }
}
	}

	public static abstract class Style {
	// GEN member:android.app.Notification$Style:public void setBuilder(android.app.Notification$Builder);
	public void setBuilder(android.app.Notification.Builder arg0) {}
}

	public static class MediaStyle extends Style {

		public MediaStyle setShowActionsInCompactView(int... viewActions) { return this; }

		public MediaStyle setMediaSession(MediaSession.Token token) { return this; }
	}

	public static class BigTextStyle extends Style {

		public BigTextStyle(Notification.Builder builder) {}

		public BigTextStyle setBigContentTitle(CharSequence title) { return this; }

		public BigTextStyle bigText(CharSequence text) { return this; }

		public BigTextStyle setSummaryText(CharSequence summaryText) { return this; }
	}

	public static class InboxStyle extends Style {

		public InboxStyle(Notification.Builder builder) {}

		public InboxStyle setBigContentTitle(CharSequence title) { return this; }

		public InboxStyle setSummaryText(CharSequence summaryText) { return this; }

		public InboxStyle addLine(CharSequence line) { return this; }
	}

	// GEN class:android.app.Notification$BigPictureStyle
	public class BigPictureStyle extends android.app.Notification.Style {
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle();
		public BigPictureStyle() {}
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle(android.app.Notification$Builder);
		public BigPictureStyle(android.app.Notification.Builder arg0) {}
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle setBigContentTitle(java.lang.CharSequence);
		public android.app.Notification.BigPictureStyle setBigContentTitle(java.lang.CharSequence arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle setSummaryText(java.lang.CharSequence);
		public android.app.Notification.BigPictureStyle setSummaryText(java.lang.CharSequence arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle setContentDescription(java.lang.CharSequence);
		public android.app.Notification.BigPictureStyle setContentDescription(java.lang.CharSequence arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle bigPicture(android.graphics.Bitmap);
		public android.app.Notification.BigPictureStyle bigPicture(android.graphics.Bitmap arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle bigPicture(android.graphics.drawable.Icon);
		public android.app.Notification.BigPictureStyle bigPicture(android.graphics.drawable.Icon arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle showBigPictureWhenCollapsed(boolean);
		public android.app.Notification.BigPictureStyle showBigPictureWhenCollapsed(boolean arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle bigLargeIcon(android.graphics.Bitmap);
		public android.app.Notification.BigPictureStyle bigLargeIcon(android.graphics.Bitmap arg0) { return null; }
	
		// GEN member:android.app.Notification$BigPictureStyle:public android.app.Notification$BigPictureStyle bigLargeIcon(android.graphics.drawable.Icon);
		public android.app.Notification.BigPictureStyle bigLargeIcon(android.graphics.drawable.Icon arg0) { return null; }
	}


	// GEN class:android.app.Notification$BubbleMetadata
	public final class BubbleMetadata implements android.os.Parcelable {
		// GEN member:android.app.Notification$BubbleMetadata:public java.lang.String getShortcutId();
		public java.lang.String getShortcutId() { return null; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public android.app.PendingIntent getIntent();
		public android.app.PendingIntent getIntent() { return null; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public android.app.PendingIntent getDeleteIntent();
		public android.app.PendingIntent getDeleteIntent() { return null; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public android.graphics.drawable.Icon getIcon();
		public android.graphics.drawable.Icon getIcon() { return null; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public int getDesiredHeight();
		public int getDesiredHeight() { return 0; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public int getDesiredHeightResId();
		public int getDesiredHeightResId() { return 0; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public boolean getAutoExpandBubble();
		public boolean getAutoExpandBubble() { return false; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public boolean isNotificationSuppressed();
		public boolean isNotificationSuppressed() { return false; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public boolean isBubbleSuppressable();
		public boolean isBubbleSuppressable() { return false; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public boolean isBubbleSuppressed();
		public boolean isBubbleSuppressed() { return false; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.app.Notification$BubbleMetadata:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected BubbleMetadata() {}
	}


	// GEN class:android.app.Notification$CallStyle
	public class CallStyle extends android.app.Notification.Style {
		// GEN member:android.app.Notification$CallStyle:public static final int CALL_TYPE_INCOMING = 1;
		public static final int CALL_TYPE_INCOMING = 1;
	
		// GEN member:android.app.Notification$CallStyle:public static final int CALL_TYPE_ONGOING = 2;
		public static final int CALL_TYPE_ONGOING = 2;
	
		// GEN member:android.app.Notification$CallStyle:public static final int CALL_TYPE_SCREENING = 3;
		public static final int CALL_TYPE_SCREENING = 3;
	
		// GEN member:android.app.Notification$CallStyle:public static final int CALL_TYPE_UNKNOWN = 0;
		public static final int CALL_TYPE_UNKNOWN = 0;
	
		// GEN member:android.app.Notification$CallStyle:public android.app.Notification$CallStyle setIsVideo(boolean);
		public android.app.Notification.CallStyle setIsVideo(boolean arg0) { return null; }
	
		// GEN member:android.app.Notification$CallStyle:public android.app.Notification$CallStyle setVerificationIcon(android.graphics.drawable.Icon);
		public android.app.Notification.CallStyle setVerificationIcon(android.graphics.drawable.Icon arg0) { return null; }
	
		// GEN member:android.app.Notification$CallStyle:public android.app.Notification$CallStyle setVerificationText(java.lang.CharSequence);
		public android.app.Notification.CallStyle setVerificationText(java.lang.CharSequence arg0) { return null; }
	
		// GEN member:android.app.Notification$CallStyle:public android.app.Notification$CallStyle setAnswerButtonColorHint(int);
		public android.app.Notification.CallStyle setAnswerButtonColorHint(int arg0) { return null; }
	
		// GEN member:android.app.Notification$CallStyle:public android.app.Notification$CallStyle setDeclineButtonColorHint(int);
		public android.app.Notification.CallStyle setDeclineButtonColorHint(int arg0) { return null; }
	
		protected CallStyle() {}
	}


	// GEN class:android.app.Notification$DecoratedCustomViewStyle
	public class DecoratedCustomViewStyle extends android.app.Notification.Style {
		// GEN member:android.app.Notification$DecoratedCustomViewStyle:public android.app.Notification$DecoratedCustomViewStyle();
		public DecoratedCustomViewStyle() {}
	}


	// GEN class:android.app.Notification$MessagingStyle
	public class MessagingStyle extends android.app.Notification.Style {
		// GEN member:android.app.Notification$MessagingStyle:public static final int MAXIMUM_RETAINED_MESSAGES = 25;
		public static final int MAXIMUM_RETAINED_MESSAGES = 25;
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle(java.lang.CharSequence);
		public MessagingStyle(java.lang.CharSequence arg0) {}
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle(android.app.Person);
		public MessagingStyle(android.app.Person arg0) {}
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Person getUser();
		public android.app.Person getUser() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public java.lang.CharSequence getUserDisplayName();
		public java.lang.CharSequence getUserDisplayName() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle setConversationTitle(java.lang.CharSequence);
		public android.app.Notification.MessagingStyle setConversationTitle(java.lang.CharSequence arg0) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public java.lang.CharSequence getConversationTitle();
		public java.lang.CharSequence getConversationTitle() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle addMessage(java.lang.CharSequence, long, java.lang.CharSequence);
		public android.app.Notification.MessagingStyle addMessage(java.lang.CharSequence arg0, long arg1, java.lang.CharSequence arg2) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle addMessage(java.lang.CharSequence, long, android.app.Person);
		public android.app.Notification.MessagingStyle addMessage(java.lang.CharSequence arg0, long arg1, android.app.Person arg2) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle addMessage(android.app.Notification$MessagingStyle$Message);
		public android.app.Notification.MessagingStyle addMessage(android.app.Notification.MessagingStyle.Message arg0) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle addHistoricMessage(android.app.Notification$MessagingStyle$Message);
		public android.app.Notification.MessagingStyle addHistoricMessage(android.app.Notification.MessagingStyle.Message arg0) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public java.util.List<android.app.Notification$MessagingStyle$Message> getMessages();
		public java.util.List<android.app.Notification.MessagingStyle.Message> getMessages() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.app.Notification$MessagingStyle:public java.util.List<android.app.Notification$MessagingStyle$Message> getHistoricMessages();
		public java.util.List<android.app.Notification.MessagingStyle.Message> getHistoricMessages() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.app.Notification$MessagingStyle:public android.app.Notification$MessagingStyle setGroupConversation(boolean);
		public android.app.Notification.MessagingStyle setGroupConversation(boolean arg0) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle:public boolean isGroupConversation();
		public boolean isGroupConversation() { return false; }
	
		protected MessagingStyle() {}
	
	// GEN class:android.app.Notification$MessagingStyle$Message
	public final class Message {
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.app.Notification$MessagingStyle$Message(java.lang.CharSequence, long, java.lang.CharSequence);
		public Message(java.lang.CharSequence arg0, long arg1, java.lang.CharSequence arg2) {}
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.app.Notification$MessagingStyle$Message(java.lang.CharSequence, long, android.app.Person);
		public Message(java.lang.CharSequence arg0, long arg1, android.app.Person arg2) {}
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.app.Notification$MessagingStyle$Message setData(java.lang.String, android.net.Uri);
		public android.app.Notification.MessagingStyle.Message setData(java.lang.String arg0, android.net.Uri arg1) { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public java.lang.CharSequence getText();
		public java.lang.CharSequence getText() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public long getTimestamp();
		public long getTimestamp() { return 0; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.os.Bundle getExtras();
		public android.os.Bundle getExtras() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public java.lang.CharSequence getSender();
		public java.lang.CharSequence getSender() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.app.Person getSenderPerson();
		public android.app.Person getSenderPerson() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public java.lang.String getDataMimeType();
		public java.lang.String getDataMimeType() { return null; }
	
		// GEN member:android.app.Notification$MessagingStyle$Message:public android.net.Uri getDataUri();
		public android.net.Uri getDataUri() { return null; }
	
		protected Message() {}
	}

}


	// GEN member:android.app.Notification:public android.graphics.drawable.Icon getLargeIcon();
	public android.graphics.drawable.Icon getLargeIcon() { return null; }
}
