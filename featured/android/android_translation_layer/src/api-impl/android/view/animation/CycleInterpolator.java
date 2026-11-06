package android.view.animation;

public class CycleInterpolator extends android.view.animation.BaseInterpolator {
	private final float cycles;

	public CycleInterpolator(float cycles) {
		this.cycles = cycles;
	}

	public float getInterpolation(float input) {
		return (float)Math.sin(2 * cycles * Math.PI * input);
	}
}
