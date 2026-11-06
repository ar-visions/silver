package android.webkit;

// android.jar's shape, made by gen.py
public abstract class WebBackForwardList implements java.lang.Cloneable, java.io.Serializable {
	// GEN member:android.webkit.WebBackForwardList:public android.webkit.WebBackForwardList();
	public WebBackForwardList() {}

	// GEN member:android.webkit.WebBackForwardList:public abstract android.webkit.WebHistoryItem getCurrentItem();
	public abstract android.webkit.WebHistoryItem getCurrentItem();

	// GEN member:android.webkit.WebBackForwardList:public abstract int getCurrentIndex();
	public abstract int getCurrentIndex();

	// GEN member:android.webkit.WebBackForwardList:public abstract android.webkit.WebHistoryItem getItemAtIndex(int);
	public abstract android.webkit.WebHistoryItem getItemAtIndex(int arg0);

	// GEN member:android.webkit.WebBackForwardList:public abstract int getSize();
	public abstract int getSize();

	// GEN member:android.webkit.WebBackForwardList:protected abstract android.webkit.WebBackForwardList clone();
	protected abstract android.webkit.WebBackForwardList clone();
}
