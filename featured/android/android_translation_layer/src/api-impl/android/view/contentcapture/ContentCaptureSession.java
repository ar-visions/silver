package android.view.contentcapture;

// android.jar's shape, made by gen.py
public abstract class ContentCaptureSession implements java.lang.AutoCloseable {
	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void destroy();
	public final void destroy() {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public void close();
	public void close() {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewAppeared(android.view.ViewStructure);
	public final void notifyViewAppeared(android.view.ViewStructure arg0) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewDisappeared(android.view.autofill.AutofillId);
	public final void notifyViewDisappeared(android.view.autofill.AutofillId arg0) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewsAppeared(java.util.List<android.view.ViewStructure>);
	public final void notifyViewsAppeared(java.util.List<android.view.ViewStructure> arg0) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewsDisappeared(android.view.autofill.AutofillId, long[]);
	public final void notifyViewsDisappeared(android.view.autofill.AutofillId arg0, long[] arg1) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewTextChanged(android.view.autofill.AutofillId, java.lang.CharSequence);
	public final void notifyViewTextChanged(android.view.autofill.AutofillId arg0, java.lang.CharSequence arg1) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifyViewInsetsChanged(android.graphics.Insets);
	public final void notifyViewInsetsChanged(android.graphics.Insets arg0) {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifySessionResumed();
	public final void notifySessionResumed() {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final void notifySessionPaused();
	public final void notifySessionPaused() {}

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final android.view.ViewStructure newViewStructure(android.view.View);
	public final android.view.ViewStructure newViewStructure(android.view.View arg0) { return null; }

	// GEN member:android.view.contentcapture.ContentCaptureSession:public android.view.autofill.AutofillId newAutofillId(android.view.autofill.AutofillId, long);
	public android.view.autofill.AutofillId newAutofillId(android.view.autofill.AutofillId arg0, long arg1) { return null; }

	// GEN member:android.view.contentcapture.ContentCaptureSession:public final android.view.ViewStructure newVirtualViewStructure(android.view.autofill.AutofillId, long);
	public final android.view.ViewStructure newVirtualViewStructure(android.view.autofill.AutofillId arg0, long arg1) { return null; }

	// GEN member:android.view.contentcapture.ContentCaptureSession:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected ContentCaptureSession() {}
}
