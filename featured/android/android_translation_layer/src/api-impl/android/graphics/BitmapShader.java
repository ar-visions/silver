package android.graphics;

public class BitmapShader extends Shader {

	Bitmap bitmap;

	public BitmapShader(Bitmap bitmap, TileMode tileX, TileMode tileY) {
		this.bitmap = bitmap;
	}

	public void setLocalMatrix(Matrix matrix) {}

	// GEN member:android.graphics.BitmapShader:public void setFilterMode(int);
	public void setFilterMode(int arg0) {}
}
