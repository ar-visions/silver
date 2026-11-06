package android.webkit;

public class WebChromeClient {

	public void onProgressChanged(WebView view, int newProgress) {}

	public boolean onCreateWindow(WebView view, boolean isDialog, boolean isUserGesture, android.os.Message resultMsg) {
		return false;
	}

	// GEN class:android.webkit.WebChromeClient$CustomViewCallback
	public interface CustomViewCallback {
		// GEN member:android.webkit.WebChromeClient$CustomViewCallback:public abstract void onCustomViewHidden();
		default void onCustomViewHidden() {}
	}


	// GEN member:android.webkit.WebChromeClient:public void onCloseWindow(android.webkit.WebView);
	public void onCloseWindow(android.webkit.WebView arg0) {}

	// GEN member:android.webkit.WebChromeClient:public boolean onConsoleMessage(android.webkit.ConsoleMessage);
	public boolean onConsoleMessage(android.webkit.ConsoleMessage arg0) { return false; }

	// GEN class:android.webkit.WebChromeClient$FileChooserParams
	public abstract class FileChooserParams {
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public static final int MODE_OPEN = 0;
		public static final int MODE_OPEN = 0;
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public static final int MODE_OPEN_MULTIPLE = 1;
		public static final int MODE_OPEN_MULTIPLE = 1;
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public static final int MODE_SAVE = 3;
		public static final int MODE_SAVE = 3;
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public android.webkit.WebChromeClient$FileChooserParams();
		public FileChooserParams() {}
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract int getMode();
		public abstract int getMode();
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract java.lang.String[] getAcceptTypes();
		public abstract java.lang.String[] getAcceptTypes();
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract boolean isCaptureEnabled();
		public abstract boolean isCaptureEnabled();
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract java.lang.CharSequence getTitle();
		public abstract java.lang.CharSequence getTitle();
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract java.lang.String getFilenameHint();
		public abstract java.lang.String getFilenameHint();
	
		// GEN member:android.webkit.WebChromeClient$FileChooserParams:public abstract android.content.Intent createIntent();
		public abstract android.content.Intent createIntent();
	}

}
