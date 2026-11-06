package android.view.animation;

public class AnticipateInterpolator extends android.view.animation.BaseInterpolator {
	private final float tension;

	public AnticipateInterpolator() {
		this(2.0f);
	}

	public AnticipateInterpolator(float tension) {
		this.tension = tension;
	}

	public float getInterpolation(float t) {
		return t * t * ((tension + 1) * t - tension);
	}
}
