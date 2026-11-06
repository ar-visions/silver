package android.view.accessibility;

import java.lang.CharSequence;

public class AccessibilityNodeInfo implements android.os.Parcelable {
	public static final class AccessibilityAction implements android.os.Parcelable {
		public static final AccessibilityAction ACTION_CONTEXT_CLICK = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SET_PROGRESS = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SHOW_ON_SCREEN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_TO_POSITION = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_UP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_LEFT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_RIGHT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SCROLL_DOWN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_UP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_DOWN = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_LEFT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_PAGE_RIGHT = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_MOVE_WINDOW = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_SHOW_TOOLTIP = new AccessibilityAction(0, null);
		public static final AccessibilityAction ACTION_HIDE_TOOLTIP = new AccessibilityAction(0, null);

		public AccessibilityAction(int actionId, CharSequence label) {}

		public int getId() { return 0; }

		public CharSequence getLabel() { return null; }
	
	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_DRAG_CANCEL;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_DRAG_CANCEL = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_DRAG_DROP;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_DRAG_DROP = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_DRAG_START;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_DRAG_START = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_IME_ENTER;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_IME_ENTER = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_LONG_CLICK;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_LONG_CLICK = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_PRESS_AND_HOLD;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_PRESS_AND_HOLD = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_SCROLL_BACKWARD;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_SCROLL_BACKWARD = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_SCROLL_FORWARD;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_SCROLL_FORWARD = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_SCROLL_IN_DIRECTION;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_SCROLL_IN_DIRECTION = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_SHOW_TEXT_SUGGESTIONS;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_SHOW_TEXT_SUGGESTIONS = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_CLICK;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_CLICK = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_COLLAPSE;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_COLLAPSE = null;

	// GEN member:android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction:public static final android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction ACTION_EXPAND;
	public static final android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction ACTION_EXPAND = null;
}

	public class RangeInfo {
		public static final int RANGE_TYPE_INT = 0;
		public static final int RANGE_TYPE_FLOAT = 1;
		public static final int RANGE_TYPE_PERCENT = 2;
	}

	public static AccessibilityNodeInfo obtain() { return null; }

	public void setClassName(CharSequence className) {}

	public void addAction(int action) {}

	public void setCheckable(boolean checkable) {}

	public void setChecked(boolean checked) {}

	public void setClickable(boolean clickable) {}

	public void setContentDescription(CharSequence description) {}

	public void setEditable(boolean editable) {}

	public void setEnabled(boolean enabled) {}

	public void setFocusable(boolean focusable) {}

	public void setFocused(boolean focused) {}

	public void setScrollable(boolean scrollable) {}

	public void setVisibleToUser(boolean visible) {}

	public void setTextSelection(int start, int end) {}

	public void setRangeInfo(RangeInfo info) {}

	// GEN class:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo
	public final class CollectionInfo {
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public static final int SELECTION_MODE_MULTIPLE = 2;
		public static final int SELECTION_MODE_MULTIPLE = 2;
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public static final int SELECTION_MODE_NONE = 0;
		public static final int SELECTION_MODE_NONE = 0;
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public static final int SELECTION_MODE_SINGLE = 1;
		public static final int SELECTION_MODE_SINGLE = 1;
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public static final int UNDEFINED = -1;
		public static final int UNDEFINED = -1;
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public android.view.accessibility.AccessibilityNodeInfo$CollectionInfo(int, int, boolean);
		public CollectionInfo(int arg0, int arg1, boolean arg2) {}
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public android.view.accessibility.AccessibilityNodeInfo$CollectionInfo(int, int, boolean, int);
		public CollectionInfo(int arg0, int arg1, boolean arg2, int arg3) {}
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public int getRowCount();
		public int getRowCount() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public int getColumnCount();
		public int getColumnCount() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public boolean isHierarchical();
		public boolean isHierarchical() { return false; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public int getSelectionMode();
		public int getSelectionMode() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public int getItemCount();
		public int getItemCount() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionInfo:public int getImportantForAccessibilityItemCount();
		public int getImportantForAccessibilityItemCount() { return 0; }
	
		protected CollectionInfo() {}
	}


