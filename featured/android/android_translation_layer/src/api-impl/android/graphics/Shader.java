package android.graphics;

public class Shader {

	public enum TileMode {
		CLAMP,
		MIRROR,
		REPEAT,
	// GEN member:android.graphics.Shader$TileMode:public static final android.graphics.Shader$TileMode DECAL;
	DECAL
	}

	protected void init(long ni) {
	}

	public void setLocalMatrix(Matrix matrix) {}

	protected void copyLocalMatrix(Shader dest) {}

	// GEN member:android.graphics.Shader:public boolean getLocalMatrix(android.graphics.Matrix);
	public boolean getLocalMatrix(android.graphics.Matrix arg0) { return false; }
}
