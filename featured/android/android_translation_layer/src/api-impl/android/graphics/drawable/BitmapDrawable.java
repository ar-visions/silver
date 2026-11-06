package android.graphics.drawable;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import com.android.internal.R;
import java.io.IOException;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

public class BitmapDrawable extends Drawable {

	private Bitmap bitmap;
	private ColorStateList tint;
	private PorterDuff.Mode tintMode = DEFAULT_TINT_MODE;
	private ColorFilter colorFilter;
	private PorterDuffColorFilter tintFilter;
	private Paint paint;

	public BitmapDrawable() {
	}

	public BitmapDrawable(Resources res, Bitmap bitmap) {
		this.bitmap = bitmap;
		if (bitmap != null)
			setPaintable(bitmap.getTexture());
	}

	public BitmapDrawable(Bitmap bitmap) {
		this.bitmap = bitmap;
		if (bitmap != null)
			setPaintable(bitmap.getTexture());
	}

	public Bitmap getBitmap() {
		return bitmap;
	}

	public void inflate(Resources r, XmlPullParser parser, AttributeSet attrs, Theme theme)
	    throws XmlPullParserException, IOException {
		final TypedArray a = obtainAttributes(r, theme, attrs, R.styleable.BitmapDrawable);
		if (a.hasValue(R.styleable.BitmapDrawable_src)) {
			BitmapDrawable dr = null;
			try {
				// TODO: getDrawable() might pick up another <bitmap> XML,
				// but we should reject anything other than image files.
				dr = (BitmapDrawable)a.getDrawable(R.styleable.BitmapDrawable_src);
			} catch (java.lang.Exception e) {
				e.printStackTrace();
			}
			if (dr != null)
				bitmap = dr.bitmap;
		}
		a.recycle();
		if (bitmap == null)
			throw new XmlPullParserException("<bitmap> needs a valid `src' attribute");
		setPaintable(bitmap.getTexture());
	}

	@Override
	public int getIntrinsicWidth() {
		if (bitmap == null)
			return 0;
		return bitmap.getWidth();
	}

	@Override
	public int getIntrinsicHeight() {
		if (bitmap == null)
			return 0;
		return bitmap.getHeight();
	}

	// filtered: drawn by draw() through a paint, not the texture
	private void filtered() {
		tintFilter = updateTintFilter(tintFilter, tint, tintMode);
		ColorFilter f = colorFilter != null ? colorFilter : tintFilter;
		if (f == null) {
			paint = null;
			if (bitmap != null)
				setPaintable(bitmap.getTexture());
			return;
		}
		if (paint == null)
			paint = new Paint();
		paint.setColorFilter(f);
		long own = native_constructor();
		setPaintable(own);
		native_unref(own);
		invalidateSelf();
	}

	@Override
	public void setTintList(ColorStateList tint) {
		this.tint = tint;
		filtered();
	}

	@Override
	public void setTintMode(PorterDuff.Mode mode) {
		tintMode = mode;
		filtered();
	}

	@Override
	public void setColorFilter(ColorFilter filter) {
		colorFilter = filter;
		filtered();
	}

	@Override
	public ColorFilter getColorFilter() {
		return colorFilter;
	}

	@Override
	public void clearColorFilter() {
		setColorFilter(null);
	}

	@Override
	public void draw(Canvas canvas) {
		if (bitmap == null)
			return;
		if (paint == null) {
			super.draw(canvas);
			return;
		}
		canvas.drawBitmap(bitmap, null, getBounds(), paint);
	}

	public void setTileModeX(Shader.TileMode mode) {}

	public Paint getPaint() {
		return new Paint();
	}

	public void setGravity(int g) {}
}
