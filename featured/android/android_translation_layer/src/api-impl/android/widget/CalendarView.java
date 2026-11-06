package android.widget;

// android.jar's shape, made by gen.py
public class CalendarView extends android.widget.FrameLayout {
	// GEN member:android.widget.CalendarView:public android.widget.CalendarView(android.content.Context);
	public CalendarView(android.content.Context arg0) { super((android.content.Context)null); }

	// GEN member:android.widget.CalendarView:public android.widget.CalendarView(android.content.Context, android.util.AttributeSet);
	public CalendarView(android.content.Context arg0, android.util.AttributeSet arg1) { super((android.content.Context)null); }

	// GEN member:android.widget.CalendarView:public android.widget.CalendarView(android.content.Context, android.util.AttributeSet, int);
	public CalendarView(android.content.Context arg0, android.util.AttributeSet arg1, int arg2) { super((android.content.Context)null); }

	// GEN member:android.widget.CalendarView:public android.widget.CalendarView(android.content.Context, android.util.AttributeSet, int, int);
	public CalendarView(android.content.Context arg0, android.util.AttributeSet arg1, int arg2, int arg3) { super((android.content.Context)null); }

	// GEN member:android.widget.CalendarView:public void setShownWeekCount(int);
	public void setShownWeekCount(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getShownWeekCount();
	public int getShownWeekCount() { return 0; }

	// GEN member:android.widget.CalendarView:public void setSelectedWeekBackgroundColor(int);
	public void setSelectedWeekBackgroundColor(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getSelectedWeekBackgroundColor();
	public int getSelectedWeekBackgroundColor() { return 0; }

	// GEN member:android.widget.CalendarView:public void setFocusedMonthDateColor(int);
	public void setFocusedMonthDateColor(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getFocusedMonthDateColor();
	public int getFocusedMonthDateColor() { return 0; }

	// GEN member:android.widget.CalendarView:public void setUnfocusedMonthDateColor(int);
	public void setUnfocusedMonthDateColor(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getUnfocusedMonthDateColor();
	public int getUnfocusedMonthDateColor() { return 0; }

	// GEN member:android.widget.CalendarView:public void setWeekNumberColor(int);
	public void setWeekNumberColor(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getWeekNumberColor();
	public int getWeekNumberColor() { return 0; }

	// GEN member:android.widget.CalendarView:public void setWeekSeparatorLineColor(int);
	public void setWeekSeparatorLineColor(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getWeekSeparatorLineColor();
	public int getWeekSeparatorLineColor() { return 0; }

	// GEN member:android.widget.CalendarView:public void setSelectedDateVerticalBar(int);
	public void setSelectedDateVerticalBar(int arg0) {}

	// GEN member:android.widget.CalendarView:public void setSelectedDateVerticalBar(android.graphics.drawable.Drawable);
	public void setSelectedDateVerticalBar(android.graphics.drawable.Drawable arg0) {}

	// GEN member:android.widget.CalendarView:public android.graphics.drawable.Drawable getSelectedDateVerticalBar();
	public android.graphics.drawable.Drawable getSelectedDateVerticalBar() { return null; }

	// GEN member:android.widget.CalendarView:public void setWeekDayTextAppearance(int);
	public void setWeekDayTextAppearance(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getWeekDayTextAppearance();
	public int getWeekDayTextAppearance() { return 0; }

	// GEN member:android.widget.CalendarView:public void setDateTextAppearance(int);
	public void setDateTextAppearance(int arg0) {}

	// GEN member:android.widget.CalendarView:public int getDateTextAppearance();
	public int getDateTextAppearance() { return 0; }

	// GEN member:android.widget.CalendarView:public long getMinDate();
	public long getMinDate() { return 0; }

	// GEN member:android.widget.CalendarView:public void setMinDate(long);
	public void setMinDate(long arg0) {}

	// GEN member:android.widget.CalendarView:public long getMaxDate();
	public long getMaxDate() { return 0; }

	// GEN member:android.widget.CalendarView:public void setMaxDate(long);
	public void setMaxDate(long arg0) {}

	// GEN member:android.widget.CalendarView:public void setShowWeekNumber(boolean);
	public void setShowWeekNumber(boolean arg0) {}

	// GEN member:android.widget.CalendarView:public boolean getShowWeekNumber();
	public boolean getShowWeekNumber() { return false; }

	// GEN member:android.widget.CalendarView:public int getFirstDayOfWeek();
	public int getFirstDayOfWeek() { return 0; }

	// GEN member:android.widget.CalendarView:public void setFirstDayOfWeek(int);
	public void setFirstDayOfWeek(int arg0) {}

	// GEN member:android.widget.CalendarView:public void setOnDateChangeListener(android.widget.CalendarView$OnDateChangeListener);
	public void setOnDateChangeListener(android.widget.CalendarView.OnDateChangeListener arg0) {}

	// GEN member:android.widget.CalendarView:public long getDate();
	public long getDate() { return 0; }

	// GEN member:android.widget.CalendarView:public void setDate(long);
	public void setDate(long arg0) {}

	// GEN member:android.widget.CalendarView:public void setDate(long, boolean, boolean);
	public void setDate(long arg0, boolean arg1, boolean arg2) {}

	// GEN member:android.widget.CalendarView:protected void onConfigurationChanged(android.content.res.Configuration);
	protected void onConfigurationChanged(android.content.res.Configuration arg0) {}

	// GEN member:android.widget.CalendarView:public java.lang.CharSequence getAccessibilityClassName();
	public java.lang.CharSequence getAccessibilityClassName() { return null; }

	protected CalendarView() { super((android.content.Context)null); }

	// GEN class:android.widget.CalendarView$OnDateChangeListener
	public interface OnDateChangeListener {
		// GEN member:android.widget.CalendarView$OnDateChangeListener:public abstract void onSelectedDayChange(android.widget.CalendarView, int, int, int);
		default void onSelectedDayChange(android.widget.CalendarView arg0, int arg1, int arg2, int arg3) {}
	}

}
