package android.view.animation;

import android.graphics.Path;

public class PathInterpolator extends BaseInterpolator {

	public PathInterpolator(Path path) {
		super();
	}

	public PathInterpolator(float f1, float f2, float f3, float f4) {
		super();
	}

	@Override
	public float getInterpolation(float input) {
		return input;
	}

	// GEN member:android.view.animation.PathInterpolator:public android.view.animation.PathInterpolator(float, float);
	public PathInterpolator(float arg0, float arg1) { this((android.graphics.Path)null); }
}
