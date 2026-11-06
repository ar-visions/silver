package android.view.accessibility;

// android.jar's shape, made by gen.py
public final class AccessibilityEvent extends android.view.accessibility.AccessibilityRecord implements android.os.Parcelable {
	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION = 4;
	public static final int CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION = 4;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_CONTENT_INVALID = 1024;
	public static final int CONTENT_CHANGE_TYPE_CONTENT_INVALID = 1024;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_DRAG_CANCELLED = 512;
	public static final int CONTENT_CHANGE_TYPE_DRAG_CANCELLED = 512;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_DRAG_DROPPED = 256;
	public static final int CONTENT_CHANGE_TYPE_DRAG_DROPPED = 256;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_DRAG_STARTED = 128;
	public static final int CONTENT_CHANGE_TYPE_DRAG_STARTED = 128;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_ENABLED = 4096;
	public static final int CONTENT_CHANGE_TYPE_ENABLED = 4096;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_ERROR = 2048;
	public static final int CONTENT_CHANGE_TYPE_ERROR = 2048;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_PANE_APPEARED = 16;
	public static final int CONTENT_CHANGE_TYPE_PANE_APPEARED = 16;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_PANE_DISAPPEARED = 32;
	public static final int CONTENT_CHANGE_TYPE_PANE_DISAPPEARED = 32;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_PANE_TITLE = 8;
	public static final int CONTENT_CHANGE_TYPE_PANE_TITLE = 8;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_STATE_DESCRIPTION = 64;
	public static final int CONTENT_CHANGE_TYPE_STATE_DESCRIPTION = 64;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_SUBTREE = 1;
	public static final int CONTENT_CHANGE_TYPE_SUBTREE = 1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_TEXT = 2;
	public static final int CONTENT_CHANGE_TYPE_TEXT = 2;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int CONTENT_CHANGE_TYPE_UNDEFINED = 0;
	public static final int CONTENT_CHANGE_TYPE_UNDEFINED = 0;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final android.os.Parcelable$Creator<android.view.accessibility.AccessibilityEvent> CREATOR;
	public static final android.os.Parcelable.Creator<android.view.accessibility.AccessibilityEvent> CREATOR = null;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int INVALID_POSITION = -1;
	public static final int INVALID_POSITION = -1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int MAX_TEXT_LENGTH = 500;
	public static final int MAX_TEXT_LENGTH = 500;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int SPEECH_STATE_LISTENING_END = 8;
	public static final int SPEECH_STATE_LISTENING_END = 8;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int SPEECH_STATE_LISTENING_START = 4;
	public static final int SPEECH_STATE_LISTENING_START = 4;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int SPEECH_STATE_SPEAKING_END = 2;
	public static final int SPEECH_STATE_SPEAKING_END = 2;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int SPEECH_STATE_SPEAKING_START = 1;
	public static final int SPEECH_STATE_SPEAKING_START = 1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPES_ALL_MASK = -1;
	public static final int TYPES_ALL_MASK = -1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_ANNOUNCEMENT = 16384;
	public static final int TYPE_ANNOUNCEMENT = 16384;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_ASSIST_READING_CONTEXT = 16777216;
	public static final int TYPE_ASSIST_READING_CONTEXT = 16777216;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_GESTURE_DETECTION_END = 524288;
	public static final int TYPE_GESTURE_DETECTION_END = 524288;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_GESTURE_DETECTION_START = 262144;
	public static final int TYPE_GESTURE_DETECTION_START = 262144;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_NOTIFICATION_STATE_CHANGED = 64;
	public static final int TYPE_NOTIFICATION_STATE_CHANGED = 64;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_SPEECH_STATE_CHANGE = 33554432;
	public static final int TYPE_SPEECH_STATE_CHANGE = 33554432;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_TOUCH_EXPLORATION_GESTURE_END = 1024;
	public static final int TYPE_TOUCH_EXPLORATION_GESTURE_END = 1024;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_TOUCH_EXPLORATION_GESTURE_START = 512;
	public static final int TYPE_TOUCH_EXPLORATION_GESTURE_START = 512;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_TOUCH_INTERACTION_END = 2097152;
	public static final int TYPE_TOUCH_INTERACTION_END = 2097152;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_TOUCH_INTERACTION_START = 1048576;
	public static final int TYPE_TOUCH_INTERACTION_START = 1048576;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_ACCESSIBILITY_FOCUSED = 32768;
	public static final int TYPE_VIEW_ACCESSIBILITY_FOCUSED = 32768;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 65536;
	public static final int TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 65536;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_CLICKED = 1;
	public static final int TYPE_VIEW_CLICKED = 1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_CONTEXT_CLICKED = 8388608;
	public static final int TYPE_VIEW_CONTEXT_CLICKED = 8388608;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_FOCUSED = 8;
	public static final int TYPE_VIEW_FOCUSED = 8;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_HOVER_ENTER = 128;
	public static final int TYPE_VIEW_HOVER_ENTER = 128;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_HOVER_EXIT = 256;
	public static final int TYPE_VIEW_HOVER_EXIT = 256;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_LONG_CLICKED = 2;
	public static final int TYPE_VIEW_LONG_CLICKED = 2;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_SCROLLED = 4096;
	public static final int TYPE_VIEW_SCROLLED = 4096;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_SELECTED = 4;
	public static final int TYPE_VIEW_SELECTED = 4;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_TARGETED_BY_SCROLL = 67108864;
	public static final int TYPE_VIEW_TARGETED_BY_SCROLL = 67108864;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_TEXT_CHANGED = 16;
	public static final int TYPE_VIEW_TEXT_CHANGED = 16;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_TEXT_SELECTION_CHANGED = 8192;
	public static final int TYPE_VIEW_TEXT_SELECTION_CHANGED = 8192;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 131072;
	public static final int TYPE_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY = 131072;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_WINDOWS_CHANGED = 4194304;
	public static final int TYPE_WINDOWS_CHANGED = 4194304;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_WINDOW_CONTENT_CHANGED = 2048;
	public static final int TYPE_WINDOW_CONTENT_CHANGED = 2048;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int TYPE_WINDOW_STATE_CHANGED = 32;
	public static final int TYPE_WINDOW_STATE_CHANGED = 32;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_ACCESSIBILITY_FOCUSED = 128;
	public static final int WINDOWS_CHANGE_ACCESSIBILITY_FOCUSED = 128;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_ACTIVE = 32;
	public static final int WINDOWS_CHANGE_ACTIVE = 32;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_ADDED = 1;
	public static final int WINDOWS_CHANGE_ADDED = 1;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_BOUNDS = 8;
	public static final int WINDOWS_CHANGE_BOUNDS = 8;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_CHILDREN = 512;
	public static final int WINDOWS_CHANGE_CHILDREN = 512;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_FOCUSED = 64;
	public static final int WINDOWS_CHANGE_FOCUSED = 64;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_LAYER = 16;
	public static final int WINDOWS_CHANGE_LAYER = 16;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_PARENT = 256;
	public static final int WINDOWS_CHANGE_PARENT = 256;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_PIP = 1024;
	public static final int WINDOWS_CHANGE_PIP = 1024;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_REMOVED = 2;
	public static final int WINDOWS_CHANGE_REMOVED = 2;

