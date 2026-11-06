package android.view;

import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;

public interface MenuItem {

	public interface OnMenuItemClickListener {
		public boolean onMenuItemClick(MenuItem item);
	}

	public interface OnActionExpandListener {
		public boolean onMenuItemActionExpand(MenuItem item);
		public boolean onMenuItemActionCollapse(MenuItem item);
	}

	public MenuItem setIcon(int iconRes);

	public MenuItem setVisible(boolean visible);

	public MenuItem setChecked(boolean checked);

	public MenuItem setEnabled(boolean enabled);

	public MenuItem setCheckable(boolean checkable);

	public boolean isCheckable();

	public MenuItem setTitleCondensed(CharSequence titleCondensed);

	public MenuItem setTitle(CharSequence title);

	public MenuItem setActionView(View actionView);

	public void setShowAsAction(int action);

	public int getItemId();

	public int getGroupId();

	public MenuItem setOnMenuItemClickListener(OnMenuItemClickListener listener);

	public MenuItem setTitle(int resId);

	public boolean isVisible();

	public Drawable getIcon();

	public SubMenu getSubMenu();

	public MenuItem setActionView(int resId);

	public View getActionView();

	public boolean hasSubMenu();

	public MenuItem setOnActionExpandListener(OnActionExpandListener listener);

	public MenuItem setIcon(Drawable icon);

	public boolean isChecked();

	public MenuItem setShowAsActionFlags(int action);

	public MenuItem setAlphabeticShortcut(char alphaChar);

	public MenuItem setShortcut(char numeric, char alpha);

	public int getOrder();

	public boolean isEnabled();

	public CharSequence getTitleCondensed();

	public CharSequence getTitle();

	public MenuItem setNumericShortcut(char numericChar);

	public boolean expandActionView();

	public boolean isActionViewExpanded();

	public MenuItem setIntent(Intent intent);

	public MenuItem setIconTintList(ColorStateList tint);

	// GEN member:android.view.MenuItem:public abstract boolean collapseActionView();
	default boolean collapseActionView() { return false; }

	// GEN member:android.view.MenuItem:public abstract char getAlphabeticShortcut();
	default char getAlphabeticShortcut() { return 0; }

	// GEN member:android.view.MenuItem:public abstract android.content.Intent getIntent();
	default android.content.Intent getIntent() { return null; }

	// GEN member:android.view.MenuItem:public abstract android.view.ContextMenu$ContextMenuInfo getMenuInfo();
	default android.view.ContextMenu.ContextMenuInfo getMenuInfo() { return null; }

	// GEN member:android.view.MenuItem:public abstract char getNumericShortcut();
	default char getNumericShortcut() { return 0; }

	// GEN member:android.view.MenuItem:public default android.view.MenuItem setAlphabeticShortcut(char, int);
	default android.view.MenuItem setAlphabeticShortcut(char arg0, int arg1) { return null; }

	// GEN member:android.view.MenuItem:public default android.view.MenuItem setContentDescription(java.lang.CharSequence);
	default android.view.MenuItem setContentDescription(java.lang.CharSequence arg0) { return null; }

	// GEN member:android.view.MenuItem:public default android.view.MenuItem setIconTintMode(android.graphics.PorterDuff$Mode);
	default android.view.MenuItem setIconTintMode(android.graphics.PorterDuff.Mode arg0) { return null; }

	// GEN member:android.view.MenuItem:public default android.view.MenuItem setNumericShortcut(char, int);
	default android.view.MenuItem setNumericShortcut(char arg0, int arg1) { return null; }

	// GEN member:android.view.MenuItem:public default android.view.MenuItem setTooltipText(java.lang.CharSequence);
	default android.view.MenuItem setTooltipText(java.lang.CharSequence arg0) { return null; }

	// GEN member:android.view.MenuItem:public abstract android.view.MenuItem setActionProvider(android.view.ActionProvider);
	default android.view.MenuItem setActionProvider(android.view.ActionProvider arg0) { return null; }
}
