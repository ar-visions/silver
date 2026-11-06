package android.app;

public class FragmentTransaction {

	private Activity activity;

	public FragmentTransaction(Activity activity) {
		this.activity = activity;
	}

	public FragmentTransaction add(Fragment fragment, String string) {
		fragment.activity = activity;
		activity.fragments.add(fragment);
		return this;
	}

	public int commit() {
		return 0;
	}

	// GEN member:android.app.FragmentTransaction:public abstract int commitAllowingStateLoss();
	public int commitAllowingStateLoss() { return 0; }

	// GEN member:android.app.FragmentTransaction:public abstract android.app.FragmentTransaction replace(int, android.app.Fragment, java.lang.String);
	public android.app.FragmentTransaction replace(int arg0, android.app.Fragment arg1, java.lang.String arg2) { return null; }

	// GEN member:android.app.FragmentTransaction:public abstract void commitNow();
	public void commitNow() {}
}
