package android.graphics;

import android.atl.GskCanvas;
import android.atl.annotation.Export;
import android.content.res.Resources;
import android.util.Log;

public class Canvas {

	public static enum EdgeType {
		BW;
	}
	public static final int HAS_ALPHA_LAYER_SAVE_FLAG = (1 << 2);

	private Bitmap bitmap;
	private GskCanvas gsk_canvas;

	public Canvas() {
		if (!(this instanceof GskCanvas)) {
			gsk_canvas = new GskCanvas(0);
		}
	}

	public Canvas(Bitmap bmp) {
		this.bitmap = bmp;
		gsk_canvas = new GskCanvas(bmp.getSnapshot());
	}

	public int save() {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			return gsk_canvas.save();
		}
	}
	public void restore() {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.restore();
		}
	}

	public int getSaveCount() {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			return gsk_canvas.getSaveCount();
		}
	}

	// ---

	/**
	 * Draw the specified Rect using the specified paint. The rectangle will
	 * be filled or framed based on the Style in the paint.
	 *
	 * @param rect  The rect to be drawn
	 * @param paint The paint used to draw the rect
	 */
	public void drawRect(RectF r, Paint paint) {
		drawRect(r.left, r.top, r.right, r.bottom, paint);
	}

	/**
	 * Draw the specified Rect using the specified Paint. The rectangle
	 * will be filled or framed based on the Style in the paint.
	 *
	 * @param r        The rectangle to be drawn.
	 * @param paint    The paint used to draw the rectangle
	 */
	public void drawRect(Rect r, Paint paint) {
		drawRect(r.left, r.top, r.right, r.bottom, paint);
	}

	/**
	 * Draw the specified Rect using the specified paint. The rectangle will
	 * be filled or framed based on the Style in the paint.
	 *
	 * @param left   The left side of the rectangle to be drawn
	 * @param top    The top side of the rectangle to be drawn
	 * @param right  The right side of the rectangle to be drawn
	 * @param bottom The bottom side of the rectangle to be drawn
	 * @param paint  The paint used to draw the rect
	 */
	public void drawRect(float left, float top, float right, float bottom, Paint paint) {
		synchronized (bitmap) {
			if (paint != null && paint.getXfermode() instanceof PorterDuffXfermode && ((PorterDuffXfermode)paint.getXfermode()).porterDuffMode == PorterDuff.Mode.CLEAR.nativeInt) {
				int oldSaveCount = gsk_canvas.getSaveCount();
				gsk_canvas.restoreToCount(1);
				bitmap.eraseColor(0);
				gsk_canvas.snapshot = bitmap.getSnapshot();
				while (gsk_canvas.getSaveCount() < oldSaveCount)
					gsk_canvas.save();
				return;
			}
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawRect(left, top, right, bottom, paint);
		}
	}

	// ---
	/**
	 * Preconcat the current matrix with the specified rotation.
	 *
	 * @param degrees The amount to rotate, in degrees
	 */
	public void rotate(float degrees) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.rotate(degrees);
		}
	}

	/**
	 * Preconcat the current matrix with the specified rotation.
	 *
	 * @param degrees The amount to rotate, in degrees
	 * @param px The x-coord for the pivot point (unchanged by the rotation)
	 * @param py The y-coord for the pivot point (unchanged by the rotation)
	 */
	public void rotate(float degrees, float px, float py) {
		translate(px, py);
		rotate(degrees);
		translate(-px, -py);
	}
	// ---
	/**
	 * Draw the text, with origin at (x,y), using the specified paint. The
	 * origin is interpreted based on the Align setting in the paint.
	 *
	 * @param text  The text to be drawn
	 * @param x     The x-coordinate of the origin of the text being drawn
	 * @param y     The y-coordinate of the origin of the text being drawn
	 * @param paint The paint used for the text (e.g. color, size, style)
	 */
	public void drawText(String text, float x, float y, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawText(text, x, y, paint);
		}
	}

	/**
	 * Draw the text, with origin at (x,y), using the specified paint.
	 * The origin is interpreted based on the Align setting in the paint.
	 *
	 * @param text  The text to be drawn
	 * @param start The index of the first character in text to draw
	 * @param end   (end - 1) is the index of the last character in text to draw
	 * @param x     The x-coordinate of the origin of the text being drawn
	 * @param y     The y-coordinate of the origin of the text being drawn
	 * @param paint The paint used for the text (e.g. color, size, style)
	 */
	public void drawText(String text, int start, int end, float x, float y, Paint paint) {
		drawText(text.substring(start, end), x, y, paint);
	}

	/**
	 * Draw the specified range of text, specified by start/end, with its
	 * origin at (x,y), in the specified Paint. The origin is interpreted
	 * based on the Align setting in the Paint.
	 *
	 * @param text     The text to be drawn
	 * @param start    The index of the first character in text to draw
	 * @param end      (end - 1) is the index of the last character in text
	 *                 to draw
	 * @param x        The x-coordinate of origin for where to draw the text
	 * @param y        The y-coordinate of origin for where to draw the text
	 * @param paint The paint used for the text (e.g. color, size, style)
	 */
	@Export
	public void drawText(CharSequence text, int start, int end, float x, float y, Paint paint) {
		drawText(text.toString().substring(start, end), x, y, paint);
		/*if (text instanceof String || text instanceof SpannedString
		      || text instanceof SpannableString) {
		    native_drawText(mNativeCanvas, text.toString(), start, end, x, y,
			    paint.mBidiFlags, paint.mNativePaint);
		} else if (text instanceof GraphicsOperations) {
		    ((GraphicsOperations) text).drawText(this, start, end, x, y,
			    paint);
		} else {
		    char[] buf = TemporaryBuffer.obtain(end - start);
		    TextUtils.getChars(text, start, end, buf, 0);
		    native_drawText(mNativeCanvas, buf, 0, end - start, x, y,
			    paint.mBidiFlags, paint.mNativePaint);
		    TemporaryBuffer.recycle(buf);
		}*/
	}

	public void drawText(char text[], int start, int end, float x, float y, Paint paint) {
		drawText(new String(text), start, end, x, y, paint);
	}

	public void drawTextOnPath(String text, Path path, float x_offset, float y_offset, Paint paint) {
		Log.w("Canvas", "STUB: drawTextOnPath");
	}

	// ---
	/**
	 * <p>Draw the specified arc, which will be scaled to fit inside the
	 * specified oval.</p>
	 *
	 * <p>If the start angle is negative or >= 360, the start angle is treated
	 * as start angle modulo 360.</p>
	 *
	 * <p>If the sweep angle is >= 360, then the oval is drawn
	 * completely. Note that this differs slightly from SkPath::arcTo, which
	 * treats the sweep angle modulo 360. If the sweep angle is negative,
	 * the sweep angle is treated as sweep angle modulo 360</p>
	 *
	 * <p>The arc is drawn clockwise. An angle of 0 degrees correspond to the
	 * geometric angle of 0 degrees (3 o'clock on a watch.)</p>
	 *
	 * @param oval       The bounds of oval used to define the shape and size
	 *                   of the arc
	 * @param startAngle Starting angle (in degrees) where the arc begins
	 * @param sweepAngle Sweep angle (in degrees) measured clockwise
	 * @param useCenter If true, include the center of the oval in the arc, and
			    close it if it is being stroked. This will draw a wedge
	 * @param paint      The paint used to draw the arc
	 */
	public void drawArc(RectF oval, float startAngle, float sweepAngle, boolean useCenter,
	                    Paint paint) {
		if (oval == null) {
			throw new NullPointerException();
		}
		drawArc(oval.left, oval.top, oval.right, oval.bottom, startAngle, sweepAngle, useCenter, paint);
	}
	// ---
	/**
	 * Preconcat the current matrix with the specified scale.
	 *
	 * @param sx The amount to scale in X
	 * @param sy The amount to scale in Y
	 */
	public /*native*/ void scale(float sx, float sy) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.scale(sx, sy);
		}
	}

	/**
	 * Preconcat the current matrix with the specified scale.
	 *
	 * @param sx The amount to scale in X
	 * @param sy The amount to scale in Y
	 * @param px The x-coord for the pivot point (unchanged by the scale)
	 * @param py The y-coord for the pivot point (unchanged by the scale)
	 */
	public final void scale(float sx, float sy, float px, float py) {
		translate(px, py);
		scale(sx, sy);
		translate(-px, -py);
	}
	// ---
	/**
	 * Draw the specified bitmap, with its top/left corner at (x,y), using
	 * the specified paint, transformed by the current matrix.
	 *
	 * <p>Note: if the paint contains a maskfilter that generates a mask which
	 * extends beyond the bitmap's original width/height (e.g. BlurMaskFilter),
	 * then the bitmap will be drawn as if it were in a Shader with CLAMP mode.
	 * Thus the color outside of the original width/height will be the edge
	 * color replicated.
	 *
	 * <p>If the bitmap and canvas have different densities, this function
	 * will take care of automatically scaling the bitmap to draw at the
	 * same density as the canvas.
	 *
	 * @param bitmap The bitmap to be drawn
	 * @param left   The position of the left side of the bitmap being drawn
	 * @param top    The position of the top side of the bitmap being drawn
	 * @param paint  The paint used to draw the bitmap (may be null)
	 */
	public void drawBitmap(Bitmap bitmap, float left, float top, Paint paint) {
		Rect dst = new Rect((int)left, (int)top, (int)left + bitmap.getWidth(), (int)top + bitmap.getHeight());
		drawBitmap(bitmap, null, dst, paint);
	}

	/**
	 * Draw the specified bitmap, scaling/translating automatically to fill
	 * the destination rectangle. If the source rectangle is not null, it
	 * specifies the subset of the bitmap to draw.
	 *
	 * <p>Note: if the paint contains a maskfilter that generates a mask which
	 * extends beyond the bitmap's original width/height (e.g. BlurMaskFilter),
	 * then the bitmap will be drawn as if it were in a Shader with CLAMP mode.
	 * Thus the color outside of the original width/height will be the edge
	 * color replicated.
	 *
	 * <p>This function <em>ignores the density associated with the bitmap</em>.
	 * This is because the source and destination rectangle coordinate
	 * spaces are in their respective densities, so must already have the
	 * appropriate scaling factor applied.
	 *
	 * @param bitmap The bitmap to be drawn
	 * @param src    May be null. The subset of the bitmap to be drawn
	 * @param dst    The rectangle that the bitmap will be scaled/translated
	 *               to fit into
	 * @param paint  May be null. The paint used to draw the bitmap
	 */
	public void drawBitmap(Bitmap bitmap, Rect src, RectF dst, Paint paint) {
		drawBitmap(bitmap, src, new Rect((int)dst.left, (int)dst.top, (int)dst.right, (int)dst.bottom), paint);
	}

	/**
	 * Draw the specified bitmap, scaling/translating automatically to fill
	 * the destination rectangle. If the source rectangle is not null, it
	 * specifies the subset of the bitmap to draw.
	 *
	 * <p>Note: if the paint contains a maskfilter that generates a mask which
	 * extends beyond the bitmap's original width/height (e.g. BlurMaskFilter),
	 * then the bitmap will be drawn as if it were in a Shader with CLAMP mode.
	 * Thus the color outside of the original width/height will be the edge
	 * color replicated.
	 *
	 * <p>This function <em>ignores the density associated with the bitmap</em>.
	 * This is because the source and destination rectangle coordinate
	 * spaces are in their respective densities, so must already have the
	 * appropriate scaling factor applied.
	 *
	 * @param bitmap The bitmap to be drawn
	 * @param src    May be null. The subset of the bitmap to be drawn
	 * @param dst    The rectangle that the bitmap will be scaled/translated
	 *               to fit into
	 * @param paint  May be null. The paint used to draw the bitmap
	 */
	public void drawBitmap(Bitmap bitmap, Rect src, Rect dst, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = this.bitmap.getSnapshot();
			gsk_canvas.drawBitmap(bitmap, src, dst, paint);
		}
	}

	/**
	 * Treat the specified array of colors as a bitmap, and draw it. This gives
	 * the same result as first creating a bitmap from the array, and then
	 * drawing it, but this method avoids explicitly creating a bitmap object
	 * which can be more efficient if the colors are changing often.
	 *
	 * @param colors Array of colors representing the pixels of the bitmap
	 * @param offset Offset into the array of colors for the first pixel
	 * @param stride The number of colors in the array between rows (must be
	 *               >= width or <= -width).
	 * @param x The X coordinate for where to draw the bitmap
	 * @param y The Y coordinate for where to draw the bitmap
	 * @param width The width of the bitmap
	 * @param height The height of the bitmap
	 * @param hasAlpha True if the alpha channel of the colors contains valid
	 *                 values. If false, the alpha byte is ignored (assumed to
	 *                 be 0xFF for every pixel).
	 * @param paint  May be null. The paint used to draw the bitmap
	 */
	public void drawBitmap(int[] colors, int offset, int stride, float x, float y,
	                       int width, int height, boolean hasAlpha, Paint paint) {
		Log.w("Canvas", "STUB: drawBitmap(colors, offset, ...)");
		/*        // check for valid input
			if (width < 0) {
			    throw new IllegalArgumentException("width must be >= 0");
			}
			if (height < 0) {
			    throw new IllegalArgumentException("height must be >= 0");
			}
			if (Math.abs(stride) < width) {
			    throw new IllegalArgumentException("abs(stride) must be >= width");
			}
			int lastScanline = offset + (height - 1) * stride;
			int length = colors.length;
			if (offset < 0 || (offset + width > length) || lastScanline < 0
				|| (lastScanline + width > length)) {
			    throw new ArrayIndexOutOfBoundsException();
			}
			// quick escape if there's nothing to draw
			if (width == 0 || height == 0) {
			    return;
			}
			// punch down to native for the actual draw
			native_drawBitmap(mNativeCanvas, colors, offset, stride, x, y, width, height, hasAlpha,
				paint != null ? paint.mNativePaint : 0);*/
	}

	/**
	 * Legacy version of drawBitmap(int[] colors, ...) that took ints for x,y
	 */
	public void drawBitmap(int[] colors, int offset, int stride, int x, int y,
	                       int width, int height, boolean hasAlpha, Paint paint) {
		// call through to the common float version
		drawBitmap(colors, offset, stride, (float)x, (float)y, width, height,
		           hasAlpha, paint);
	}

	/**
	 * Draw the bitmap using the specified matrix.
	 *
	 * @param bitmap The bitmap to draw
	 * @param matrix The matrix used to transform the bitmap when it is drawn
	 * @param paint  May be null. The paint used to draw the bitmap
	 */
	public void drawBitmap(Bitmap bitmap, Matrix matrix, Paint paint) {
		save();
		concat(matrix);
		drawBitmap(bitmap, 0, 0, paint);
		restore();
	}
	// ---
	/**
	 * Draw a line segment with the specified start and stop x,y coordinates,
	 * using the specified paint.
	 *
	 * <p>Note that since a line is always "framed", the Style is ignored in the paint.</p>
	 *
	 * <p>Degenerate lines (length is 0) will not be drawn.</p>
	 *
	 * @param startX The x-coordinate of the start point of the line
	 * @param startY The y-coordinate of the start point of the line
	 * @param paint  The paint used to draw the line
	 */
	public void drawLine(float startX, float startY, float stopX, float stopY, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawLine(startX, startY, stopX, stopY, paint);
		}
	}

	public void drawLines(float[] points, Paint paint) {
		drawLines(points, 0, points.length, paint);
	}

	public void drawLines(float[] points, int offset, int count, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawLines(points, offset, count, paint);
		}
	}

	public void setBitmap(Bitmap bitmap) {
		if (bitmap != null && !bitmap.isMutable()) {
			throw new IllegalStateException("Bitmap must be mutable");
		}
		this.bitmap = bitmap;
		gsk_canvas.snapshot = bitmap == null ? 0 : bitmap.getSnapshot();
	}

	public void drawPath(Path path, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawPath(path, paint);
		}
	}

	public void restoreToCount(int count) {
		if (count < 1)
			throw new IllegalArgumentException("count must be >= 1");
		while (getSaveCount() > count)
			restore();
	}

	public void drawRoundRect(float left, float top, float right, float bottom, float rx, float ry, Paint paint) {
		synchronized (bitmap) {
			if (paint.getShader() instanceof BitmapShader) {
				BitmapShader shader = (BitmapShader)paint.getShader();
				drawBitmap(shader.bitmap, 0, 0, paint);
			} else if (paint.getXfermode() instanceof PorterDuffXfermode && ((PorterDuffXfermode)paint.getXfermode()).porterDuffMode == PorterDuff.Mode.CLEAR.nativeInt) {
				int oldSaveCount = gsk_canvas.getSaveCount();
				gsk_canvas.restoreToCount(1);
				bitmap.eraseColor(0);
				gsk_canvas.snapshot = bitmap.getSnapshot();
				while (gsk_canvas.getSaveCount() < oldSaveCount)
					gsk_canvas.save();
			} else {
				gsk_canvas.snapshot = bitmap.getSnapshot();
				gsk_canvas.drawRoundRect(left, top, right, bottom, rx, ry, paint);
			}
		}
	}

	public void drawRoundRect(RectF rect, float rx, float ry, Paint paint) {
		drawRoundRect(rect.left, rect.top, rect.right, rect.bottom, rx, ry, paint);
	}

	public void getMatrix(Matrix matrix) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.getMatrix(matrix);
		}
	}

	public Matrix getMatrix() {
		Matrix matrix = new Matrix();
		getMatrix(matrix);
		return matrix;
	}

	public void translate(float dx, float dy) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.translate(dx, dy);
		}
	}

	public void drawCircle(float cx, float cy, float radius, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawCircle(cx, cy, radius, paint);
		}
	}

	public void drawOval(float left, float top, float right, float bottom, Paint paint) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.drawOval(left, top, right, bottom, paint);
		}
	}

	public Rect getClipBounds() {
		Rect rect = new Rect();
		getClipBounds(rect);
		return rect;
	}

	public void concat(Matrix matrix) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			gsk_canvas.concat(matrix);
		}
	}

	public void setMatrix(Matrix matrix) {
		Matrix transform = getMatrix();
		if (transform.isIdentity()) {
			transform = matrix;
		} else {
			getMatrix().invert(transform); // revert the current matrix
			transform.preConcat(matrix);   // apply the new matrix
		}
		concat(transform);
	}

	public int getWidth() {
		return (bitmap == null) ? 0 : bitmap.getWidth();
	}

	public int getHeight() {
		return (bitmap == null) ? 0 : bitmap.getHeight();
	}

	public void drawColor(int color) {
		Paint paint = new Paint();
		paint.setColor(color);
		drawPaint(paint);
	}

	public void drawARGB(int a, int r, int g, int b) {
		Log.w("Canvas", "STUB: drawARGB(" + a + ", " + r + ", " + g + ", " + b + ")");
	}

	public int saveLayer(RectF bounds, Paint paint, int flags) {
		return save();
	}

	public int saveLayer(RectF bounds, Paint paint) {
		return save();
	}

	public int saveLayer(float left, float top, float right, float bottom, Paint paint) {
		return save();
	}

	public void drawOval(RectF oval, Paint paint) {
		drawRoundRect(oval, oval.width() / 2, oval.height() / 2, paint);
	}

	public void drawColor(int color, PorterDuff.Mode mode) {
		Log.w("Canvas", "STUB: drawColor(" + String.format("0x%08x", color) + ", " + mode + ")");
	}

	public boolean clipRect(int left, int top, int right, int bottom) {
		return clipRect((float)left, top, right, bottom);
	}

	public boolean clipRect(float left, float top, float right, float bottom) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			return gsk_canvas.clipRect(left, top, right, bottom);
		}
	}

	public boolean clipRect(Rect rect) {
		return clipRect((float)rect.left, rect.top, rect.right, rect.bottom);
	}

	public boolean clipRect(Rect rect, Region.Op op) {
		return clipRect((float)rect.left, rect.top, rect.right, rect.bottom);
	}

	public boolean clipPath(Path path) {
		synchronized (bitmap) {
			gsk_canvas.snapshot = bitmap.getSnapshot();
			return gsk_canvas.clipPath(path);
		}
	}

	public boolean clipPath(Path path, Region.Op op) {
		return clipPath(path);
	}

	public boolean isHardwareAccelerated() {
		return false;
	}

	public boolean clipRect(RectF rect) {
		return clipRect(rect.left, rect.top, rect.right, rect.bottom);
	}

	public boolean clipRect(float left, float top, float right, float bottom, Region.Op op) {
		return clipRect(left, top, right, bottom);
	}

	public boolean clipRect(RectF rect, Region.Op op) {
		return clipRect(rect.left, rect.top, rect.right, rect.bottom);
	}

	public void drawArc(float left, float top, float right, float bottom, float startAngle, float sweepAngle, boolean includeCenter, Paint paint) {
		Path path = new Path();
		path.addArc(left, top, right, bottom, startAngle, sweepAngle);
		drawPath(path, paint);
		path.reset();
	}

	public boolean getClipBounds(Rect outRect) {
		/* UGLY HACK */
		outRect.set(0, 0, Resources.getSystem().getDisplayMetrics().widthPixels, Resources.getSystem().getDisplayMetrics().heightPixels);
		return true;
	}

	public void drawPaint(Paint paint) {
		// GskColorNode needs a fixed size. Draw a large rectangle to fill the whole viewport.
		// Values should not be near MAXINT to avoid overflow in the renderer.
		drawRect(-100000, -100000, 100000, 100000, paint);
	}

	public void drawPicture(Picture picture) {
		Log.w("Canvas", "STUB: drawPicture");
	}

	public void drawPoint(float x, float y, Paint paint) {
		Log.w("Canvas", "STUB: drawPoint");
	}

	public void drawPoints(float[] pts, Paint paint) {
		drawPoints(pts, 0, pts.length / 2, paint);
	}

	public void drawPoints(float[] pts, int offset, int count, Paint paint) {
		for (int i = offset; i < count; i++) {
			drawPoint(pts[i * 2], pts[i * 2 + 1], paint);
		}
	}

	public boolean quickReject(float left, float top, float right, float bottom, EdgeType edgeType) {
		return false;
	}

	// GEN class:android.graphics.Canvas$VertexMode
	public enum VertexMode {
		TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN;
	}


	// GEN member:android.graphics.Canvas:public boolean clipOutPath(android.graphics.Path);
	public boolean clipOutPath(android.graphics.Path arg0) { return false; }

	// GEN member:android.graphics.Canvas:public boolean clipOutRect(float, float, float, float);
	public boolean clipOutRect(float arg0, float arg1, float arg2, float arg3) { return false; }

	// GEN member:android.graphics.Canvas:public boolean clipOutRect(int, int, int, int);
	public boolean clipOutRect(int arg0, int arg1, int arg2, int arg3) { return false; }

	// GEN member:android.graphics.Canvas:public boolean clipOutRect(android.graphics.RectF);
	public boolean clipOutRect(android.graphics.RectF arg0) { return false; }

	// GEN member:android.graphics.Canvas:public boolean clipOutRect(android.graphics.Rect);
	public boolean clipOutRect(android.graphics.Rect arg0) { return false; }

	// GEN member:android.graphics.Canvas:public void disableZ();
	public void disableZ() {}

	// GEN member:android.graphics.Canvas:public void drawBitmapMesh(android.graphics.Bitmap, int, int, float[], int, int[], int, android.graphics.Paint);
	public void drawBitmapMesh(android.graphics.Bitmap arg0, int arg1, int arg2, float[] arg3, int arg4, int[] arg5, int arg6, android.graphics.Paint arg7) {}

	// GEN member:android.graphics.Canvas:public void drawColor(int, android.graphics.BlendMode);
	public void drawColor(int arg0, android.graphics.BlendMode arg1) {}

	// GEN member:android.graphics.Canvas:public void drawColor(long, android.graphics.BlendMode);
	public void drawColor(long arg0, android.graphics.BlendMode arg1) {}

	// GEN member:android.graphics.Canvas:public void drawColor(long);
	public void drawColor(long arg0) {}

	// GEN member:android.graphics.Canvas:public void drawDoubleRoundRect(android.graphics.RectF, float, float, android.graphics.RectF, float, float, android.graphics.Paint);
	public void drawDoubleRoundRect(android.graphics.RectF arg0, float arg1, float arg2, android.graphics.RectF arg3, float arg4, float arg5, android.graphics.Paint arg6) {}

	// GEN member:android.graphics.Canvas:public void drawDoubleRoundRect(android.graphics.RectF, float[], android.graphics.RectF, float[], android.graphics.Paint);
	public void drawDoubleRoundRect(android.graphics.RectF arg0, float[] arg1, android.graphics.RectF arg2, float[] arg3, android.graphics.Paint arg4) {}

	// GEN member:android.graphics.Canvas:public void drawGlyphs(int[], int, float[], int, int, android.graphics.fonts.Font, android.graphics.Paint);
	public void drawGlyphs(int[] arg0, int arg1, float[] arg2, int arg3, int arg4, android.graphics.fonts.Font arg5, android.graphics.Paint arg6) {}

	// GEN member:android.graphics.Canvas:public void drawPatch(android.graphics.NinePatch, android.graphics.RectF, android.graphics.Paint);
	public void drawPatch(android.graphics.NinePatch arg0, android.graphics.RectF arg1, android.graphics.Paint arg2) {}

	// GEN member:android.graphics.Canvas:public void drawPatch(android.graphics.NinePatch, android.graphics.Rect, android.graphics.Paint);
	public void drawPatch(android.graphics.NinePatch arg0, android.graphics.Rect arg1, android.graphics.Paint arg2) {}

	// GEN member:android.graphics.Canvas:public void drawPicture(android.graphics.Picture, android.graphics.RectF);
	public void drawPicture(android.graphics.Picture arg0, android.graphics.RectF arg1) {}

	// GEN member:android.graphics.Canvas:public void drawPicture(android.graphics.Picture, android.graphics.Rect);
	public void drawPicture(android.graphics.Picture arg0, android.graphics.Rect arg1) {}

	// GEN member:android.graphics.Canvas:public void drawPosText(char[], int, int, float[], android.graphics.Paint);
	public void drawPosText(char[] arg0, int arg1, int arg2, float[] arg3, android.graphics.Paint arg4) {}

	// GEN member:android.graphics.Canvas:public void drawPosText(java.lang.String, float[], android.graphics.Paint);
	public void drawPosText(java.lang.String arg0, float[] arg1, android.graphics.Paint arg2) {}

	// GEN member:android.graphics.Canvas:public void drawRenderNode(android.graphics.RenderNode);
	public void drawRenderNode(android.graphics.RenderNode arg0) {}

	// GEN member:android.graphics.Canvas:public void drawRGB(int, int, int);
	public void drawRGB(int arg0, int arg1, int arg2) {}

	// GEN member:android.graphics.Canvas:public void drawTextOnPath(char[], int, int, android.graphics.Path, float, float, android.graphics.Paint);
	public void drawTextOnPath(char[] arg0, int arg1, int arg2, android.graphics.Path arg3, float arg4, float arg5, android.graphics.Paint arg6) {}

	// GEN member:android.graphics.Canvas:public void drawTextRun(char[], int, int, int, int, float, float, boolean, android.graphics.Paint);
	public void drawTextRun(char[] arg0, int arg1, int arg2, int arg3, int arg4, float arg5, float arg6, boolean arg7, android.graphics.Paint arg8) {}

	// GEN member:android.graphics.Canvas:public void drawTextRun(android.graphics.text.MeasuredText, int, int, int, int, float, float, boolean, android.graphics.Paint);
	public void drawTextRun(android.graphics.text.MeasuredText arg0, int arg1, int arg2, int arg3, int arg4, float arg5, float arg6, boolean arg7, android.graphics.Paint arg8) {}

	// GEN member:android.graphics.Canvas:public void drawTextRun(java.lang.CharSequence, int, int, int, int, float, float, boolean, android.graphics.Paint);
	public void drawTextRun(java.lang.CharSequence arg0, int arg1, int arg2, int arg3, int arg4, float arg5, float arg6, boolean arg7, android.graphics.Paint arg8) {}

	// GEN member:android.graphics.Canvas:public void drawVertices(android.graphics.Canvas$VertexMode, int, float[], int, float[], int, int[], int, short[], int, int, android.graphics.Paint);
	public void drawVertices(android.graphics.Canvas.VertexMode arg0, int arg1, float[] arg2, int arg3, float[] arg4, int arg5, int[] arg6, int arg7, short[] arg8, int arg9, int arg10, android.graphics.Paint arg11) {}

	// GEN member:android.graphics.Canvas:public void enableZ();
	public void enableZ() {}

	// GEN member:android.graphics.Canvas:public int getDensity();
	public int getDensity() { return 0; }

	// GEN member:android.graphics.Canvas:public android.graphics.DrawFilter getDrawFilter();
	public android.graphics.DrawFilter getDrawFilter() { return null; }

	// GEN member:android.graphics.Canvas:public int getMaximumBitmapHeight();
	public int getMaximumBitmapHeight() { return 0; }

	// GEN member:android.graphics.Canvas:public int getMaximumBitmapWidth();
	public int getMaximumBitmapWidth() { return 0; }

	// GEN member:android.graphics.Canvas:public boolean isOpaque();
	public boolean isOpaque() { return false; }

	// GEN member:android.graphics.Canvas:public boolean quickReject(float, float, float, float);
	public boolean quickReject(float arg0, float arg1, float arg2, float arg3) { return false; }

	// GEN member:android.graphics.Canvas:public boolean quickReject(android.graphics.Path, android.graphics.Canvas$EdgeType);
	public boolean quickReject(android.graphics.Path arg0, android.graphics.Canvas.EdgeType arg1) { return false; }

	// GEN member:android.graphics.Canvas:public boolean quickReject(android.graphics.Path);
	public boolean quickReject(android.graphics.Path arg0) { return false; }

	// GEN member:android.graphics.Canvas:public boolean quickReject(android.graphics.RectF, android.graphics.Canvas$EdgeType);
	public boolean quickReject(android.graphics.RectF arg0, android.graphics.Canvas.EdgeType arg1) { return false; }

	// GEN member:android.graphics.Canvas:public boolean quickReject(android.graphics.RectF);
	public boolean quickReject(android.graphics.RectF arg0) { return false; }

	// GEN member:android.graphics.Canvas:public int saveLayerAlpha(float, float, float, float, int);
	public int saveLayerAlpha(float arg0, float arg1, float arg2, float arg3, int arg4) { return 0; }

	// GEN member:android.graphics.Canvas:public int saveLayerAlpha(float, float, float, float, int, int);
	public int saveLayerAlpha(float arg0, float arg1, float arg2, float arg3, int arg4, int arg5) { return 0; }

	// GEN member:android.graphics.Canvas:public int saveLayerAlpha(android.graphics.RectF, int);
	public int saveLayerAlpha(android.graphics.RectF arg0, int arg1) { return 0; }

	// GEN member:android.graphics.Canvas:public int saveLayerAlpha(android.graphics.RectF, int, int);
	public int saveLayerAlpha(android.graphics.RectF arg0, int arg1, int arg2) { return 0; }

	// GEN member:android.graphics.Canvas:public int saveLayer(float, float, float, float, android.graphics.Paint, int);
	public int saveLayer(float arg0, float arg1, float arg2, float arg3, android.graphics.Paint arg4, int arg5) { return 0; }

	// GEN member:android.graphics.Canvas:public void setDensity(int);
	public void setDensity(int arg0) {}

	// GEN member:android.graphics.Canvas:public void setDrawFilter(android.graphics.DrawFilter);
	public void setDrawFilter(android.graphics.DrawFilter arg0) {}

	// GEN member:android.graphics.Canvas:public void skew(float, float);
	public void skew(float arg0, float arg1) {}
}
