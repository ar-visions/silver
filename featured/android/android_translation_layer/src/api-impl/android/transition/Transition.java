package android.transition;

import android.animation.TimeInterpolator;
import android.view.View;

public class Transition implements java.lang.Cloneable {

	public interface TransitionListener {}

	public Transition clone() {
		return new Transition();
	}

	public Transition addListener(TransitionListener listener) {
		return this;
	}

	public Transition excludeTarget(int targetId, boolean exclude) {
		return this;
	}

	public Transition addTarget(View target) {
		return this;
	}

	public Transition addTarget(String targetName) {
		return this;
	}

	public Transition setDuration(long duration) {
		return this;
	}

	public Transition setInterpolator(TimeInterpolator interpolator) {
		return this;
	}

	// GEN class:android.transition.Transition$EpicenterCallback
	public abstract class EpicenterCallback {
		// GEN member:android.transition.Transition$EpicenterCallback:public android.transition.Transition$EpicenterCallback();
		public EpicenterCallback() {}
	
		// GEN member:android.transition.Transition$EpicenterCallback:public abstract android.graphics.Rect onGetEpicenter(android.transition.Transition);
		public abstract android.graphics.Rect onGetEpicenter(android.transition.Transition arg0);
	}


	// GEN member:android.transition.Transition:public java.util.List<java.lang.Integer> getTargetIds();
	public java.util.List<java.lang.Integer> getTargetIds() { return new java.util.ArrayList<>(); }

	// GEN member:android.transition.Transition:public java.util.List<java.lang.String> getTargetNames();
	public java.util.List<java.lang.String> getTargetNames() { return new java.util.ArrayList<>(); }

	// GEN member:android.transition.Transition:public java.util.List<android.view.View> getTargets();
	public java.util.List<android.view.View> getTargets() { return new java.util.ArrayList<>(); }

	// GEN member:android.transition.Transition:public java.util.List<java.lang.Class> getTargetTypes();
	public java.util.List<java.lang.Class> getTargetTypes() { return new java.util.ArrayList<>(); }

	// GEN member:android.transition.Transition:public android.transition.Transition removeListener(android.transition.Transition$TransitionListener);
	public android.transition.Transition removeListener(android.transition.Transition.TransitionListener arg0) { return null; }

	// GEN member:android.transition.Transition:public android.transition.Transition removeTarget(android.view.View);
	public android.transition.Transition removeTarget(android.view.View arg0) { return null; }

	// GEN member:android.transition.Transition:public void setEpicenterCallback(android.transition.Transition$EpicenterCallback);
	public void setEpicenterCallback(android.transition.Transition.EpicenterCallback arg0) {}
}
