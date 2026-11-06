package android.webkit;

// android.jar's shape, made by gen.py
public abstract class WebHistoryItem implements java.lang.Cloneable {
	// GEN member:android.webkit.WebHistoryItem:public android.webkit.WebHistoryItem();
	public WebHistoryItem() {}

	// GEN member:android.webkit.WebHistoryItem:public abstract java.lang.String getUrl();
	public abstract java.lang.String getUrl();

	// GEN member:android.webkit.WebHistoryItem:public abstract java.lang.String getOriginalUrl();
	public abstract java.lang.String getOriginalUrl();

	// GEN member:android.webkit.WebHistoryItem:public abstract java.lang.String getTitle();
	public abstract java.lang.String getTitle();

	// GEN member:android.webkit.WebHistoryItem:public abstract android.graphics.Bitmap getFavicon();
	public abstract android.graphics.Bitmap getFavicon();

	// GEN member:android.webkit.WebHistoryItem:protected abstract android.webkit.WebHistoryItem clone();
	protected abstract android.webkit.WebHistoryItem clone();
}
