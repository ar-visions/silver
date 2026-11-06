package android.provider;

public class Telephony {

	public static class Sms implements android.provider.BaseColumns, android.provider.Telephony.TextBasedSmsColumns {
	// GEN member:android.provider.Telephony$Sms:public static java.lang.String getDefaultSmsPackage(android.content.Context);
	public static java.lang.String getDefaultSmsPackage(android.content.Context arg0) { return null; }
}

	public interface TextBasedSmsColumns {
		public static final java.lang.String ADDRESS = "address";
		public static final java.lang.String BODY = "body";
		public static final java.lang.String CREATOR = "creator";
		public static final java.lang.String DATE = "date";
		public static final java.lang.String DATE_SENT = "date_sent";
		public static final java.lang.String ERROR_CODE = "error_code";
		public static final java.lang.String LOCKED = "locked";
		public static final int MESSAGE_TYPE_ALL = 0;
		public static final int MESSAGE_TYPE_DRAFT = 3;
		public static final int MESSAGE_TYPE_FAILED = 5;
		public static final int MESSAGE_TYPE_INBOX = 1;
		public static final int MESSAGE_TYPE_OUTBOX = 4;
		public static final int MESSAGE_TYPE_QUEUED = 6;
		public static final int MESSAGE_TYPE_SENT = 2;
		public static final java.lang.String PERSON = "person";
		public static final java.lang.String PROTOCOL = "protocol";
		public static final java.lang.String READ = "read";
		public static final java.lang.String REPLY_PATH_PRESENT = "reply_path_present";
		public static final java.lang.String SEEN = "seen";
		public static final java.lang.String SERVICE_CENTER = "service_center";
		public static final java.lang.String STATUS = "status";
		public static final int STATUS_COMPLETE = 0;
		public static final int STATUS_FAILED = 64;
		public static final int STATUS_NONE = -1;
		public static final int STATUS_PENDING = 32;
		public static final java.lang.String SUBJECT = "subject";
		public static final java.lang.String SUBSCRIPTION_ID = "sub_id";
		public static final java.lang.String THREAD_ID = "thread_id";
		public static final java.lang.String TYPE = "type";
	}
}
