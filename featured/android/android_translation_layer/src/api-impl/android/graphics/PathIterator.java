package android.graphics;

// android.jar's shape, made by gen.py
public class PathIterator implements java.util.Iterator<android.graphics.PathIterator.Segment> {
	// GEN member:android.graphics.PathIterator:public static final int VERB_CLOSE = 5;
	public static final int VERB_CLOSE = 5;

	// GEN member:android.graphics.PathIterator:public static final int VERB_CONIC = 3;
	public static final int VERB_CONIC = 3;

	// GEN member:android.graphics.PathIterator:public static final int VERB_CUBIC = 4;
	public static final int VERB_CUBIC = 4;

	// GEN member:android.graphics.PathIterator:public static final int VERB_DONE = 6;
	public static final int VERB_DONE = 6;

	// GEN member:android.graphics.PathIterator:public static final int VERB_LINE = 1;
	public static final int VERB_LINE = 1;

	// GEN member:android.graphics.PathIterator:public static final int VERB_MOVE = 0;
	public static final int VERB_MOVE = 0;

	// GEN member:android.graphics.PathIterator:public static final int VERB_QUAD = 2;
	public static final int VERB_QUAD = 2;

	// GEN member:android.graphics.PathIterator:public int next(float[], int);
	public int next(float[] arg0, int arg1) { return 0; }

	// GEN member:android.graphics.PathIterator:public boolean hasNext();
	public boolean hasNext() { return false; }

	// GEN member:android.graphics.PathIterator:public int peek();
	public int peek() { return 0; }

	// GEN member:android.graphics.PathIterator:public android.graphics.PathIterator$Segment next();
	public android.graphics.PathIterator.Segment next() { return null; }

	protected PathIterator() {}

	// GEN class:android.graphics.PathIterator$Segment
	public class Segment {
		// GEN member:android.graphics.PathIterator$Segment:public int getVerb();
		public int getVerb() { return 0; }
	
		// GEN member:android.graphics.PathIterator$Segment:public float[] getPoints();
		public float[] getPoints() { return new float[0]; }
	
		// GEN member:android.graphics.PathIterator$Segment:public float getConicWeight();
		public float getConicWeight() { return 0; }
	
		protected Segment() {}
	}

}
