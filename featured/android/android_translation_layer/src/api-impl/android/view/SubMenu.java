package android.view;

import android.graphics.drawable.Drawable;

public interface SubMenu extends Menu {

	public MenuItem getItem();

	public void clearHeader();

	public SubMenu setIcon(Drawable icon);

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setHeaderIcon(int);
	default android.view.SubMenu setHeaderIcon(int arg0) { return null; }

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setHeaderIcon(android.graphics.drawable.Drawable);
	default android.view.SubMenu setHeaderIcon(android.graphics.drawable.Drawable arg0) { return null; }

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setHeaderTitle(int);
	default android.view.SubMenu setHeaderTitle(int arg0) { return null; }

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setHeaderTitle(java.lang.CharSequence);
	default android.view.SubMenu setHeaderTitle(java.lang.CharSequence arg0) { return null; }

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setHeaderView(android.view.View);
	default android.view.SubMenu setHeaderView(android.view.View arg0) { return null; }

	// GEN member:android.view.SubMenu:public abstract android.view.SubMenu setIcon(int);
	default android.view.SubMenu setIcon(int arg0) { return null; }
}
