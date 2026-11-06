package android.content.pm;

import android.app.Person;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.LocusId;
import android.graphics.drawable.Icon;
import android.os.Parcelable;
import android.os.PersistableBundle;
import java.util.Set;

public class ShortcutInfo implements Parcelable {
	public static final Creator<ShortcutInfo> CREATOR = null;

	public static class Builder {
		public Builder(Context context, String id) {}

		public ShortcutInfo build() {
			return new ShortcutInfo();
		}

		public Builder setActivity(ComponentName activity) {
			return this;
		}

		public Builder setCategories(Set<String> categories) {
			return this;
		}

		public Builder setDisabledMessage(CharSequence disabledMessage) {
			return this;
		}

		public Builder setExcludedFromSurfaces(int surfaces) {
			return this;
		}

		public Builder setExtras(PersistableBundle extras) {
			return this;
		}

		public Builder setIcon(Icon icon) {
			return this;
		}

		public Builder setIntent(Intent intent) {
			return this;
		}

		public Builder setIntents(Intent[] intents) {
			return this;
		}

		public Builder setLocusId(LocusId locusId) {
			return this;
		}

		public Builder setLongLabel(CharSequence longLabel) {
			return this;
		}

		public Builder setLongLived(boolean longLived) {
			return this;
		}

		public Builder setPersons(Person[] persons) {
			return this;
		}

		public Builder setRank(int rank) {
			return this;
		}

		public Builder setShortLabel(CharSequence shortLabel) {
			return this;
		}
	}

	// GEN member:android.content.pm.ShortcutInfo:public android.content.ComponentName getActivity();
	public android.content.ComponentName getActivity() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public java.util.Set<java.lang.String> getCategories();
	public java.util.Set<java.lang.String> getCategories() { return new java.util.HashSet<>(); }

	// GEN member:android.content.pm.ShortcutInfo:public java.lang.CharSequence getDisabledMessage();
	public java.lang.CharSequence getDisabledMessage() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public int getDisabledReason();
	public int getDisabledReason() { return 0; }

	// GEN member:android.content.pm.ShortcutInfo:public android.os.PersistableBundle getExtras();
	public android.os.PersistableBundle getExtras() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public java.lang.String getId();
	public java.lang.String getId() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public android.content.Intent[] getIntents();
	public android.content.Intent[] getIntents() { return new android.content.Intent[0]; }

	// GEN member:android.content.pm.ShortcutInfo:public long getLastChangedTimestamp();
	public long getLastChangedTimestamp() { return 0; }

	// GEN member:android.content.pm.ShortcutInfo:public android.content.LocusId getLocusId();
	public android.content.LocusId getLocusId() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public java.lang.CharSequence getLongLabel();
	public java.lang.CharSequence getLongLabel() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public java.lang.String getPackage();
	public java.lang.String getPackage() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public int getRank();
	public int getRank() { return 0; }

	// GEN member:android.content.pm.ShortcutInfo:public java.lang.CharSequence getShortLabel();
	public java.lang.CharSequence getShortLabel() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public android.os.UserHandle getUserHandle();
	public android.os.UserHandle getUserHandle() { return null; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean hasKeyFieldsOnly();
	public boolean hasKeyFieldsOnly() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isCached();
	public boolean isCached() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isDeclaredInManifest();
	public boolean isDeclaredInManifest() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isDynamic();
	public boolean isDynamic() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isEnabled();
	public boolean isEnabled() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isImmutable();
	public boolean isImmutable() { return false; }

	// GEN member:android.content.pm.ShortcutInfo:public boolean isPinned();
	public boolean isPinned() { return false; }
}
