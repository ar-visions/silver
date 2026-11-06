package android.text.format;

import java.util.Calendar;
import java.util.TimeZone;

public class Time {
	// GEN default constructor, kept
	public Time() {}

	public long gmtoff = 0;
	public String timezone = TimeZone.getDefault().getID();
	public int second, minute, hour, monthDay, month, year, weekDay, yearDay, isDst;

	public void set(long millis) {
		Calendar c = Calendar.getInstance(TimeZone.getTimeZone(timezone));
		c.setTimeInMillis(millis);
		second = c.get(Calendar.SECOND);
		minute = c.get(Calendar.MINUTE);
		hour = c.get(Calendar.HOUR_OF_DAY);
		monthDay = c.get(Calendar.DAY_OF_MONTH);
		month = c.get(Calendar.MONTH);
		year = c.get(Calendar.YEAR);
		weekDay = c.get(Calendar.DAY_OF_WEEK) - 1;
		yearDay = c.get(Calendar.DAY_OF_YEAR) - 1;
		isDst = c.get(Calendar.DST_OFFSET) != 0 ? 1 : 0;
		gmtoff = (c.get(Calendar.ZONE_OFFSET) + c.get(Calendar.DST_OFFSET)) / 1000;
	}

	public void setToNow() { set(System.currentTimeMillis()); }

	// GEN member:android.text.format.Time:public android.text.format.Time(java.lang.String);
	public Time(java.lang.String arg0) {}

	// GEN member:android.text.format.Time:public static java.lang.String getCurrentTimezone();
	public static java.lang.String getCurrentTimezone() { return null; }

	// GEN member:android.text.format.Time:public static int getJulianDay(long, long);
	public static int getJulianDay(long arg0, long arg1) { return 0; }

	// GEN member:android.text.format.Time:public int getWeekNumber();
	public int getWeekNumber() { return 0; }

	// GEN member:android.text.format.Time:public long normalize(boolean);
	public long normalize(boolean arg0) { return 0; }

	// GEN member:android.text.format.Time:public void set(int, int, int);
	public void set(int arg0, int arg1, int arg2) {}

	// GEN member:android.text.format.Time:public long setJulianDay(int);
	public long setJulianDay(int arg0) { return 0; }

	// GEN member:android.text.format.Time:public long toMillis(boolean);
	public long toMillis(boolean arg0) { return 0; }
}
