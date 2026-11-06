package android.view;

public interface ViewParent {
	public abstract ViewParent getParent();

	public boolean isLayoutRequested();

	public void requestDisallowInterceptTouchEvent(boolean disallowIntercept);

	public abstract boolean onStartNestedScroll(View child, View target, int nestedScrollAxes);

	public boolean onNestedPreFling(View target, float velocityX, float velocityY);

	public boolean onNestedFling(View target, float velocityX, float velocityY, boolean consumed);

	public void onNestedScrollAccepted(View child, View target, int nestedScrollAxes);

	public void onNestedPreScroll(View target, int dx, int dy, int[] consumed);

	public void onNestedScroll(View target, int dxConsumed, int dyConsumed, int dxUnconsumed, int dyUnconsumed);

	public void onStopNestedScroll(View target);

	public void onDescendantInvalidated(View child, View target);

	public void requestLayout();

	// GEN member:android.view.ViewParent:public abstract void notifySubtreeAccessibilityStateChanged(android.view.View, android.view.View, int);
	default void notifySubtreeAccessibilityStateChanged(android.view.View arg0, android.view.View arg1, int arg2) {}

	// GEN member:android.view.ViewParent:public abstract boolean requestSendAccessibilityEvent(android.view.View, android.view.accessibility.AccessibilityEvent);
	default boolean requestSendAccessibilityEvent(android.view.View arg0, android.view.accessibility.AccessibilityEvent arg1) { return false; }

	// GEN member:android.view.ViewParent:public abstract boolean getChildVisibleRect(android.view.View, android.graphics.Rect, android.graphics.Point);
	default boolean getChildVisibleRect(android.view.View arg0, android.graphics.Rect arg1, android.graphics.Point arg2) { return false; }
}
