package android.view.inputmethod;

public class InputConnectionWrapper implements android.view.inputmethod.InputConnection {
	private InputConnection target;

	public InputConnectionWrapper(InputConnection target, boolean mutable) {
		this.target = target;
	}

	public void setTarget(InputConnection target) {
		this.target = target;
	}

	public java.lang.CharSequence getTextBeforeCursor(int a0, int a1) {
		return target != null ? target.getTextBeforeCursor(a0, a1) : null;
	}

	public java.lang.CharSequence getTextAfterCursor(int a0, int a1) {
		return target != null ? target.getTextAfterCursor(a0, a1) : null;
	}

	public java.lang.CharSequence getSelectedText(int a0) {
		return target != null ? target.getSelectedText(a0) : null;
	}

	public int getCursorCapsMode(int a0) {
		return target != null ? target.getCursorCapsMode(a0) : 0;
	}

	public android.view.inputmethod.ExtractedText getExtractedText(android.view.inputmethod.ExtractedTextRequest a0, int a1) {
		return target != null ? target.getExtractedText(a0, a1) : null;
	}

	public boolean deleteSurroundingText(int a0, int a1) {
		return target != null ? target.deleteSurroundingText(a0, a1) : false;
	}

	public boolean setComposingText(java.lang.CharSequence a0, int a1) {
		return target != null ? target.setComposingText(a0, a1) : false;
	}

	public boolean setComposingRegion(int a0, int a1) {
		return target != null ? target.setComposingRegion(a0, a1) : false;
	}

	public boolean finishComposingText() {
		return target != null ? target.finishComposingText() : false;
	}

	public boolean commitText(java.lang.CharSequence a0, int a1) {
		return target != null ? target.commitText(a0, a1) : false;
	}

	public boolean commitCompletion(android.view.inputmethod.CompletionInfo a0) {
		return target != null ? target.commitCompletion(a0) : false;
	}

	public boolean commitCorrection(android.view.inputmethod.CorrectionInfo a0) {
		return target != null ? target.commitCorrection(a0) : false;
	}

	public boolean setSelection(int a0, int a1) {
		return target != null ? target.setSelection(a0, a1) : false;
	}

	public boolean performEditorAction(int a0) {
		return target != null ? target.performEditorAction(a0) : false;
	}

	public boolean performContextMenuAction(int a0) {
		return target != null ? target.performContextMenuAction(a0) : false;
	}

	public boolean beginBatchEdit() {
		return target != null ? target.beginBatchEdit() : false;
	}

	public boolean endBatchEdit() {
		return target != null ? target.endBatchEdit() : false;
	}

	public boolean sendKeyEvent(android.view.KeyEvent a0) {
		return target != null ? target.sendKeyEvent(a0) : false;
	}

	public boolean clearMetaKeyStates(int a0) {
		return target != null ? target.clearMetaKeyStates(a0) : false;
	}

	public boolean reportFullscreenMode(boolean a0) {
		return target != null ? target.reportFullscreenMode(a0) : false;
	}

	public boolean performPrivateCommand(java.lang.String a0, android.os.Bundle a1) {
		return target != null ? target.performPrivateCommand(a0, a1) : false;
	}

	public void closeConnection() {
		if (target != null) target.closeConnection();
	}

	public boolean deleteSurroundingTextInCodePoints(int a0, int a1) {
		return target != null ? target.deleteSurroundingTextInCodePoints(a0, a1) : false;
	}

	public void performHandwritingGesture(android.view.inputmethod.HandwritingGesture a0, java.util.concurrent.Executor a1, java.util.function.IntConsumer a2) {
		if (target != null) target.performHandwritingGesture(a0, a1, a2);
	}

	public boolean previewHandwritingGesture(android.view.inputmethod.PreviewableHandwritingGesture a0, android.os.CancellationSignal a1) {
		return target != null ? target.previewHandwritingGesture(a0, a1) : false;
	}

	public boolean requestCursorUpdates(int a0) {
		return target != null ? target.requestCursorUpdates(a0) : false;
	}

	// GEN member:android.view.inputmethod.InputConnectionWrapper:public boolean commitContent(android.view.inputmethod.InputContentInfo, int, android.os.Bundle);
	public boolean commitContent(android.view.inputmethod.InputContentInfo arg0, int arg1, android.os.Bundle arg2) { return false; }
}
