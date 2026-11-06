package android.text.style;

public class URLSpan extends ClickableSpan implements android.text.ParcelableSpan {

	private String url;

	public URLSpan(String url) {
		this.url = url;
	}

	public String getURL() {
		return url;
	}
}
