package android.view;

import android.atl.annotation.Export;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.util.AttributeSet;

/* a TextureView is a SurfaceView's widget here: the app draws into
 * the SurfaceTexture it is handed, through the same EGL window */
public class TextureView extends View {

	private SurfaceTextureListener surfaceTextureListener;
	private SurfaceTexture surfaceTexture;
	private boolean created;
	private int width;
	private int height;

	public TextureView(Context context) {
		super(context);
	}

	public TextureView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public TextureView(Context context, AttributeSet attrs, int defStyleAttr) {
		super(context, attrs, defStyleAttr);
	}

	public TextureView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);
	}

	protected native long native_constructor(Context context, AttributeSet attrs);

	public interface SurfaceTextureListener {
		void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height);
		void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height);
		boolean onSurfaceTextureDestroyed(SurfaceTexture surface);
		void onSurfaceTextureUpdated(SurfaceTexture surface);
	}

	// the surface is there once the widget is realized and has a size
	private void offer() {
		if (!created || width == 0 || height == 0)
			return;
		if (surfaceTexture == null) {
			surfaceTexture = new SurfaceTexture(0);
			surfaceTexture.widget = this.widget;
			if (surfaceTextureListener != null)
				surfaceTextureListener.onSurfaceTextureAvailable(surfaceTexture, width, height);
		} else if (surfaceTextureListener != null) {
			surfaceTextureListener.onSurfaceTextureSizeChanged(surfaceTexture, width, height);
		}
	}

	@Export
	private void surfaceCreated() {
		created = true;
		offer();
	}

	@Export
	private void surfaceChanged(int format, int width, int height) {
		this.width = width;
		this.height = height;
		offer();
	}

	public void setSurfaceTextureListener(SurfaceTextureListener listener) {
		this.surfaceTextureListener = listener;
		if (listener != null && surfaceTexture != null)
			listener.onSurfaceTextureAvailable(surfaceTexture, width, height);
	}

	public SurfaceTextureListener getSurfaceTextureListener() {
		return surfaceTextureListener;
	}

	public boolean isAvailable() {
		return surfaceTexture != null;
	}

	public SurfaceTexture getSurfaceTexture() {
		return surfaceTexture;
	}

	public void setSurfaceTexture(SurfaceTexture surfaceTexture) {
		this.surfaceTexture = surfaceTexture;
	}

	public void setOpaque(boolean opaque) {}

	public boolean isOpaque() {
		return true;
	}

	public void setTransform(android.graphics.Matrix transform) {}

	public android.graphics.Matrix getTransform(android.graphics.Matrix transform) {
		return transform;
	}

	public android.graphics.Bitmap getBitmap() {
		return null;
	}

	public android.graphics.Bitmap getBitmap(int width, int height) {
		return null;
	}

	public android.graphics.Bitmap getBitmap(android.graphics.Bitmap bitmap) {
		return bitmap;
	}

	public android.graphics.Canvas lockCanvas() {
		return null;
	}

	public void unlockCanvasAndPost(android.graphics.Canvas canvas) {}
}
