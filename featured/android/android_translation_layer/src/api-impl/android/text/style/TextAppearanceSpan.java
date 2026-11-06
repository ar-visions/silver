package android.text.style;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Typeface;
import android.text.TextPaint;

public class TextAppearanceSpan extends MetricAffectingSpan implements android.text.ParcelableSpan {
	private final String family;
	private final int style;
	private final int textSize;
	private final ColorStateList textColor;
	private final ColorStateList textColorLink;

	public TextAppearanceSpan(Context context, int appearance) {
		this(context, appearance, -1);
	}

	public TextAppearanceSpan(Context context, int appearance, int colorList) {
		TypedArray a = context.obtainStyledAttributes(appearance, com.android.internal.R.styleable.TextAppearance);
		ColorStateList color = a.getColorStateList(com.android.internal.R.styleable.TextAppearance_textColor);
		textColorLink = a.getColorStateList(com.android.internal.R.styleable.TextAppearance_textColorLink);
		textSize = a.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, -1);
		style = a.getInt(com.android.internal.R.styleable.TextAppearance_textStyle, 0);
		family = a.getString(com.android.internal.R.styleable.TextAppearance_fontFamily);
		a.recycle();
		if (colorList >= 0)
			color = context.getResources().getColorStateList(colorList);
		textColor = color;
	}

	public TextAppearanceSpan(String family, int style, int size, ColorStateList color, ColorStateList linkColor) {
		this.family = family;
		this.style = style;
		this.textSize = size;
		this.textColor = color;
		this.textColorLink = linkColor;
	}

	public String getFamily() { return family; }
	public ColorStateList getTextColor() { return textColor; }
	public ColorStateList getLinkTextColor() { return textColorLink; }
	public int getTextSize() { return textSize; }
	public int getTextStyle() { return style; }

	@Override
	public void updateDrawState(TextPaint ds) {
		updateMeasureState(ds);
		if (textColor != null)
			ds.setColor(textColor.getColorForState(ds.drawableState, 0));
		if (textColorLink != null)
			ds.linkColor = textColorLink.getColorForState(ds.drawableState, 0);
	}

	@Override
	public void updateMeasureState(TextPaint ds) {
		if (family != null || style != 0) {
			Typeface tf = ds.getTypeface();
			int old = tf == null ? 0 : tf.getStyle();
			tf = family != null ? Typeface.create(family, old | style) : Typeface.create(tf, old | style);
			ds.setTypeface(tf);
		}
		if (textSize > 0)
			ds.setTextSize(textSize);
	}
}
