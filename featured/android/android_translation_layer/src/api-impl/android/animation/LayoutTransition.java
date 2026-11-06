package android.animation;

public class LayoutTransition {

	public void enableTransitionType(int transitionType) {}

	public void setStartDelay(int transitionType, long startDelay) {}

	public void setAnimator(int transitionType, Animator animator) {}

	public void setDuration(long duration) {}

	public Animator getAnimator(int transitionType) {
		return new ObjectAnimator();
	}

	public void setDuration(int transitionType, long duration) {}

	public void setInterpolator(int transitionType, TimeInterpolator interpolator) {}

	public void disableTransitionType(int transitionType) {}

	// GEN member:android.animation.LayoutTransition:public boolean isChangingLayout();
	public boolean isChangingLayout() { return false; }
}
