package android.provider;

import android.net.Uri;

public class ContactsContract {

	public static final Uri AUTHORITY_URI = Uri.parse("content://com.android.contacts");

	public static final class CommonDataKinds {

		public static final class Phone implements android.provider.ContactsContract.CommonDataKinds.CommonColumns, android.provider.ContactsContract.DataColumnsWithJoins {
			public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/phones");
		
	// GEN member:android.provider.ContactsContract$CommonDataKinds$Phone:public static java.lang.CharSequence getTypeLabel(android.content.res.Resources, int, java.lang.CharSequence);
	public static java.lang.CharSequence getTypeLabel(android.content.res.Resources arg0, int arg1, java.lang.CharSequence arg2) { return null; }

	// GEN member:android.provider.ContactsContract$CommonDataKinds$Phone:public static final android.net.Uri CONTENT_FILTER_URI;
	public static final android.net.Uri CONTENT_FILTER_URI = null;
}

		public static final class Email implements android.provider.ContactsContract.CommonDataKinds.CommonColumns, android.provider.ContactsContract.DataColumnsWithJoins {
			public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/emails");
		
	// GEN member:android.provider.ContactsContract$CommonDataKinds$Email:public static java.lang.CharSequence getTypeLabel(android.content.res.Resources, int, java.lang.CharSequence);
	public static java.lang.CharSequence getTypeLabel(android.content.res.Resources arg0, int arg1, java.lang.CharSequence arg2) { return null; }

	// GEN member:android.provider.ContactsContract$CommonDataKinds$Email:public static final android.net.Uri CONTENT_FILTER_URI;
	public static final android.net.Uri CONTENT_FILTER_URI = null;
}
	
		public interface CommonColumns extends android.provider.ContactsContract.CommonDataKinds.BaseTypes {
			public static final java.lang.String DATA = "data1";
			public static final java.lang.String LABEL = "data3";
			public static final java.lang.String TYPE = "data2";
		}
	
		public interface BaseTypes {
			public static final int TYPE_CUSTOM = 0;
		}
	}

	public static final class Profile implements android.provider.BaseColumns, android.provider.ContactsContract.ContactNameColumns, android.provider.ContactsContract.ContactOptionsColumns, android.provider.ContactsContract.ContactStatusColumns, android.provider.ContactsContract.ContactsColumns {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/profile");
	}

	public static final class Contacts implements android.provider.BaseColumns, android.provider.ContactsContract.ContactNameColumns, android.provider.ContactsContract.ContactOptionsColumns, android.provider.ContactsContract.ContactStatusColumns, android.provider.ContactsContract.ContactsColumns {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/contacts");
	
	// GEN member:android.provider.ContactsContract$Contacts:public static android.net.Uri lookupContact(android.content.ContentResolver, android.net.Uri);
	public static android.net.Uri lookupContact(android.content.ContentResolver arg0, android.net.Uri arg1) { return null; }

	// GEN member:android.provider.ContactsContract$Contacts:public static java.io.InputStream openContactPhotoInputStream(android.content.ContentResolver, android.net.Uri, boolean);
	public static java.io.InputStream openContactPhotoInputStream(android.content.ContentResolver arg0, android.net.Uri arg1, boolean arg2) { return null; }

	// GEN member:android.provider.ContactsContract$Contacts:public static android.net.Uri getLookupUri(long, java.lang.String);
	public static android.net.Uri getLookupUri(long arg0, java.lang.String arg1) { return null; }
}

	public static final class RawContacts implements android.provider.BaseColumns, android.provider.ContactsContract.ContactNameColumns, android.provider.ContactsContract.ContactOptionsColumns, android.provider.ContactsContract.RawContactsColumns, android.provider.ContactsContract.SyncColumns {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/raw_contacts");
	}

	public static final class Data implements android.provider.ContactsContract.DataColumnsWithJoins {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.contacts/data");
	}

