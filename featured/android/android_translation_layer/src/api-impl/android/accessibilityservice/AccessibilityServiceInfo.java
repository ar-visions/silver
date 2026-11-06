package android.accessibilityservice;

// android.jar's shape, made by gen.py
public class AccessibilityServiceInfo implements android.os.Parcelable {
	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_CONTROL_MAGNIFICATION = 16;
	public static final int CAPABILITY_CAN_CONTROL_MAGNIFICATION = 16;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_PERFORM_GESTURES = 32;
	public static final int CAPABILITY_CAN_PERFORM_GESTURES = 32;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 4;
	public static final int CAPABILITY_CAN_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 4;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS = 8;
	public static final int CAPABILITY_CAN_REQUEST_FILTER_KEY_EVENTS = 8;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_REQUEST_FINGERPRINT_GESTURES = 64;
	public static final int CAPABILITY_CAN_REQUEST_FINGERPRINT_GESTURES = 64;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION = 2;
	public static final int CAPABILITY_CAN_REQUEST_TOUCH_EXPLORATION = 2;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 1;
	public static final int CAPABILITY_CAN_RETRIEVE_WINDOW_CONTENT = 1;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int CAPABILITY_CAN_TAKE_SCREENSHOT = 128;
	public static final int CAPABILITY_CAN_TAKE_SCREENSHOT = 128;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final android.os.Parcelable$Creator<android.accessibilityservice.AccessibilityServiceInfo> CREATOR;
	public static final android.os.Parcelable.Creator<android.accessibilityservice.AccessibilityServiceInfo> CREATOR = null;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int DEFAULT = 1;
	public static final int DEFAULT = 1;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_ALL_MASK = -1;
	public static final int FEEDBACK_ALL_MASK = -1;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_AUDIBLE = 4;
	public static final int FEEDBACK_AUDIBLE = 4;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_BRAILLE = 32;
	public static final int FEEDBACK_BRAILLE = 32;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_GENERIC = 16;
	public static final int FEEDBACK_GENERIC = 16;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_HAPTIC = 2;
	public static final int FEEDBACK_HAPTIC = 2;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_SPOKEN = 1;
	public static final int FEEDBACK_SPOKEN = 1;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FEEDBACK_VISUAL = 8;
	public static final int FEEDBACK_VISUAL = 8;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_ENABLE_ACCESSIBILITY_VOLUME = 128;
	public static final int FLAG_ENABLE_ACCESSIBILITY_VOLUME = 128;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 2;
	public static final int FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 2;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_INPUT_METHOD_EDITOR = 32768;
	public static final int FLAG_INPUT_METHOD_EDITOR = 32768;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REPORT_VIEW_IDS = 16;
	public static final int FLAG_REPORT_VIEW_IDS = 16;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_2_FINGER_PASSTHROUGH = 8192;
	public static final int FLAG_REQUEST_2_FINGER_PASSTHROUGH = 8192;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_ACCESSIBILITY_BUTTON = 256;
	public static final int FLAG_REQUEST_ACCESSIBILITY_BUTTON = 256;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 8;
	public static final int FLAG_REQUEST_ENHANCED_WEB_ACCESSIBILITY = 8;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_FILTER_KEY_EVENTS = 32;
	public static final int FLAG_REQUEST_FILTER_KEY_EVENTS = 32;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_FINGERPRINT_GESTURES = 512;
	public static final int FLAG_REQUEST_FINGERPRINT_GESTURES = 512;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_MULTI_FINGER_GESTURES = 4096;
	public static final int FLAG_REQUEST_MULTI_FINGER_GESTURES = 4096;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK = 1024;
	public static final int FLAG_REQUEST_SHORTCUT_WARNING_DIALOG_SPOKEN_FEEDBACK = 1024;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 4;
	public static final int FLAG_REQUEST_TOUCH_EXPLORATION_MODE = 4;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 64;
	public static final int FLAG_RETRIEVE_INTERACTIVE_WINDOWS = 64;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_SEND_MOTION_EVENTS = 16384;
	public static final int FLAG_SEND_MOTION_EVENTS = 16384;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static final int FLAG_SERVICE_HANDLES_DOUBLE_TAP = 2048;
	public static final int FLAG_SERVICE_HANDLES_DOUBLE_TAP = 2048;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int eventTypes;
	public int eventTypes;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int feedbackType;
	public int feedbackType;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int flags;
	public int flags;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public long notificationTimeout;
	public long notificationTimeout;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String[] packageNames;
	public java.lang.String[] packageNames;

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public android.content.pm.ResolveInfo getResolveInfo();
	public android.content.pm.ResolveInfo getResolveInfo() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String getSettingsActivityName();
	public java.lang.String getSettingsActivityName() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String getTileServiceName();
	public java.lang.String getTileServiceName() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public boolean getCanRetrieveWindowContent();
	public boolean getCanRetrieveWindowContent() { return false; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int getCapabilities();
	public int getCapabilities() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int getMotionEventSources();
	public int getMotionEventSources() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public void setMotionEventSources(int);
	public void setMotionEventSources(int arg0) {}

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.CharSequence loadSummary(android.content.pm.PackageManager);
	public java.lang.CharSequence loadSummary(android.content.pm.PackageManager arg0) { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.CharSequence loadIntro(android.content.pm.PackageManager);
	public java.lang.CharSequence loadIntro(android.content.pm.PackageManager arg0) { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String getDescription();
	public java.lang.String getDescription() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String loadDescription(android.content.pm.PackageManager);
	public java.lang.String loadDescription(android.content.pm.PackageManager arg0) { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public void setNonInteractiveUiTimeoutMillis(int);
	public void setNonInteractiveUiTimeoutMillis(int arg0) {}

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int getNonInteractiveUiTimeoutMillis();
	public int getNonInteractiveUiTimeoutMillis() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public void setInteractiveUiTimeoutMillis(int);
	public void setInteractiveUiTimeoutMillis(int arg0) {}

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int getInteractiveUiTimeoutMillis();
	public int getInteractiveUiTimeoutMillis() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public boolean isAccessibilityTool();
	public boolean isAccessibilityTool() { return false; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static java.lang.String feedbackTypeToString(int);
	public static java.lang.String feedbackTypeToString(int arg0) { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static java.lang.String flagToString(int);
	public static java.lang.String flagToString(int arg0) { return null; }

	// GEN member:android.accessibilityservice.AccessibilityServiceInfo:public static java.lang.String capabilityToString(int);
	public static java.lang.String capabilityToString(int arg0) { return null; }

	protected AccessibilityServiceInfo() {}
}
