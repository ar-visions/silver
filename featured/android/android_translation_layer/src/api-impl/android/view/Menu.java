package android.view;

import android.content.ComponentName;
import android.content.Intent;
import android.view.MenuItem;

public interface Menu {

	public static final int NONE = 0;

	public MenuItem add(int groupId, int itemId, int order, CharSequence title);

	public MenuItem add(int groupId, int itemId, int order, int titleRes);

	public MenuItem findItem(int id);

	public MenuItem getItem(int id);

	public void clear();

	public void removeGroup(int groupId);

	public SubMenu addSubMenu(int id);

	public MenuItem add(int id);

	public MenuItem add(CharSequence text);

	public void setGroupCheckable(int group, boolean checkable, boolean exclusive);

	public SubMenu addSubMenu(int groupId, int itemId, int order, CharSequence title);

	public void setGroupVisible(int group, boolean visible);

	public void removeItem(int id);

	public int size();

	public boolean hasVisibleItems();

	public SubMenu addSubMenu(int groupId, int itemId, int order, int titleRes);

	public int addIntentOptions(int groupId, int itemId, int order, ComponentName caller, Intent[] specifics, Intent intent, int flags, MenuItem[] outSpecificItems);

	public void close();

	// GEN member:android.view.Menu:public abstract android.view.SubMenu addSubMenu(java.lang.CharSequence);
	default android.view.SubMenu addSubMenu(java.lang.CharSequence arg0) { return null; }

	// GEN member:android.view.Menu:public abstract boolean isShortcutKey(int, android.view.KeyEvent);
	default boolean isShortcutKey(int arg0, android.view.KeyEvent arg1) { return false; }

	// GEN member:android.view.Menu:public abstract boolean performIdentifierAction(int, int);
	default boolean performIdentifierAction(int arg0, int arg1) { return false; }

	// GEN member:android.view.Menu:public abstract boolean performShortcut(int, android.view.KeyEvent, int);
	default boolean performShortcut(int arg0, android.view.KeyEvent arg1, int arg2) { return false; }

	// GEN member:android.view.Menu:public default void setGroupDividerEnabled(boolean);
	default void setGroupDividerEnabled(boolean arg0) {}

	// GEN member:android.view.Menu:public abstract void setGroupEnabled(int, boolean);
	default void setGroupEnabled(int arg0, boolean arg1) {}

	// GEN member:android.view.Menu:public abstract void setQwertyMode(boolean);
	default void setQwertyMode(boolean arg0) {}
}
