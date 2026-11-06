package android.widget;

import android.content.Context;
import android.util.AttributeSet;

public class RatingBar extends AbsSeekBar {

	public RatingBar(Context context) {
		this(context, null);
	}

	public RatingBar(Context context, AttributeSet attributeSet) {
		super(context, attributeSet);
	}

	public void setRating(float rating) {
		setProgress((int)rating);
	}

	// GEN member:android.widget.RatingBar:public int getNumStars();
	public int getNumStars() { return 0; }
}
