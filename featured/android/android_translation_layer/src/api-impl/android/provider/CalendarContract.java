package android.provider;

import android.net.Uri;

public class CalendarContract {

	public static final class Events implements android.provider.BaseColumns, android.provider.CalendarContract.CalendarColumns, android.provider.CalendarContract.EventsColumns, android.provider.CalendarContract.SyncColumns {
		public static final Uri CONTENT_URI = Uri.parse("content://com.android.calendar/events");
	}

	public interface CalendarColumns {
		public static final java.lang.String ALLOWED_ATTENDEE_TYPES = "allowedAttendeeTypes";
		public static final java.lang.String ALLOWED_AVAILABILITY = "allowedAvailability";
		public static final java.lang.String ALLOWED_REMINDERS = "allowedReminders";
		public static final java.lang.String CALENDAR_ACCESS_LEVEL = "calendar_access_level";
		public static final java.lang.String CALENDAR_COLOR = "calendar_color";
		public static final java.lang.String CALENDAR_COLOR_KEY = "calendar_color_index";
		public static final java.lang.String CALENDAR_DISPLAY_NAME = "calendar_displayName";
		public static final java.lang.String CALENDAR_TIME_ZONE = "calendar_timezone";
		public static final int CAL_ACCESS_CONTRIBUTOR = 500;
		public static final int CAL_ACCESS_EDITOR = 600;
		public static final int CAL_ACCESS_FREEBUSY = 100;
		public static final int CAL_ACCESS_NONE = 0;
		public static final int CAL_ACCESS_OVERRIDE = 400;
		public static final int CAL_ACCESS_OWNER = 700;
		public static final int CAL_ACCESS_READ = 200;
		public static final int CAL_ACCESS_RESPOND = 300;
		public static final int CAL_ACCESS_ROOT = 800;
		public static final java.lang.String CAN_MODIFY_TIME_ZONE = "canModifyTimeZone";
		public static final java.lang.String CAN_ORGANIZER_RESPOND = "canOrganizerRespond";
		public static final java.lang.String IS_PRIMARY = "isPrimary";
		public static final java.lang.String MAX_REMINDERS = "maxReminders";
		public static final java.lang.String OWNER_ACCOUNT = "ownerAccount";
		public static final java.lang.String SYNC_EVENTS = "sync_events";
		public static final java.lang.String VISIBLE = "visible";
	}

