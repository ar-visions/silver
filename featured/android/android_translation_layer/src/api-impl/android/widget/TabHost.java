package android.widget;

import android.view.View;

public class TabHost extends View implements android.view.ViewTreeObserver.OnTouchModeChangeListener {
	public TabHost(android.content.Context context) {
		super(context);
	}

	public static class TabSpec {
		private String tag;
		public TabSpec(String tag) {
			this.tag = tag;
		}

		public TabSpec setIndicator(CharSequence label) {
			return this;
		}

		public TabSpec setIndicator(CharSequence label, android.graphics.drawable.Drawable icon) {
			return this;
		}

		public TabSpec setContent(android.content.Intent intent) {
			return this;
		}

		public TabSpec setContent(android.widget.TabHost.TabContentFactory factory) {
			return this;
		}
	}

	public interface TabContentFactory {
		View createTabContent(String tag);
	}

	public TabSpec newTabSpec(String tag) {
		return new TabSpec(tag);
	}

	public void setup() {}

	public void addTab(TabSpec tabSpec) {}

	public void setCurrentTab(int index) {}

	public void setCurrentTabByTag(String tag) {}

	public void setOnTabChangedListener(OnTabChangeListener listener) {}

	public interface OnTabChangeListener {
		void onTabChanged(String tabId);
	}
	public void onTouchModeChanged(boolean arg0) {}
}
