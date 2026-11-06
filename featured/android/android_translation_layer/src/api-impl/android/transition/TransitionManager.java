package android.transition;

import android.view.ViewGroup;

// no animation here: each change lands at once
public class TransitionManager {
	public static void beginDelayedTransition(ViewGroup sceneRoot) {}

	public static void beginDelayedTransition(ViewGroup sceneRoot, Transition transition) {}

	public static void endTransitions(ViewGroup sceneRoot) {}

	public static void go(Scene scene) {
		scene.enter();
	}

	public static void go(Scene scene, Transition transition) {
		scene.enter();
	}

	public void setTransition(Scene scene, Transition transition) {}

	public void setTransition(Scene fromScene, Scene toScene, Transition transition) {}

	public void transitionTo(Scene scene) {
		scene.enter();
	}
}
