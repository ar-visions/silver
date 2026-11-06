package android.content.pm;

import android.content.IntentFilter;
import android.graphics.drawable.Drawable;

public class ResolveInfo implements android.os.Parcelable {
	public ActivityInfo activityInfo = new ActivityInfo();
	public ServiceInfo serviceInfo = new ServiceInfo();
	public IntentFilter filter = new IntentFilter();
	public int priority = -500;

	public Drawable loadIcon(PackageManager pm) {
		Drawable icon = activityInfo.loadIcon(pm);
		if (icon == null) {
			icon = new Drawable();
		}
		return icon;
	}

	public CharSequence loadLabel(PackageManager pm) {
		CharSequence label = activityInfo.loadLabel(pm);
		if (label == null) {
			label = "fixme ResolveInfo.loadLabel";
		}
		return label;
	}

	public static class DisplayNameComparator implements java.util.Comparator<ResolveInfo> {
		private final PackageManager pm;
		private final java.text.Collator collator = java.text.Collator.getInstance();

		public DisplayNameComparator(PackageManager pm) {
			this.pm = pm;
		}

		public final int compare(ResolveInfo a, ResolveInfo b) {
			CharSequence sa = a.loadLabel(pm);
			CharSequence sb = b.loadLabel(pm);
			return collator.compare(sa == null ? "" : sa.toString(), sb == null ? "" : sb.toString());
		}
	}

	public ProviderInfo providerInfo;
	public int specificIndex = -1;

	// GEN member:android.content.pm.ResolveInfo:public boolean isDefault;
	public boolean isDefault;

	// GEN member:android.content.pm.ResolveInfo:public java.lang.String resolvePackageName;
	public java.lang.String resolvePackageName;
}
