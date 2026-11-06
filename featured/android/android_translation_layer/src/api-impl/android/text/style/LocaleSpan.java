package android.text.style;

import android.os.LocaleList;
import android.text.TextPaint;
import java.util.Locale;

public class LocaleSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
	private final LocaleList locales;

	public LocaleSpan(Locale locale) {
		locales = locale == null ? LocaleList.getEmptyLocaleList() : new LocaleList(locale);
	}

	public LocaleSpan(LocaleList locales) {
		this.locales = locales;
	}

	public Locale getLocale() { return locales.get(0); }
	public LocaleList getLocales() { return locales; }

	@Override
	public void updateDrawState(TextPaint ds) { ds.setTextLocales(locales); }

	@Override
	public void updateMeasureState(TextPaint paint) { paint.setTextLocales(locales); }
}
