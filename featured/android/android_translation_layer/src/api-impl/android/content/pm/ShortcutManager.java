package android.content.pm;

import java.util.Collections;
import java.util.List;

public class ShortcutManager {
	// a phone launcher's largest shortcut icon (xxhdpi, 64 dp)
	public int getIconMaxWidth() { return 192; }
	public int getIconMaxHeight() { return 192; }

	public void removeAllDynamicShortcuts() {
	}

	public List getShortcuts(int matchFlags) {
		return Collections.emptyList();
	}

	// published shortcuts are a launcher concept that has no equivalent outside AOSP
	public List<ShortcutInfo> getDynamicShortcuts() {
		return Collections.emptyList();
	}

	public void removeLongLivedShortcuts(List<String> shortcutIds) {
	}

	public void reportShortcutUsed(String shortcutId) {
	}

	public boolean setDynamicShortcuts(List<ShortcutInfo> shortcutInfoList) {
		return true;
	}

	// the rest of the API: no launcher keeps shortcuts here
	public boolean addDynamicShortcuts(List<ShortcutInfo> shortcutInfoList) { return true; }
	public boolean updateShortcuts(List<ShortcutInfo> shortcutInfoList) { return true; }
	public void pushDynamicShortcut(ShortcutInfo shortcut) {}
	public void removeDynamicShortcuts(List<String> shortcutIds) {}
	public void enableShortcuts(List<String> shortcutIds) {}
	public void disableShortcuts(List<String> shortcutIds) {}
	public void disableShortcuts(List<String> shortcutIds, CharSequence disabledMessage) {}
	public List<ShortcutInfo> getManifestShortcuts() { return Collections.emptyList(); }
	public List<ShortcutInfo> getPinnedShortcuts() { return Collections.emptyList(); }
	public int getMaxShortcutCountPerActivity() { return 15; }
	public boolean isRateLimitingActive() { return false; }
	public boolean isRequestPinShortcutSupported() { return false; }
	public boolean requestPinShortcut(ShortcutInfo shortcut, android.content.IntentSender resultIntent) { return false; }
	public android.content.Intent createShortcutResultIntent(ShortcutInfo shortcut) { return null; }
}
