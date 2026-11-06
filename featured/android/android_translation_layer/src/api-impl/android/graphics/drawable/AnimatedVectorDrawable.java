package android.graphics.drawable;

public class AnimatedVectorDrawable extends Drawable implements Animatable2 {

	public void registerAnimationCallback(Animatable2.AnimationCallback callback) {}

	public boolean unregisterAnimationCallback(Animatable2.AnimationCallback callback) {
		return false;
	}

	public void start() {}

	public void stop() {}

	// GEN member:android.graphics.drawable.AnimatedVectorDrawable:public boolean isRunning();
	public boolean isRunning() { return false; }
}
