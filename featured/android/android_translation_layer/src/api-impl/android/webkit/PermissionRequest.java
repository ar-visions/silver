package android.webkit;

// android.jar's shape, made by gen.py
public abstract class PermissionRequest {
	// GEN member:android.webkit.PermissionRequest:public android.webkit.PermissionRequest();
	public PermissionRequest() {}

	// GEN member:android.webkit.PermissionRequest:public abstract android.net.Uri getOrigin();
	public abstract android.net.Uri getOrigin();

	// GEN member:android.webkit.PermissionRequest:public abstract java.lang.String[] getResources();
	public abstract java.lang.String[] getResources();

	// GEN member:android.webkit.PermissionRequest:public abstract void grant(java.lang.String[]);
	public abstract void grant(java.lang.String[] arg0);

	// GEN member:android.webkit.PermissionRequest:public abstract void deny();
	public abstract void deny();
}
