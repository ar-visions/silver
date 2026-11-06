package android.content;

import android.atl.annotation.Export;

public interface DialogInterface {
	/** The identifier for the positive button. */
	int BUTTON_POSITIVE = -1;
	/** The identifier for the negative button. */
	int BUTTON_NEGATIVE = -2;
	/** The identifier for the neutral button. */
	int BUTTON_NEUTRAL = -3;

	@Export
	public void dismiss();

	public void cancel();

	public interface OnDismissListener {
		void onDismiss(DialogInterface dialog);
	}
	public interface OnClickListener {
		@Export
		void onClick(DialogInterface dialog, int which);
	}
	public interface OnShowListener {
		void onShow(DialogInterface dialog);
	}
	public interface OnCancelListener {
	
	// GEN member:android.content.DialogInterface$OnCancelListener:public abstract void onCancel(android.content.DialogInterface);
	default void onCancel(android.content.DialogInterface arg0) {}
}
	public interface OnMultiChoiceClickListener {
	
	// GEN member:android.content.DialogInterface$OnMultiChoiceClickListener:public abstract void onClick(android.content.DialogInterface, int, boolean);
	default void onClick(android.content.DialogInterface arg0, int arg1, boolean arg2) {}
}
	public interface OnKeyListener {
	
	// GEN member:android.content.DialogInterface$OnKeyListener:public abstract boolean onKey(android.content.DialogInterface, int, android.view.KeyEvent);
	default boolean onKey(android.content.DialogInterface arg0, int arg1, android.view.KeyEvent arg2) { return false; }
}
}
