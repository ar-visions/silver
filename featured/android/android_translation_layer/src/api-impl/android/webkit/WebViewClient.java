package android.webkit;

public class WebViewClient {
	public void onPageStarted(WebView view, String url) {}
	public void onPageFinished(WebView view, String url) {}
	public boolean shouldOverrideUrlLoading(WebView view, String url) {
		return false; // continue loading the URL
	}

	public void onPageStarted(WebView view, String url, android.graphics.Bitmap favicon) {}

	public void onLoadResource(WebView view, String url) {}

	public void onPageCommitVisible(WebView view, String url) {}

	public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
		return false;
	}

	// GEN member:android.webkit.WebViewClient:public void onReceivedError(android.webkit.WebView, android.webkit.WebResourceRequest, android.webkit.WebResourceError);
	public void onReceivedError(android.webkit.WebView arg0, android.webkit.WebResourceRequest arg1, android.webkit.WebResourceError arg2) {}

	// GEN member:android.webkit.WebViewClient:public android.webkit.WebResourceResponse shouldInterceptRequest(android.webkit.WebView, android.webkit.WebResourceRequest);
	public android.webkit.WebResourceResponse shouldInterceptRequest(android.webkit.WebView arg0, android.webkit.WebResourceRequest arg1) { return null; }

	// GEN member:android.webkit.WebViewClient:public android.webkit.WebResourceResponse shouldInterceptRequest(android.webkit.WebView, java.lang.String);
	public android.webkit.WebResourceResponse shouldInterceptRequest(android.webkit.WebView arg0, java.lang.String arg1) { return null; }
}
