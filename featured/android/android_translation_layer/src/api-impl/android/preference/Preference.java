package android.preference;

public class Preference implements java.lang.Comparable<Preference> {
	public interface OnPreferenceChangeListener {
		public boolean onPreferenceChange(Preference preference, Object newValue);
	}
	public void setOnPreferenceChangeListener(OnPreferenceChangeListener onPreferenceChangeListener) {}
	public int compareTo(Preference arg0) { return 0; }
}