	public interface ContactNameColumns {
		public static final java.lang.String DISPLAY_NAME_ALTERNATIVE = "display_name_alt";
		public static final java.lang.String DISPLAY_NAME_PRIMARY = "display_name";
		public static final java.lang.String DISPLAY_NAME_SOURCE = "display_name_source";
		public static final java.lang.String PHONETIC_NAME = "phonetic_name";
		public static final java.lang.String PHONETIC_NAME_STYLE = "phonetic_name_style";
		public static final java.lang.String SORT_KEY_ALTERNATIVE = "sort_key_alt";
		public static final java.lang.String SORT_KEY_PRIMARY = "sort_key";
	}

	public interface ContactOptionsColumns {
		public static final java.lang.String CUSTOM_RINGTONE = "custom_ringtone";
		public static final java.lang.String LAST_TIME_CONTACTED = "last_time_contacted";
		public static final java.lang.String PINNED = "pinned";
		public static final java.lang.String SEND_TO_VOICEMAIL = "send_to_voicemail";
		public static final java.lang.String STARRED = "starred";
		public static final java.lang.String TIMES_CONTACTED = "times_contacted";
	}

	public interface ContactStatusColumns {
		public static final java.lang.String CONTACT_CHAT_CAPABILITY = "contact_chat_capability";
		public static final java.lang.String CONTACT_PRESENCE = "contact_presence";
		public static final java.lang.String CONTACT_STATUS = "contact_status";
		public static final java.lang.String CONTACT_STATUS_ICON = "contact_status_icon";
		public static final java.lang.String CONTACT_STATUS_LABEL = "contact_status_label";
		public static final java.lang.String CONTACT_STATUS_RES_PACKAGE = "contact_status_res_package";
		public static final java.lang.String CONTACT_STATUS_TIMESTAMP = "contact_status_ts";
	}

	public interface ContactsColumns {
		public static final java.lang.String CONTACT_LAST_UPDATED_TIMESTAMP = "contact_last_updated_timestamp";
		public static final java.lang.String DISPLAY_NAME = "display_name";
		public static final java.lang.String HAS_PHONE_NUMBER = "has_phone_number";
		public static final java.lang.String IN_DEFAULT_DIRECTORY = "in_default_directory";
		public static final java.lang.String IN_VISIBLE_GROUP = "in_visible_group";
		public static final java.lang.String IS_USER_PROFILE = "is_user_profile";
		public static final java.lang.String LOOKUP_KEY = "lookup";
		public static final java.lang.String NAME_RAW_CONTACT_ID = "name_raw_contact_id";
		public static final java.lang.String PHOTO_FILE_ID = "photo_file_id";
		public static final java.lang.String PHOTO_ID = "photo_id";
		public static final java.lang.String PHOTO_THUMBNAIL_URI = "photo_thumb_uri";
		public static final java.lang.String PHOTO_URI = "photo_uri";
	}

	public interface DataColumnsWithJoins extends android.provider.BaseColumns, android.provider.ContactsContract.RawContactsColumns, android.provider.ContactsContract.ContactsColumns, android.provider.ContactsContract.ContactNameColumns, android.provider.ContactsContract.ContactOptionsColumns, android.provider.ContactsContract.ContactStatusColumns, android.provider.ContactsContract.DataColumns, android.provider.ContactsContract.DataUsageStatColumns, android.provider.ContactsContract.StatusColumns {
	}

	public interface RawContactsColumns {
		public static final java.lang.String ACCOUNT_TYPE_AND_DATA_SET = "account_type_and_data_set";
		public static final java.lang.String AGGREGATION_MODE = "aggregation_mode";
		public static final java.lang.String BACKUP_ID = "backup_id";
		public static final java.lang.String CONTACT_ID = "contact_id";
		public static final java.lang.String DATA_SET = "data_set";
		public static final java.lang.String DELETED = "deleted";
		public static final java.lang.String METADATA_DIRTY = "metadata_dirty";
		public static final java.lang.String RAW_CONTACT_IS_READ_ONLY = "raw_contact_is_read_only";
		public static final java.lang.String RAW_CONTACT_IS_USER_PROFILE = "raw_contact_is_user_profile";
	}

