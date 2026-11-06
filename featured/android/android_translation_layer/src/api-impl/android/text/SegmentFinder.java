package android.text;

// android.jar's shape, made by gen.py
public abstract class SegmentFinder {
	// GEN member:android.text.SegmentFinder:public static final int DONE = -1;
	public static final int DONE = -1;

	// GEN member:android.text.SegmentFinder:public android.text.SegmentFinder();
	public SegmentFinder() {}

	// GEN member:android.text.SegmentFinder:public abstract int previousStartBoundary(int);
	public abstract int previousStartBoundary(int arg0);

	// GEN member:android.text.SegmentFinder:public abstract int previousEndBoundary(int);
	public abstract int previousEndBoundary(int arg0);

	// GEN member:android.text.SegmentFinder:public abstract int nextStartBoundary(int);
	public abstract int nextStartBoundary(int arg0);

	// GEN member:android.text.SegmentFinder:public abstract int nextEndBoundary(int);
	public abstract int nextEndBoundary(int arg0);
}
