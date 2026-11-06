package android.webkit;

import android.atl.annotation.Export;
import android.content.Context;
import android.content.res.AssetManager;
import android.util.AttributeSet;
import android.util.Base64;
import android.view.MotionEvent;
import android.view.ViewGroup;

public class WebView extends ViewGroup implements android.view.ViewGroup.OnHierarchyChangeListener, android.view.ViewTreeObserver.OnGlobalFocusChangeListener {

	private WebViewClient webViewClient;

	public WebView(Context context) {
		this(context, null);
	}

	public WebView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public WebView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		return native_gtkEventPropagation();
	}

	public WebSettings getSettings() {
		return new WebSettings();
	}

	public void setDownloadListener(DownloadListener downloadListener) {}

	public void setScrollBarStyle(int scrollBarStyle) {}

	public void setWebViewClient(WebViewClient webViewClient) {
		this.webViewClient = webViewClient;
	}

	// to be used by native code
	@Export
	void internalLoadChanged(int loadState, String url) {
		if (loadState == /*WEBKIT_LOAD_STARTED*/ 0 && webViewClient != null) {
			webViewClient.onPageStarted(this, url);
		} else if (loadState == /*WEBKIT_LOAD_FINISHED*/ 3 && webViewClient != null) {
			webViewClient.onPageFinished(this, url);
		}
	}

	@Export
	boolean internalShouldOverrideUrlLoading(String url) {
		if (webViewClient != null)
			return webViewClient.shouldOverrideUrlLoading(this, url);
		else
			return false;
	}

	public void setVerticalScrollBarEnabled(boolean enabled) {}
	public void setVerticalScrollbarOverlay(boolean overlay) {}
	public void setInitialScale(int scaleInPercent) {}

	public void addJavascriptInterface(Object object, String name) {
		// HACK: directly call onRenderingDone for OctoDroid, as the javascript interface is not implemented yet
		if (object.getClass().getName().startsWith("com.gh4a") && "NativeClient".equals(name)) {
			try {
				object.getClass().getMethod("onRenderingDone").invoke(object);
			} catch (ReflectiveOperationException e) {
				e.printStackTrace();
			}
		}
	}

	public void setWebChromeClient(WebChromeClient dummy) {}

	public void removeAllViews() {}

	public void destroy() {}

	public void loadDataWithBaseURL(String baseUrl, String data, String mimeType, String encoding, String historyUrl) {
		if ("base64".equals(encoding)) {
			data = new String(Base64.decode(data, 0));
		}
		if (mimeType != null && mimeType.contains(";")) {
			mimeType = mimeType.substring(0, mimeType.indexOf(";"));
		}
		// webkit doesn't allow overwriting the file:// uri scheme. So we replace it with the android-asset:// scheme
		data = data.replace("file:///android_asset/", "android-asset:///assets/");
		native_loadDataWithBaseURL(widget, baseUrl, data, mimeType, encoding);
	}

	public void loadUrl(String url) {
		if (url.startsWith("javascript:")) {
			System.out.println("loadUrl: " + url + " - not implemented yet");
			return;
		}
		// webkit doesn't allow overwriting the file:// uri scheme. So we replace it with the android-asset:// scheme
		url = url.replace("file:///android_asset/", "android-asset:///assets/");
		native_loadUrl(widget, url);
	}

	public void stopLoading() {}

	// to be used by native code
	@Export
	AssetManager internalGetAssetManager() {
		return getContext().getResources().getAssets();
	}

	public void onPause() {}

	public void onResume() {}

	public boolean isPaused() {
		return false;
	}

	public void resumeTimers() {}

	public void loadData(String data, String mimeType, String encoding) {
		loadDataWithBaseURL("about:blank", data, mimeType, encoding, "about:blank");
	}

	public void evaluateJavascript(String script, ValueCallback resultCallback) {}

	public static void setWebContentsDebuggingEnabled(boolean enabled) {}

	public void clearFormData() {}

	public void clearHistory() {}

	public void clearMatches() {}

	public void clearSslPreferences() {}

	public void clearCache(boolean includeDiskFiles) {}

	// directly accessed by androidx WebViewGlueCommunicator to get ClassLoader
	private static Object getFactory() {
		return new Object();
	}

	public void removeJavascriptInterface(String name) {}

	@Override
	protected native long native_constructor(Context context, AttributeSet attrs);
	private native void native_loadDataWithBaseURL(long widget, String baseUrl, String data, String mimeType, String encoding);
	private native void native_loadUrl(long widget, String url);

	public boolean canGoBack() {
		return false;
	}

	public void goBack() {}

	public void reload() {}

	public String getUrl() {
		return null;
	}

	public static void setDataDirectorySuffix(String suffix) {}

	// GEN class:android.webkit.WebView$WebViewTransport
	public class WebViewTransport {
		// GEN member:android.webkit.WebView$WebViewTransport:public android.webkit.WebView$WebViewTransport(android.webkit.WebView);
		public WebViewTransport(android.webkit.WebView arg0) {}
	
		// GEN member:android.webkit.WebView$WebViewTransport:public synchronized void setWebView(android.webkit.WebView);
		public void setWebView(android.webkit.WebView arg0) {}
	
		// GEN member:android.webkit.WebView$WebViewTransport:public synchronized android.webkit.WebView getWebView();
		public android.webkit.WebView getWebView() { return null; }
	
		protected WebViewTransport() {}
	}


	// GEN member:android.webkit.WebView:public android.webkit.WebBackForwardList copyBackForwardList();
	public android.webkit.WebBackForwardList copyBackForwardList() { return null; }

	// GEN member:android.webkit.WebView:public android.webkit.WebMessagePort[] createWebMessageChannel();
	public android.webkit.WebMessagePort[] createWebMessageChannel() { return new android.webkit.WebMessagePort[0]; }

	// GEN member:android.webkit.WebView:public void loadUrl(java.lang.String, java.util.Map<java.lang.String, java.lang.String>);
	public void loadUrl(java.lang.String arg0, java.util.Map<java.lang.String, java.lang.String> arg1) {}

	// GEN member:android.webkit.WebView:public void postUrl(java.lang.String, byte[]);
	public void postUrl(java.lang.String arg0, byte[] arg1) {}

	// GEN member:android.webkit.WebView:public void postWebMessage(android.webkit.WebMessage, android.net.Uri);
	public void postWebMessage(android.webkit.WebMessage arg0, android.net.Uri arg1) {}

	// GEN member:android.webkit.WebView:public android.webkit.WebBackForwardList restoreState(android.os.Bundle);
	public android.webkit.WebBackForwardList restoreState(android.os.Bundle arg0) { return null; }

	// GEN member:android.webkit.WebView:public android.webkit.WebBackForwardList saveState(android.os.Bundle);
	public android.webkit.WebBackForwardList saveState(android.os.Bundle arg0) { return null; }
	public void onChildViewRemoved(android.view.View arg0, android.view.View arg1) {}
	public void onChildViewAdded(android.view.View arg0, android.view.View arg1) {}
	public void onGlobalFocusChanged(android.view.View arg0, android.view.View arg1) {}

	// GEN class:android.webkit.WebView$HitTestResult
	public class HitTestResult {
		// GEN member:android.webkit.WebView$HitTestResult:public static final int ANCHOR_TYPE = 1;
		public static final int ANCHOR_TYPE = 1;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int EDIT_TEXT_TYPE = 9;
		public static final int EDIT_TEXT_TYPE = 9;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int EMAIL_TYPE = 4;
		public static final int EMAIL_TYPE = 4;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int GEO_TYPE = 3;
		public static final int GEO_TYPE = 3;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int IMAGE_ANCHOR_TYPE = 6;
		public static final int IMAGE_ANCHOR_TYPE = 6;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int IMAGE_TYPE = 5;
		public static final int IMAGE_TYPE = 5;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int PHONE_TYPE = 2;
		public static final int PHONE_TYPE = 2;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int SRC_ANCHOR_TYPE = 7;
		public static final int SRC_ANCHOR_TYPE = 7;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int SRC_IMAGE_ANCHOR_TYPE = 8;
		public static final int SRC_IMAGE_ANCHOR_TYPE = 8;
	
		// GEN member:android.webkit.WebView$HitTestResult:public static final int UNKNOWN_TYPE = 0;
		public static final int UNKNOWN_TYPE = 0;
	
		// GEN member:android.webkit.WebView$HitTestResult:public int getType();
		public int getType() { return 0; }
	
		// GEN member:android.webkit.WebView$HitTestResult:public java.lang.String getExtra();
		public java.lang.String getExtra() { return null; }
	
		protected HitTestResult() {}
	}


	// GEN member:android.webkit.WebView:public android.webkit.WebView(android.content.Context, android.util.AttributeSet, int, boolean);
	public WebView(android.content.Context arg0, android.util.AttributeSet arg1, int arg2, boolean arg3) { this((android.content.Context)null); }

	// GEN member:android.webkit.WebView:public static android.content.pm.PackageInfo getCurrentWebViewPackage();
	public static android.content.pm.PackageInfo getCurrentWebViewPackage() { return null; }

	// GEN member:android.webkit.WebView:public android.webkit.WebView$HitTestResult getHitTestResult();
	public android.webkit.WebView.HitTestResult getHitTestResult() { return null; }

	// GEN member:android.webkit.WebView:public java.lang.String getOriginalUrl();
	public java.lang.String getOriginalUrl() { return null; }

	// GEN member:android.webkit.WebView:public static java.lang.ClassLoader getWebViewClassLoader();
	public static java.lang.ClassLoader getWebViewClassLoader() { return null; }
}
