package android.webkit;

import java.io.InputStream;

public class WebResourceResponse {

	public WebResourceResponse(String mimeType, String encoding, InputStream data) {}

	// GEN member:android.webkit.WebResourceResponse:public int getStatusCode();
	public int getStatusCode() { return 0; }

	// GEN member:android.webkit.WebResourceResponse:public android.webkit.WebResourceResponse(java.lang.String, java.lang.String, int, java.lang.String, java.util.Map<java.lang.String, java.lang.String>, java.io.InputStream);
	public WebResourceResponse(java.lang.String arg0, java.lang.String arg1, int arg2, java.lang.String arg3, java.util.Map<java.lang.String, java.lang.String> arg4, java.io.InputStream arg5) { this((java.lang.String)null, (java.lang.String)null, (java.io.InputStream)null); }

	// GEN member:android.webkit.WebResourceResponse:public java.io.InputStream getData();
	public java.io.InputStream getData() { return null; }

	// GEN member:android.webkit.WebResourceResponse:public java.lang.String getEncoding();
	public java.lang.String getEncoding() { return null; }

	// GEN member:android.webkit.WebResourceResponse:public java.lang.String getMimeType();
	public java.lang.String getMimeType() { return null; }

	// GEN member:android.webkit.WebResourceResponse:public java.lang.String getReasonPhrase();
	public java.lang.String getReasonPhrase() { return null; }

	// GEN member:android.webkit.WebResourceResponse:public java.util.Map<java.lang.String, java.lang.String> getResponseHeaders();
	public java.util.Map<java.lang.String, java.lang.String> getResponseHeaders() { return new java.util.HashMap<>(); }

	// GEN member:android.webkit.WebResourceResponse:public void setResponseHeaders(java.util.Map<java.lang.String, java.lang.String>);
	public void setResponseHeaders(java.util.Map<java.lang.String, java.lang.String> arg0) {}
}
