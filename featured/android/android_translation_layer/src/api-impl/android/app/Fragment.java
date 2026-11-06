package android.app;

import android.os.Bundle;

public class Fragment implements android.content.ComponentCallbacks2, android.view.View.OnCreateContextMenuListener {

	Activity activity;
	// steps it has had, so each comes once and in order
	boolean created;
	boolean activity_created;

	public void onAttach(Activity activity) {
	}

	public void onCreate(Bundle savedInstanceState) {
	}

	public void onActivityCreated(Bundle savedInstanceState) {
	}

	// created, then told its activity's onCreate has run, as Android
	void bring_to_activity_created() {
		if (!created) {
			created = true;
			onAttach(activity);
			onCreate(null);
		}
		if (!activity_created) {
			activity_created = true;
			onActivityCreated(null);
		}
	}

	public void onStart() {
	}

	public void onResume() {
	}

	public void onPause() {
	}

	public void onStop() {
	}

	public void onDestroy() {
	}

	public Activity getActivity() {
		return activity;
	}

	// GEN member:android.app.Fragment:public void dump(java.lang.String, java.io.FileDescriptor, java.io.PrintWriter, java.lang.String[]);
	public void dump(java.lang.String arg0, java.io.FileDescriptor arg1, java.io.PrintWriter arg2, java.lang.String[] arg3) {}

	// GEN member:android.app.Fragment:public static android.app.Fragment instantiate(android.content.Context, java.lang.String, android.os.Bundle);
	public static android.app.Fragment instantiate(android.content.Context arg0, java.lang.String arg1, android.os.Bundle arg2) { return null; }

	// GEN member:android.app.Fragment:public final boolean isRemoving();
	public final boolean isRemoving() { return false; }

	// GEN member:android.app.Fragment:public void onActivityResult(int, int, android.content.Intent);
	public void onActivityResult(int arg0, int arg1, android.content.Intent arg2) {}

	// GEN member:android.app.Fragment:public void onSaveInstanceState(android.os.Bundle);
	public void onSaveInstanceState(android.os.Bundle arg0) {}

	// GEN member:android.app.Fragment:public void onAttach(android.content.Context);
	public void onAttach(android.content.Context arg0) {}

	// GEN member:android.app.Fragment:public void onDetach();
	public void onDetach() {}
}
