package android.provider;

import android.net.Uri;

public class Contacts {
	public static class People implements android.provider.BaseColumns, android.provider.Contacts.PeopleColumns, android.provider.Contacts.PhonesColumns, android.provider.Contacts.PresenceColumns {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/people");

		public static final Uri CONTENT_FILTER_URI = Uri.parse("content://com.android.contacts/people_filter");
	}

	public interface PeopleColumns {
		public static final java.lang.String CUSTOM_RINGTONE = "custom_ringtone";
		public static final java.lang.String DISPLAY_NAME = "display_name";
		public static final java.lang.String LAST_TIME_CONTACTED = "last_time_contacted";
		public static final java.lang.String NAME = "name";
		public static final java.lang.String NOTES = "notes";
		public static final java.lang.String PHONETIC_NAME = "phonetic_name";
		public static final java.lang.String PHOTO_VERSION = "photo_version";
		public static final java.lang.String SEND_TO_VOICEMAIL = "send_to_voicemail";
		public static final java.lang.String STARRED = "starred";
		public static final java.lang.String TIMES_CONTACTED = "times_contacted";
	}

	public interface PhonesColumns {
		public static final java.lang.String ISPRIMARY = "isprimary";
		public static final java.lang.String LABEL = "label";
		public static final java.lang.String NUMBER = "number";
		public static final java.lang.String NUMBER_KEY = "number_key";
		public static final java.lang.String TYPE = "type";
		public static final int TYPE_CUSTOM = 0;
		public static final int TYPE_FAX_HOME = 5;
		public static final int TYPE_FAX_WORK = 4;
		public static final int TYPE_HOME = 1;
		public static final int TYPE_MOBILE = 2;
		public static final int TYPE_OTHER = 7;
		public static final int TYPE_PAGER = 6;
		public static final int TYPE_WORK = 3;
	}

	public interface PresenceColumns {
		public static final int AVAILABLE = 5;
		public static final int AWAY = 2;
		public static final int DO_NOT_DISTURB = 4;
		public static final int IDLE = 3;
		public static final java.lang.String IM_ACCOUNT = "im_account";
		public static final java.lang.String IM_HANDLE = "im_handle";
		public static final java.lang.String IM_PROTOCOL = "im_protocol";
		public static final int INVISIBLE = 1;
		public static final int OFFLINE = 0;
		public static final java.lang.String PRESENCE_CUSTOM_STATUS = "status";
		public static final java.lang.String PRESENCE_STATUS = "mode";
		public static final java.lang.String PRIORITY = "priority";
	}
}