	public interface EventsColumns {
		public static final int ACCESS_CONFIDENTIAL = 1;
		public static final int ACCESS_DEFAULT = 0;
		public static final java.lang.String ACCESS_LEVEL = "accessLevel";
		public static final int ACCESS_PRIVATE = 2;
		public static final int ACCESS_PUBLIC = 3;
		public static final java.lang.String ALL_DAY = "allDay";
		public static final java.lang.String AVAILABILITY = "availability";
		public static final int AVAILABILITY_BUSY = 0;
		public static final int AVAILABILITY_FREE = 1;
		public static final int AVAILABILITY_TENTATIVE = 2;
		public static final java.lang.String CALENDAR_ID = "calendar_id";
		public static final java.lang.String CAN_INVITE_OTHERS = "canInviteOthers";
		public static final java.lang.String CUSTOM_APP_PACKAGE = "customAppPackage";
		public static final java.lang.String CUSTOM_APP_URI = "customAppUri";
		public static final java.lang.String DESCRIPTION = "description";
		public static final java.lang.String DISPLAY_COLOR = "displayColor";
		public static final java.lang.String DTEND = "dtend";
		public static final java.lang.String DTSTART = "dtstart";
		public static final java.lang.String DURATION = "duration";
		public static final java.lang.String EVENT_COLOR = "eventColor";
		public static final java.lang.String EVENT_COLOR_KEY = "eventColor_index";
		public static final java.lang.String EVENT_END_TIMEZONE = "eventEndTimezone";
		public static final java.lang.String EVENT_LOCATION = "eventLocation";
		public static final java.lang.String EVENT_TIMEZONE = "eventTimezone";
		public static final java.lang.String EXDATE = "exdate";
		public static final java.lang.String EXRULE = "exrule";
		public static final java.lang.String GUESTS_CAN_INVITE_OTHERS = "guestsCanInviteOthers";
		public static final java.lang.String GUESTS_CAN_MODIFY = "guestsCanModify";
		public static final java.lang.String GUESTS_CAN_SEE_GUESTS = "guestsCanSeeGuests";
		public static final java.lang.String HAS_ALARM = "hasAlarm";
		public static final java.lang.String HAS_ATTENDEE_DATA = "hasAttendeeData";
		public static final java.lang.String HAS_EXTENDED_PROPERTIES = "hasExtendedProperties";
		public static final java.lang.String IS_ORGANIZER = "isOrganizer";
		public static final java.lang.String LAST_DATE = "lastDate";
		public static final java.lang.String LAST_SYNCED = "lastSynced";
		public static final java.lang.String ORGANIZER = "organizer";
		public static final java.lang.String ORIGINAL_ALL_DAY = "originalAllDay";
		public static final java.lang.String ORIGINAL_ID = "original_id";
		public static final java.lang.String ORIGINAL_INSTANCE_TIME = "originalInstanceTime";
		public static final java.lang.String ORIGINAL_SYNC_ID = "original_sync_id";
		public static final java.lang.String RDATE = "rdate";
		public static final java.lang.String RRULE = "rrule";
		public static final java.lang.String SELF_ATTENDEE_STATUS = "selfAttendeeStatus";
		public static final java.lang.String STATUS = "eventStatus";
		public static final int STATUS_CANCELED = 2;
		public static final int STATUS_CONFIRMED = 1;
		public static final int STATUS_TENTATIVE = 0;
		public static final java.lang.String SYNC_DATA1 = "sync_data1";
		public static final java.lang.String SYNC_DATA10 = "sync_data10";
		public static final java.lang.String SYNC_DATA2 = "sync_data2";
		public static final java.lang.String SYNC_DATA3 = "sync_data3";
		public static final java.lang.String SYNC_DATA4 = "sync_data4";
		public static final java.lang.String SYNC_DATA5 = "sync_data5";
		public static final java.lang.String SYNC_DATA6 = "sync_data6";
		public static final java.lang.String SYNC_DATA7 = "sync_data7";
		public static final java.lang.String SYNC_DATA8 = "sync_data8";
		public static final java.lang.String SYNC_DATA9 = "sync_data9";
		public static final java.lang.String TITLE = "title";
		public static final java.lang.String UID_2445 = "uid2445";
	}

	public interface SyncColumns extends android.provider.CalendarContract.CalendarSyncColumns {
		public static final java.lang.String ACCOUNT_NAME = "account_name";
		public static final java.lang.String ACCOUNT_TYPE = "account_type";
		public static final java.lang.String CAN_PARTIALLY_UPDATE = "canPartiallyUpdate";
		public static final java.lang.String DELETED = "deleted";
		public static final java.lang.String DIRTY = "dirty";
		public static final java.lang.String MUTATORS = "mutators";
		public static final java.lang.String _SYNC_ID = "_sync_id";
	}

	public interface CalendarSyncColumns {
		public static final java.lang.String CAL_SYNC1 = "cal_sync1";
		public static final java.lang.String CAL_SYNC10 = "cal_sync10";
		public static final java.lang.String CAL_SYNC2 = "cal_sync2";
		public static final java.lang.String CAL_SYNC3 = "cal_sync3";
		public static final java.lang.String CAL_SYNC4 = "cal_sync4";
		public static final java.lang.String CAL_SYNC5 = "cal_sync5";
		public static final java.lang.String CAL_SYNC6 = "cal_sync6";
		public static final java.lang.String CAL_SYNC7 = "cal_sync7";
		public static final java.lang.String CAL_SYNC8 = "cal_sync8";
		public static final java.lang.String CAL_SYNC9 = "cal_sync9";
	}
}
