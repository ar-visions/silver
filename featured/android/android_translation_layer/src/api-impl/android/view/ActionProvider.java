package android.view;

// android.jar's shape, made by gen.py
public abstract class ActionProvider {
	// GEN member:android.view.ActionProvider:public android.view.ActionProvider(android.content.Context);
	public ActionProvider(android.content.Context arg0) {}

	// GEN member:android.view.ActionProvider:public abstract android.view.View onCreateActionView();
	public abstract android.view.View onCreateActionView();

	// GEN member:android.view.ActionProvider:public android.view.View onCreateActionView(android.view.MenuItem);
	public android.view.View onCreateActionView(android.view.MenuItem arg0) { return null; }

	// GEN member:android.view.ActionProvider:public boolean overridesItemVisibility();
	public boolean overridesItemVisibility() { return false; }

	// GEN member:android.view.ActionProvider:public boolean isVisible();
	public boolean isVisible() { return false; }

	// GEN member:android.view.ActionProvider:public void refreshVisibility();
	public void refreshVisibility() {}

	// GEN member:android.view.ActionProvider:public boolean onPerformDefaultAction();
	public boolean onPerformDefaultAction() { return false; }

	// GEN member:android.view.ActionProvider:public boolean hasSubMenu();
	public boolean hasSubMenu() { return false; }

	// GEN member:android.view.ActionProvider:public void onPrepareSubMenu(android.view.SubMenu);
	public void onPrepareSubMenu(android.view.SubMenu arg0) {}

	protected ActionProvider() {}

	// GEN class:android.view.ActionProvider$VisibilityListener
	public interface VisibilityListener {
		// GEN member:android.view.ActionProvider$VisibilityListener:public abstract void onActionProviderVisibilityChanged(boolean);
		default void onActionProviderVisibilityChanged(boolean arg0) {}
	}

}
