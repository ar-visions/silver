package android.transition;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

// a layout of a scene root; entering it shows that layout there
public class Scene {
	private ViewGroup root;
	private View layout;
	private int layoutId = -1;
	private Context context;

	public Scene(ViewGroup sceneRoot) {
		root = sceneRoot;
	}

	public Scene(ViewGroup sceneRoot, View layout) {
		root = sceneRoot;
		this.layout = layout;
	}

	private Scene(ViewGroup sceneRoot, int layoutId, Context context) {
		root = sceneRoot;
		this.layoutId = layoutId;
		this.context = context;
	}

	public static Scene getSceneForLayout(ViewGroup sceneRoot, int layoutId, Context context) {
		return new Scene(sceneRoot, layoutId, context);
	}

	public ViewGroup getSceneRoot() {
		return root;
	}

	public void enter() {
		if (layoutId > 0 || layout != null) {
			root.removeAllViews();
			if (layoutId > 0)
				LayoutInflater.from(context).inflate(layoutId, root);
			else
				root.addView(layout);
		}
	}

	public void exit() {}

	public void setEnterAction(Runnable action) {}

	public void setExitAction(Runnable action) {}
}