	public interface SyncColumns extends android.provider.ContactsContract.BaseSyncColumns {
		public static final java.lang.String ACCOUNT_NAME = "account_name";
		public static final java.lang.String ACCOUNT_TYPE = "account_type";
		public static final java.lang.String DIRTY = "dirty";
		public static final java.lang.String SOURCE_ID = "sourceid";
		public static final java.lang.String VERSION = "version";
	}

	public interface BaseSyncColumns {
		public static final java.lang.String SYNC1 = "sync1";
		public static final java.lang.String SYNC2 = "sync2";
		public static final java.lang.String SYNC3 = "sync3";
		public static final java.lang.String SYNC4 = "sync4";
	}

	public interface DataColumns {
		public static final java.lang.String CARRIER_PRESENCE = "carrier_presence";
		public static final int CARRIER_PRESENCE_VT_CAPABLE = 1;
		public static final java.lang.String DATA1 = "data1";
		public static final java.lang.String DATA10 = "data10";
		public static final java.lang.String DATA11 = "data11";
		public static final java.lang.String DATA12 = "data12";
		public static final java.lang.String DATA13 = "data13";
		public static final java.lang.String DATA14 = "data14";
		public static final java.lang.String DATA15 = "data15";
		public static final java.lang.String DATA2 = "data2";
		public static final java.lang.String DATA3 = "data3";
		public static final java.lang.String DATA4 = "data4";
		public static final java.lang.String DATA5 = "data5";
		public static final java.lang.String DATA6 = "data6";
		public static final java.lang.String DATA7 = "data7";
		public static final java.lang.String DATA8 = "data8";
		public static final java.lang.String DATA9 = "data9";
		public static final java.lang.String DATA_VERSION = "data_version";
		public static final java.lang.String IS_PRIMARY = "is_primary";
		public static final java.lang.String IS_READ_ONLY = "is_read_only";
		public static final java.lang.String IS_SUPER_PRIMARY = "is_super_primary";
		public static final java.lang.String MIMETYPE = "mimetype";
		public static final java.lang.String PREFERRED_PHONE_ACCOUNT_COMPONENT_NAME = "preferred_phone_account_component_name";
		public static final java.lang.String PREFERRED_PHONE_ACCOUNT_ID = "preferred_phone_account_id";
		public static final java.lang.String RAW_CONTACT_ID = "raw_contact_id";
		public static final java.lang.String RES_PACKAGE = "res_package";
		public static final java.lang.String SYNC1 = "data_sync1";
		public static final java.lang.String SYNC2 = "data_sync2";
		public static final java.lang.String SYNC3 = "data_sync3";
		public static final java.lang.String SYNC4 = "data_sync4";
	}

	public interface DataUsageStatColumns {
		public static final java.lang.String LAST_TIME_USED = "last_time_used";
		public static final java.lang.String TIMES_USED = "times_used";
	}

	public interface StatusColumns {
		public static final int AVAILABLE = 5;
		public static final int AWAY = 2;
		public static final int CAPABILITY_HAS_CAMERA = 4;
		public static final int CAPABILITY_HAS_VIDEO = 2;
		public static final int CAPABILITY_HAS_VOICE = 1;
		public static final java.lang.String CHAT_CAPABILITY = "chat_capability";
		public static final int DO_NOT_DISTURB = 4;
		public static final int IDLE = 3;
		public static final int INVISIBLE = 1;
		public static final int OFFLINE = 0;
		public static final java.lang.String PRESENCE = "mode";
		public static final java.lang.String PRESENCE_CUSTOM_STATUS = "status";
		public static final java.lang.String PRESENCE_STATUS = "mode";
		public static final java.lang.String STATUS = "status";
		public static final java.lang.String STATUS_ICON = "status_icon";
		public static final java.lang.String STATUS_LABEL = "status_label";
		public static final java.lang.String STATUS_RES_PACKAGE = "status_res_package";
		public static final java.lang.String STATUS_TIMESTAMP = "status_ts";
	}
}
