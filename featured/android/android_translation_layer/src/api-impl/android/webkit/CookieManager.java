package android.webkit;

import android.atl.ATLLoadedApp;
import android.net.Uri;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

// the app's cookies, kept for the process: by domain, then name
public class CookieManager {

	private static CookieManager instance;

	// domain (no leading dot) -> name -> value
	private final Map<String, Map<String, String>> jar = new HashMap<>();
	private boolean accept = true;

	public static synchronized CookieManager getInstance() {
		// HACK: disable NewPipe's WebView based PoToken generator for now
		if (ATLLoadedApp.getPrimaryApplication().pkg.packageName.equals("org.schabi.newpipe")) {
			throw new RuntimeException("CookieManager not yet fully implemented");
		}
		try { // also handle NewPipe forks which can have a different packagename
			ATLLoadedApp.getPrimaryApplication().loadClass(
			    "org.schabi.newpipe.util.potoken.PoTokenWebView");
			throw new RuntimeException("CookieManager not yet fully implemented");
		} catch (ClassNotFoundException e) {
		}
		if (instance == null)
			instance = new CookieManager();
		return instance;
	}

	private static String host_of(String url) {
		if (url == null)
			return "";
		String h = url.contains("://") ? Uri.parse(url).getHost() : url;
		if (h == null)
			h = url;
		h = h.toLowerCase();
		while (h.startsWith("."))
			h = h.substring(1);
		return h;
	}

	private static void call(ValueCallback callback, Object value) {
		if (callback != null)
			callback.onReceiveValue(value);
	}

	public synchronized void setCookie(String url, String value) {
		if (!accept || value == null)
			return;
		String domain = host_of(url);
		String pair = null;
		boolean gone = false;
		for (String part : value.split(";")) {
			String p = part.trim();
			if (pair == null) {
				pair = p;
				continue;
			}
			String lp = p.toLowerCase();
			if (lp.startsWith("domain="))
				domain = host_of(p.substring(7).trim());
			else if (lp.startsWith("max-age=") && p.substring(8).trim().startsWith("-"))
				gone = true;
			else if (lp.equals("max-age=0"))
				gone = true;
		}
		if (pair == null || pair.isEmpty())
			return;
		int eq = pair.indexOf('=');
		String name = (eq < 0) ? pair : pair.substring(0, eq).trim();
		String val = (eq < 0) ? "" : pair.substring(eq + 1).trim();
		Map<String, String> m = jar.get(domain);
		if (m == null) {
			m = new LinkedHashMap<>();
			jar.put(domain, m);
		}
		if (gone)
			m.remove(name);
		else
			m.put(name, val);
	}

	public void setCookie(String url, String value, ValueCallback<Boolean> callback) {
		setCookie(url, value);
		call(callback, Boolean.TRUE);
	}

	// every cookie whose domain is the host or a parent of it
	public synchronized String getCookie(String url) {
		String host = host_of(url);
		StringBuilder sb = new StringBuilder();
		for (Map.Entry<String, Map<String, String>> d : jar.entrySet()) {
			String dom = d.getKey();
			if (!host.equals(dom) && !host.endsWith("." + dom))
				continue;
			for (Map.Entry<String, String> c : d.getValue().entrySet()) {
				if (sb.length() > 0)
					sb.append("; ");
				sb.append(c.getKey()).append('=').append(c.getValue());
			}
		}
		return sb.length() > 0 ? sb.toString() : null;
	}

	public synchronized boolean hasCookies() {
		for (Map<String, String> m : jar.values())
			if (!m.isEmpty())
				return true;
		return false;
	}

	public synchronized void removeAllCookies(ValueCallback<Boolean> callback) {
		boolean had = hasCookies();
		jar.clear();
		call(callback, had);
	}

	// a session cookie is not told apart from a kept one here
	public void removeSessionCookies(ValueCallback<Boolean> callback) {
		call(callback, Boolean.FALSE);
	}

	public void removeExpiredCookie() {}

	public void removeAllCookie() {
		removeAllCookies(null);
	}

	public void removeSessionCookie() {}

	public void flush() {}

	public synchronized void setAcceptCookie(boolean accept) {
		this.accept = accept;
	}

	public synchronized boolean acceptCookie() {
		return accept;
	}

	public boolean acceptThirdPartyCookies(WebView webview) {
		return false;
	}

	public void setAcceptThirdPartyCookies(WebView webView, boolean accept) {}

	public static void setAcceptFileSchemeCookies(boolean accept) {}

	public static boolean allowFileSchemeCookies() {
		return false;
	}
}
