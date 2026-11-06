package android.view.autofill;

import android.graphics.Rect;
import android.view.View;

public class AutofillManager {

	public static abstract class AutofillCallback {}

	public interface AutofillClient {}

	public void registerCallback(AutofillCallback callback) {}

	public void unregisterCallback(AutofillCallback callback) {}

	public void notifyViewEntered(View view, int id, Rect bounds) {}

	public void notifyValueChanged(View view, int id, AutofillValue value) {}

	public void notifyViewExited(View view, int id) {}

	// GEN member:android.view.autofill.AutofillManager:public void cancel();
	public void cancel() {}

	// GEN member:android.view.autofill.AutofillManager:public void commit();
	public void commit() {}

	// GEN member:android.view.autofill.AutofillManager:public void notifyValueChanged(android.view.View);
	public void notifyValueChanged(android.view.View arg0) {}

	// GEN member:android.view.autofill.AutofillManager:public void notifyViewVisibilityChanged(android.view.View, int, boolean);
	public void notifyViewVisibilityChanged(android.view.View arg0, int arg1, boolean arg2) {}

	// GEN member:android.view.autofill.AutofillManager:public void requestAutofill(android.view.View, int, android.graphics.Rect);
	public void requestAutofill(android.view.View arg0, int arg1, android.graphics.Rect arg2) {}

	// GEN member:android.view.autofill.AutofillManager:public void requestAutofill(android.view.View);
	public void requestAutofill(android.view.View arg0) {}
}