	// GEN member:android.view.accessibility.AccessibilityEvent:public static final int WINDOWS_CHANGE_TITLE = 4;
	public static final int WINDOWS_CHANGE_TITLE = 4;

	// GEN member:android.view.accessibility.AccessibilityEvent:public android.view.accessibility.AccessibilityEvent();
	public AccessibilityEvent() {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public android.view.accessibility.AccessibilityEvent(int);
	public AccessibilityEvent(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public android.view.accessibility.AccessibilityEvent(android.view.accessibility.AccessibilityEvent);
	public AccessibilityEvent(android.view.accessibility.AccessibilityEvent arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getRecordCount();
	public int getRecordCount() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void appendRecord(android.view.accessibility.AccessibilityRecord);
	public void appendRecord(android.view.accessibility.AccessibilityRecord arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public android.view.accessibility.AccessibilityRecord getRecord(int);
	public android.view.accessibility.AccessibilityRecord getRecord(int arg0) { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getEventType();
	public int getEventType() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getContentChangeTypes();
	public int getContentChangeTypes() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setContentChangeTypes(int);
	public void setContentChangeTypes(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public boolean isAccessibilityDataSensitive();
	public boolean isAccessibilityDataSensitive() { return false; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setAccessibilityDataSensitive(boolean);
	public void setAccessibilityDataSensitive(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getSpeechStateChangeTypes();
	public int getSpeechStateChangeTypes() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setSpeechStateChangeTypes(int);
	public void setSpeechStateChangeTypes(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getWindowChanges();
	public int getWindowChanges() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setEventType(int);
	public void setEventType(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public long getEventTime();
	public long getEventTime() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setEventTime(long);
	public void setEventTime(long arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public java.lang.CharSequence getPackageName();
	public java.lang.CharSequence getPackageName() { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setPackageName(java.lang.CharSequence);
	public void setPackageName(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setMovementGranularity(int);
	public void setMovementGranularity(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getMovementGranularity();
	public int getMovementGranularity() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void setAction(int);
	public void setAction(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int getAction();
	public int getAction() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public static android.view.accessibility.AccessibilityEvent obtain(int);
	public static android.view.accessibility.AccessibilityEvent obtain(int arg0) { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public static android.view.accessibility.AccessibilityEvent obtain(android.view.accessibility.AccessibilityEvent);
	public static android.view.accessibility.AccessibilityEvent obtain(android.view.accessibility.AccessibilityEvent arg0) { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public static android.view.accessibility.AccessibilityEvent obtain();
	public static android.view.accessibility.AccessibilityEvent obtain() { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public void recycle();
	public void recycle() {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public void initFromParcel(android.os.Parcel);
	public void initFromParcel(android.os.Parcel arg0) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityEvent:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.view.accessibility.AccessibilityEvent:public static java.lang.String eventTypeToString(int);
	public static java.lang.String eventTypeToString(int arg0) { return null; }
}
