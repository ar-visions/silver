package android.text.method;

public interface KeyListener {

	// GEN member:android.text.method.KeyListener:public abstract void clearMetaKeyState(android.view.View, android.text.Editable, int);
	default void clearMetaKeyState(android.view.View arg0, android.text.Editable arg1, int arg2) {}

	// GEN member:android.text.method.KeyListener:public abstract int getInputType();
	default int getInputType() { return 0; }

	// GEN member:android.text.method.KeyListener:public abstract boolean onKeyDown(android.view.View, android.text.Editable, int, android.view.KeyEvent);
	default boolean onKeyDown(android.view.View arg0, android.text.Editable arg1, int arg2, android.view.KeyEvent arg3) { return false; }

	// GEN member:android.text.method.KeyListener:public abstract boolean onKeyOther(android.view.View, android.text.Editable, android.view.KeyEvent);
	default boolean onKeyOther(android.view.View arg0, android.text.Editable arg1, android.view.KeyEvent arg2) { return false; }

	// GEN member:android.text.method.KeyListener:public abstract boolean onKeyUp(android.view.View, android.text.Editable, int, android.view.KeyEvent);
	default boolean onKeyUp(android.view.View arg0, android.text.Editable arg1, int arg2, android.view.KeyEvent arg3) { return false; }
}
