package android.graphics;

public class Picture {

	public Canvas beginRecording(int width, int height) {
		return new Canvas(Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888));
	}

	public void endRecording() {}

	// GEN member:android.graphics.Picture:public void draw(android.graphics.Canvas);
	public void draw(android.graphics.Canvas arg0) {}

	// GEN member:android.graphics.Picture:public int getHeight();
	public int getHeight() { return 0; }

	// GEN member:android.graphics.Picture:public int getWidth();
	public int getWidth() { return 0; }
}
