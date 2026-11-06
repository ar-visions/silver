package android.text.format;

import android.content.Context;
import java.util.Date;
import java.util.Locale;

public class DateUtils {

	public static CharSequence getRelativeTimeSpanString(Context context, long millis, boolean withPreposition) {
		return new Date(millis).toString();
	}

	public static CharSequence getRelativeDateTimeString(Context context, long millis, long minResolution, long transitionResolution, int flags) {
		return new Date(millis).toString();
	}

	public static boolean isToday(long millis) {
		Date d1 = new Date(millis);
		Date d2 = new Date();

		return d1.getYear() == d2.getYear() && d1.getMonth() == d2.getMonth() && d1.getDate() == d2.getDate();
	}

	public static String formatElapsedTime(long elapsedSeconds) {
		final long days = elapsedSeconds / (24 * 60 * 60L);
		final long hours = elapsedSeconds / (60 * 60L) % 24L;
		final long minutes = elapsedSeconds / (60L) % 60L;
		final long seconds = elapsedSeconds % 60L;

		if (elapsedSeconds < 0) {
			return "0:00";
		} else if (days > 0) {
			return String.format("%d:%02d:%02d:%02d", days, hours, minutes, seconds);
		} else if (hours > 0) {
			return String.format("%d:%02d:%02d", hours, minutes, seconds);
		} else {
			return String.format("%d:%02d", minutes, seconds);
		}
	}

	public static String formatDateTime(Context context, long millis, int flags) {
		return new Date(millis).toString();
	}

	public static String formatDateRange(Context context, long fromMillis, long toMillis, int flags) {
		if (fromMillis == toMillis)
			return formatDateTime(context, fromMillis, flags);
		else
			return formatDateTime(context, fromMillis, flags) + " - " + formatDateTime(context, toMillis, flags);
	}

	public static CharSequence getRelativeTimeSpanString(long time, long now, long minResolutionMillis, int flags) {
		return new Date(time).toString();
	}

	// GEN member:android.text.format.DateUtils:public static java.util.Formatter formatDateRange(android.content.Context, java.util.Formatter, long, long, int, java.lang.String);
	public static java.util.Formatter formatDateRange(android.content.Context arg0, java.util.Formatter arg1, long arg2, long arg3, int arg4, java.lang.String arg5) { return null; }

	// GEN member:android.text.format.DateUtils:public static java.lang.CharSequence getRelativeTimeSpanString(long, long, long);
	public static java.lang.CharSequence getRelativeTimeSpanString(long arg0, long arg1, long arg2) { return null; }
}