	// GEN class:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo
	public final class CollectionItemInfo {
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo(int, int, int, int, boolean);
		public CollectionItemInfo(int arg0, int arg1, int arg2, int arg3, boolean arg4) {}
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo(int, int, int, int, boolean, boolean);
		public CollectionItemInfo(int arg0, int arg1, int arg2, int arg3, boolean arg4, boolean arg5) {}
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public int getColumnIndex();
		public int getColumnIndex() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public int getRowIndex();
		public int getRowIndex() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public int getColumnSpan();
		public int getColumnSpan() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public int getRowSpan();
		public int getRowSpan() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public boolean isHeading();
		public boolean isHeading() { return false; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public boolean isSelected();
		public boolean isSelected() { return false; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public java.lang.String getRowTitle();
		public java.lang.String getRowTitle() { return null; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo:public java.lang.String getColumnTitle();
		public java.lang.String getColumnTitle() { return null; }
	
		protected CollectionItemInfo() {}
	}


	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void addAction(android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction);
	public void addAction(android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void addChild(android.view.View, int);
	public void addChild(android.view.View arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void addChild(android.view.View);
	public void addChild(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.util.List<android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction> getActionList();
	public java.util.List<android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction> getActionList() { return new java.util.ArrayList<>(); }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public int getActions();
	public int getActions() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void getBoundsInParent(android.graphics.Rect);
	public void getBoundsInParent(android.graphics.Rect arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void getBoundsInScreen(android.graphics.Rect);
	public void getBoundsInScreen(android.graphics.Rect arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void getBoundsInWindow(android.graphics.Rect);
	public void getBoundsInWindow(android.graphics.Rect arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public int getChildCount();
	public int getChildCount() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getClassName();
	public java.lang.CharSequence getClassName() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getContainerTitle();
	public java.lang.CharSequence getContainerTitle() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getContentDescription();
	public java.lang.CharSequence getContentDescription() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getError();
	public java.lang.CharSequence getError() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public android.os.Bundle getExtras();
	public android.os.Bundle getExtras() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public int getMaxTextLength();
	public int getMaxTextLength() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public int getMovementGranularities();
	public int getMovementGranularities() { return 0; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getPackageName();
	public java.lang.CharSequence getPackageName() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getStateDescription();
	public java.lang.CharSequence getStateDescription() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getText();
	public java.lang.CharSequence getText() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.CharSequence getTooltipText();
	public java.lang.CharSequence getTooltipText() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.String getUniqueId();
	public java.lang.String getUniqueId() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public java.lang.String getViewIdResourceName();
	public java.lang.String getViewIdResourceName() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isAccessibilityDataSensitive();
	public boolean isAccessibilityDataSensitive() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isCheckable();
	public boolean isCheckable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isChecked();
	public boolean isChecked() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isClickable();
	public boolean isClickable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isContextClickable();
	public boolean isContextClickable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isEnabled();
	public boolean isEnabled() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isFocusable();
	public boolean isFocusable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isFocused();
	public boolean isFocused() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isImportantForAccessibility();
	public boolean isImportantForAccessibility() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isLongClickable();
	public boolean isLongClickable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isPassword();
	public boolean isPassword() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isScrollable();
	public boolean isScrollable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isSelected();
	public boolean isSelected() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isShowingHintText();
	public boolean isShowingHintText() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isTextSelectable();
	public boolean isTextSelectable() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isVisibleToUser();
	public boolean isVisibleToUser() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.accessibility.AccessibilityNodeInfo);
	public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.accessibility.AccessibilityNodeInfo arg0) { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.View);
	public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.View arg0) { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean performAction(int, android.os.Bundle);
	public boolean performAction(int arg0, android.os.Bundle arg1) { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean removeAction(android.view.accessibility.AccessibilityNodeInfo$AccessibilityAction);
	public boolean removeAction(android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction arg0) { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setAccessibilityDataSensitive(boolean);
	public void setAccessibilityDataSensitive(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setAccessibilityFocused(boolean);
	public void setAccessibilityFocused(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setAvailableExtraData(java.util.List<java.lang.String>);
	public void setAvailableExtraData(java.util.List<java.lang.String> arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setBoundsInParent(android.graphics.Rect);
	public void setBoundsInParent(android.graphics.Rect arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setBoundsInScreen(android.graphics.Rect);
	public void setBoundsInScreen(android.graphics.Rect arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setCanOpenPopup(boolean);
	public void setCanOpenPopup(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setCollectionInfo(android.view.accessibility.AccessibilityNodeInfo$CollectionInfo);
	public void setCollectionInfo(android.view.accessibility.AccessibilityNodeInfo.CollectionInfo arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setCollectionItemInfo(android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo);
	public void setCollectionItemInfo(android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setContentInvalid(boolean);
	public void setContentInvalid(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setDismissable(boolean);
	public void setDismissable(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setDrawingOrder(int);
	public void setDrawingOrder(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setError(java.lang.CharSequence);
	public void setError(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setHeading(boolean);
	public void setHeading(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setHintText(java.lang.CharSequence);
	public void setHintText(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setImportantForAccessibility(boolean);
	public void setImportantForAccessibility(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setLabeledBy(android.view.View);
	public void setLabeledBy(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setLabelFor(android.view.View);
	public void setLabelFor(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setLiveRegion(int);
	public void setLiveRegion(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setLongClickable(boolean);
	public void setLongClickable(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setMaxTextLength(int);
	public void setMaxTextLength(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setMovementGranularities(int);
	public void setMovementGranularities(int arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setPackageName(java.lang.CharSequence);
	public void setPackageName(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setPaneTitle(java.lang.CharSequence);
	public void setPaneTitle(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setParent(android.view.View, int);
	public void setParent(android.view.View arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setParent(android.view.View);
	public void setParent(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setPassword(boolean);
	public void setPassword(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setRequestInitialAccessibilityFocus(boolean);
	public void setRequestInitialAccessibilityFocus(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setScreenReaderFocusable(boolean);
	public void setScreenReaderFocusable(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setSelected(boolean);
	public void setSelected(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setShowingHintText(boolean);
	public void setShowingHintText(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setSource(android.view.View, int);
	public void setSource(android.view.View arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setStateDescription(java.lang.CharSequence);
	public void setStateDescription(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTextEntryKey(boolean);
	public void setTextEntryKey(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setText(java.lang.CharSequence);
	public void setText(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTooltipText(java.lang.CharSequence);
	public void setTooltipText(java.lang.CharSequence arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTraversalAfter(android.view.View, int);
	public void setTraversalAfter(android.view.View arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTraversalAfter(android.view.View);
	public void setTraversalAfter(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTraversalBefore(android.view.View, int);
	public void setTraversalBefore(android.view.View arg0, int arg1) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setTraversalBefore(android.view.View);
	public void setTraversalBefore(android.view.View arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setViewIdResourceName(java.lang.String);
	public void setViewIdResourceName(java.lang.String arg0) {}

	// GEN class:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo
	public final class TouchDelegateInfo implements android.os.Parcelable {
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo(java.util.Map<android.graphics.Region, android.view.View>);
		public TouchDelegateInfo(java.util.Map<android.graphics.Region, android.view.View> arg0) {}
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public int getRegionCount();
		public int getRegionCount() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public android.graphics.Region getRegionAt(int);
		public android.graphics.Region getRegionAt(int arg0) { return null; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public android.view.accessibility.AccessibilityNodeInfo getTargetForRegion(android.graphics.Region);
		public android.view.accessibility.AccessibilityNodeInfo getTargetForRegion(android.graphics.Region arg0) { return null; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.view.accessibility.AccessibilityNodeInfo$TouchDelegateInfo:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		protected TouchDelegateInfo() {}
	}


	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public android.view.accessibility.AccessibilityNodeInfo$CollectionItemInfo getCollectionItemInfo();
	public android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo getCollectionItemInfo() { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public boolean isHeading();
	public boolean isHeading() { return false; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.View, int);
	public static android.view.accessibility.AccessibilityNodeInfo obtain(android.view.View arg0, int arg1) { return null; }

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setMultiLine(boolean);
	public void setMultiLine(boolean arg0) {}

	// GEN member:android.view.accessibility.AccessibilityNodeInfo:public void setUniqueId(java.lang.String);
	public void setUniqueId(java.lang.String arg0) {}
}
