package android.view.accessibility;

import java.util.ArrayList;
import java.util.List;

public class AccessibilityManager {

	public interface AccessibilityStateChangeListener {}

	public interface TouchExplorationStateChangeListener {}

	public boolean isTouchExplorationEnabled() { return false; }

	public boolean isEnabled() { return false; }

	public List getEnabledAccessibilityServiceList(int feedbackTypeFlags) {
		return new ArrayList<>();
	}

	public boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener listener) {
		return false;
	}

	public boolean addTouchExplorationStateChangeListener(TouchExplorationStateChangeListener listener) {
		return false;
	}

	public boolean removeAccessibilityStateChangeListener(AccessibilityStateChangeListener listener) {
		return false;
	}

	public boolean removeTouchExplorationStateChangeListener(TouchExplorationStateChangeListener listener) {
		return false;
	}

	// GEN class:android.view.accessibility.AccessibilityManager$AccessibilityServicesStateChangeListener
	public interface AccessibilityServicesStateChangeListener {
		// GEN member:android.view.accessibility.AccessibilityManager$AccessibilityServicesStateChangeListener:public abstract void onAccessibilityServicesStateChanged(android.view.accessibility.AccessibilityManager);
		default void onAccessibilityServicesStateChanged(android.view.accessibility.AccessibilityManager arg0) {}
	}


	// GEN member:android.view.accessibility.AccessibilityManager:public void addAccessibilityServicesStateChangeListener(android.view.accessibility.AccessibilityManager$AccessibilityServicesStateChangeListener);
	public void addAccessibilityServicesStateChangeListener(android.view.accessibility.AccessibilityManager.AccessibilityServicesStateChangeListener arg0) {}

	// GEN member:android.view.accessibility.AccessibilityManager:public int getRecommendedTimeoutMillis(int, int);
	public int getRecommendedTimeoutMillis(int arg0, int arg1) { return 0; }

	// GEN member:android.view.accessibility.AccessibilityManager:public boolean isRequestFromAccessibilityTool();
	public boolean isRequestFromAccessibilityTool() { return false; }

	// GEN member:android.view.accessibility.AccessibilityManager:public boolean removeAccessibilityServicesStateChangeListener(android.view.accessibility.AccessibilityManager$AccessibilityServicesStateChangeListener);
	public boolean removeAccessibilityServicesStateChangeListener(android.view.accessibility.AccessibilityManager.AccessibilityServicesStateChangeListener arg0) { return false; }

	// GEN member:android.view.accessibility.AccessibilityManager:public void sendAccessibilityEvent(android.view.accessibility.AccessibilityEvent);
	public void sendAccessibilityEvent(android.view.accessibility.AccessibilityEvent arg0) {}

	// GEN member:android.view.accessibility.AccessibilityManager:public java.util.List<android.accessibilityservice.AccessibilityServiceInfo> getInstalledAccessibilityServiceList();
	public java.util.List<android.accessibilityservice.AccessibilityServiceInfo> getInstalledAccessibilityServiceList() { return new java.util.ArrayList<>(); }
}
