package android.widget;

import android.annotation.UnsupportedAppUsage;
import android.atl.annotation.Export;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.BoringLayout;
import android.text.Editable;
import android.text.InputFilter;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.TextDirectionHeuristic;
import android.text.TextDirectionHeuristics;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.BaseMovementMethod;
import android.text.method.KeyListener;
import android.text.method.MovementMethod;
import android.text.method.TransformationMethod;
import android.text.style.URLSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.ActionMode;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;

public class TextView extends View implements android.view.ViewTreeObserver.OnPreDrawListener {
	private ColorStateList colors = new ColorStateList(new int[][] {new int[0]}, new int[1]);
	private CharSequence text = "";
	private TextPaint paint = new TextPaint();
	private boolean include_padding = false;
	private int break_strategy = 0 /*BREAK_STRATEGY_SIMPLE*/;
	private int hyphenation_frequency = 0 /*HYPHENATION_FREQUENCY_NONE*/;
	private int gravity = Gravity.TOP | Gravity.START;

	public TextView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
	}

	public TextView(Context context) {
		this(context, null);
	}

	public TextView(Context context, AttributeSet attrs, int defStyleAttr) {
		this(context, attrs, defStyleAttr, 0);
	}

	public TextView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
		super(context, attrs, defStyleAttr, defStyleRes);

		TypedArray a = context.obtainStyledAttributes(attrs, com.android.internal.R.styleable.TextView, defStyleAttr, 0);
		try {
			if (a.hasValue(com.android.internal.R.styleable.TextView_text)) {
				setText(a.getText(com.android.internal.R.styleable.TextView_text));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_hint)) {
				setHint(a.getText(com.android.internal.R.styleable.TextView_hint));
			}

			int ap = a.getResourceId(com.android.internal.R.styleable.TextView_textAppearance, -1);
			if (ap != -1) {
				TypedArray aa = context.obtainStyledAttributes(ap, com.android.internal.R.styleable.TextAppearance);
				if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textColor)) {
					setTextColor(aa.getColorStateList(com.android.internal.R.styleable.TextAppearance_textColor));
				}
				if (aa.hasValue(com.android.internal.R.styleable.TextAppearance_textSize)) {
					setTextSize(aa.getDimensionPixelSize(com.android.internal.R.styleable.TextAppearance_textSize, 10));
				}
				aa.recycle();
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textColor)) {
				setTextColor(a.getColorStateList(com.android.internal.R.styleable.TextView_textColor));
			}
			if (a.hasValue(com.android.internal.R.styleable.TextView_textSize)) {
				setTextSize(a.getDimensionPixelSize(com.android.internal.R.styleable.TextView_textSize, 10));
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textStyle)) {
				int textStyle = a.getInt(com.android.internal.R.styleable.TextView_textStyle, 0);
				setTypeface(getTypeface(), textStyle);
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_gravity)) {
				setGravity(a.getInt(com.android.internal.R.styleable.TextView_gravity, gravity));
			}

			if (a.hasValue(com.android.internal.R.styleable.TextView_textAllCaps)) {
				boolean allCaps = a.getBoolean(com.android.internal.R.styleable.TextView_textAllCaps, false);
				setAllCaps(allCaps);
			}
		} catch (java.lang.Exception e) {
			System.out.println("exception while inflating TextView:");
			e.printStackTrace();
		}

		a.recycle();
		haveCustomMeasure = false;
	}

	@Override
	protected native long native_constructor(Context context, AttributeSet attrs);

	public void setText(int resId) {
		setText(getContext().getResources().getText(resId));
	}

	public void setText(CharSequence text) {
		setText(text, BufferType.NORMAL);
	}

	public void setText(CharSequence text, BufferType type) {
		this.text = text == null ? "" : text;
		String plain = text != null ? text.toString() : null;
		native_setText(plain);
		native_setSpans(text instanceof android.text.Spanned ? spanRuns((android.text.Spanned)text, plain) : null);
		if (!isLayoutRequested())
			requestLayout();
	}

	public void setText(char[] text, int start, int len) {
		setText(new String(text, start, len));
	}

	public final void append(CharSequence text) {
		append(text, 0, text.length());
	}

	public void append(CharSequence text, int start, int end) {
		setText(SpannableStringBuilder.valueOf(this.text).append(text, start, end));
	}

	// spans as runs of 4: utf-8 start, end, kind, value
	private static int[] spanRuns(android.text.Spanned text, String plain) {
		Object[] spans = text.getSpans(0, text.length(), Object.class);
		if (spans.length == 0)
			return null;
		int[] at = new int[plain.length() + 1];
		int b = 0;
		for (int i = 0; i < plain.length(); i++) {
			at[i] = b;
			b += utf8Len(plain.charAt(i));
		}
		at[plain.length()] = b;
		java.util.ArrayList<int[]> runs = new java.util.ArrayList<>();
		for (Object span : spans) {
			int s = Math.max(0, Math.min(text.getSpanStart(span), plain.length()));
			int e = Math.max(0, Math.min(text.getSpanEnd(span), plain.length()));
			if (e <= s)
				continue;
			s = at[s];
			e = at[e];
			if (span instanceof android.text.style.StyleSpan) {
				int st = ((android.text.style.StyleSpan)span).getStyle();
				if ((st & Typeface.BOLD) != 0)
					runs.add(new int[] {s, e, 1, 700});
				if ((st & Typeface.ITALIC) != 0)
					runs.add(new int[] {s, e, 2, 0});
			} else if (span instanceof android.text.style.UnderlineSpan || span instanceof android.text.style.URLSpan) {
				runs.add(new int[] {s, e, 3, 0});
			} else if (span instanceof android.text.style.StrikethroughSpan) {
				runs.add(new int[] {s, e, 4, 0});
			} else if (span instanceof android.text.style.ForegroundColorSpan) {
				runs.add(new int[] {s, e, 5, ((android.text.style.ForegroundColorSpan)span).getForegroundColor()});
			} else if (span instanceof android.text.style.BackgroundColorSpan) {
				runs.add(new int[] {s, e, 6, ((android.text.style.BackgroundColorSpan)span).getBackgroundColor()});
			} else if (span instanceof android.text.style.RelativeSizeSpan) {
				runs.add(new int[] {s, e, 7, Float.floatToIntBits(((android.text.style.RelativeSizeSpan)span).getSizeChange())});
			} else if (span instanceof android.text.style.TypefaceSpan) {
				String fam = ((android.text.style.TypefaceSpan)span).getFamily();
				if ("monospace".equals(fam))
					runs.add(new int[] {s, e, 8, 0});
			} else if (span instanceof android.text.style.SuperscriptSpan) {
				runs.add(new int[] {s, e, 9, 1});
			} else if (span instanceof android.text.style.SubscriptSpan) {
				runs.add(new int[] {s, e, 9, -1});
			}
		}
		if (runs.isEmpty())
			return null;
		int[] out = new int[runs.size() * 4];
		for (int i = 0; i < runs.size(); i++)
			System.arraycopy(runs.get(i), 0, out, i * 4, 4);
		return out;
	}

	private static int utf8Len(char c) {
		return c < 0x80 ? 1 : c < 0x800 ? 2 : Character.isSurrogate(c) ? 2 : 3;
	}

	private native final void native_setSpans(int[] runs);

	public native final void native_setText(String text);

	public void setTextSize(int unit, float size) {
		if (unit != TypedValue.COMPLEX_UNIT_SP)
			System.out.println("setTextSize called with non-SP unit (" + unit + "), we don't currently handle that");
		setTextSize(size);
	}
	public native void setTextSize(float size);

	public native final void native_setTextColor(int color);
	public void setTextColor(int color) {
		native_setTextColor(color);
	}
	public void setTextColor(ColorStateList colors) {
		if (colors != null) {
			this.colors = colors;
			native_setTextColor(colors.getDefaultColor()); // TODO: do this properly
		}
	}
	// the one set, never null: Android's TextView starts on DEFAULT
	private Typeface typeface;

	public void setTypeface(Typeface tf, int style) {
		// as Android: the one-argument form, which subclasses record
		setTypeface(style > 0 ? (tf == null ? Typeface.defaultFromStyle(style) : Typeface.create(tf, style)) : tf);
		String[] classesToRemove = {"ATL-font-bold", "ATL-font-italic"};
		native_removeClasses(widget, classesToRemove);

		switch (style) {
			case Typeface.BOLD:
				native_addClass(widget, "ATL-font-bold");
				break;
			case Typeface.ITALIC:
				native_addClass(widget, "ATL-font-italic");
				break;
			case Typeface.BOLD_ITALIC:
				native_addClass(widget, "ATL-font-bold");
				native_addClass(widget, "ATL-font-italic");
				break;
			default:
				break;
		}
	}
	public void setTypeface(Typeface tf) {
		typeface = tf != null ? tf : Typeface.DEFAULT;
	}
	public void setLineSpacing(float add, float mult) {}
	public final void setLinksClickable(boolean whether) {}

	public void setInputType(int type) {}
	public void setFilters(InputFilter[] filters) {}
	public void setCursorVisible(boolean visible) {}
	public void setImeOptions(int imeOptions) {}

	public final ColorStateList getTextColors() { return colors; }
	public static ColorStateList getTextColors(Context context, TypedArray attrs) { return new ColorStateList(new int[][] {new int[0]}, new int[1]); }

	public TextPaint getPaint() {
		return paint;
	}

	public void addTextChangedListener(TextWatcher watcher) {}
	public void removeTextChangedListener(TextWatcher watcher) {}
	public void setOnEditorActionListener(TextView.OnEditorActionListener l) {}

	public TransformationMethod getTransformationMethod() {
		return null;
	}

	public void setHintTextColor(ColorStateList colorStateList) {}
	public void setHintTextColor(int i) {}
	public void setLinkTextColor(ColorStateList colorStateList) {}

	public void setSingleLine() {}
	public void setSelection(int i) {}
	public void setSelection(int i, int j) {}

	public int getSelectionStart() {
		return 0;
	}

	public int getSelectionEnd() {
		return 0;
	}

	public void setEllipsize(TextUtils.TruncateAt truncateAt) {}

	public void setTextAppearance(Context context, int appearance) {}

	public void setMaxLines(int maxLines) {}

	public void setMinWidth(int minWidth) {}
	public void setMaxWidth(int maxWidth) {}

	// read before the field is set when View's constructor calls in
	public Typeface getTypeface() { return typeface != null ? typeface : Typeface.DEFAULT; }

	public float getTextSize() { return 10; }

	public int getGravity() {
		return gravity;
	}

	@Override
	public void setGravity(int gravity) {
		if ((gravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) == 0)
			gravity |= Gravity.START;
		if ((gravity & Gravity.VERTICAL_GRAVITY_MASK) == 0)
			gravity |= Gravity.TOP;
		this.gravity = gravity;
		int h = Gravity.getAbsoluteGravity(gravity, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK;
		int v = gravity & Gravity.VERTICAL_GRAVITY_MASK;
		native_setGravity(widget, h == Gravity.CENTER_HORIZONTAL ? 1 : h == Gravity.RIGHT ? 2 : 0,
		                  v == Gravity.CENTER_VERTICAL ? 1 : v == Gravity.BOTTOM ? 2 : 0);
	}
	private native void native_setGravity(long widget, int h, int v);

	public int getCompoundPaddingTop() { return 0; }
	public int getCompoundPaddingBottom() { return 0; }

	public CharSequence getText() {
		return text;
	};

	public void setCompoundDrawablePadding(int pad) {}

	protected native void native_setCompoundDrawables(long widget, long left, long top, long right, long bottom);

	// just to prevent garbage collection while native side uses it
	private Drawable drawableLeft = null;
	private Drawable drawableTop = null;
	private Drawable drawableRight = null;
	private Drawable drawableBottom = null;

	public void setCompoundDrawables(Drawable left, Drawable top, Drawable right, Drawable bottom) {
		native_setCompoundDrawables(widget,
		                            left != null ? left.paintable : 0,
		                            top != null ? top.paintable : 0,
		                            right != null ? right.paintable : 0,
		                            bottom != null ? bottom.paintable : 0);
		drawableLeft = left;
		drawableTop = top;
		drawableRight = right;
		drawableBottom = bottom;
	}

	public void setAllCaps(boolean allCaps) {
		String[] classesToRemove = {"ATL-text-uppercase"};
		native_removeClasses(widget, classesToRemove);

		if (allCaps) {
			native_addClass(widget, "ATL-text-uppercase");
		}
	}

	public void setSaveEnabled(boolean enabled) {}

	public final void setAutoLinkMask(int mask) {}

	public void setEditableFactory(Editable.Factory factory) {}

	public KeyListener getKeyListener() { return null; }

	public int getInputType() { return 0; }

	public final void setTransformationMethod(TransformationMethod method) {}

	public InputFilter[] getFilters() { return new InputFilter[0]; }

	public int getMaxLines() { return -1; }

	public void setCompoundDrawablesRelative(Drawable start, Drawable top, Drawable end, Drawable bottom) {
		setCompoundDrawables(start, top, end, bottom);
	}

	public int getLineCount() { return 1; }

	public URLSpan[] getUrls() { return new URLSpan[0]; }

	public void setMovementMethod(MovementMethod method) {}

	public void setTextIsSelectable(boolean selectable) {}

	public MovementMethod getMovementMethod() {
		return new BaseMovementMethod();
	}

	public CharSequence getHint() { return "HINT"; }

	public int getMinHeight() { return 0; }
	public int getMinWidth() { return 0; }
	public void setMinHeight(int minHeight) {}
	public void setMaxHeight(int maxHeight) {}

	public void setHorizontallyScrolling(boolean whether) {}
	public boolean getHorizontallyScrolling() {
		return false;
	}

	public static interface OnEditorActionListener {
		@Export
		public abstract boolean onEditorAction(TextView v, int actionId, KeyEvent event);
	}

	public static enum BufferType {
		EDITABLE,
		NORMAL,
		SPANNABLE,
	}

	public Layout getLayout() {
		return new BoringLayout(getText(), getPaint(), getWidth(), Layout.Alignment.ALIGN_NORMAL, 1, 0, new BoringLayout.Metrics(), false);
	}

	public int getCurrentTextColor() {
		return Color.CYAN;
	}

	public void setSingleLine(boolean singleLine) {}

	public int getCompoundPaddingLeft() { return 0; }

	public int getCompoundPaddingRight() { return 0; }

	public void setHint(int resId) {
		setHint(getContext().getResources().getText(resId));
	}

	public float getLetterSpacing() { return 0.f; }

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(int start, int top, int end, int bottom) {}

	public void setCompoundDrawablesRelativeWithIntrinsicBounds(Drawable start, Drawable top, Drawable end, Drawable bottom) {}

	public boolean getLinksClickable() { return true; }

	public boolean isTextSelectable() { return true; }

	public void setCompoundDrawablesWithIntrinsicBounds(int left, int top, int right, int bottom) {}

	public void setCompoundDrawablesWithIntrinsicBounds(Drawable left, Drawable top, Drawable right, Drawable bottom) {}

	public void setHint(CharSequence s) {}

	public Drawable[] getCompoundDrawablesRelative() { return new Drawable[4]; }

	public Drawable[] getCompoundDrawables() { return new Drawable[4]; }

	public void setTextAppearance(int dummy) {}

	public int length() {
		return getText().length();
	}

	public void setHighlightColor(int color) {}

	public Editable getEditableText() {
		return new SpannableStringBuilder(getText());
	}

	public int getMaxWidth() { return 1000; }

	public void nullLayouts() {}

	public void setLinkTextColor(int color) {}

	public void setCustomSelectionActionModeCallback(ActionMode.Callback actionModeCallback) {}

	public int getExtendedPaddingTop() { return 0; }

	public void setRawInputType(int type) {}

	public TextUtils.TruncateAt getEllipsize() { return null; }

	public void setLines(int lines) {}

	public int getMinLines() {
		return -1;
	}

	public void setMinLines(int lines) {}

	public void setSelectAllOnFocus(boolean selectAllOnFocus) {}

	public int getCompoundDrawablePadding() { return 0; }

	public int getPaintFlags() { return 0; }

	public void setPaintFlags(int flags) {}

	public int getLineHeight() {
		return 10; // FIXME
	}

	public int getMaxHeight() {
		return -1;
	}

	public boolean isAllCaps() { return false; }

	public int getAutoSizeStepGranularity() {
		return -1;
	}

	public void setAutoSizeTextTypeUniformWithPresetSizes(int[] presetSizes, int unit) {}

	public void setCompoundDrawableTintList(ColorStateList tint) {}

	public int getHyphenationFrequency() {
		return hyphenation_frequency;
	}

	public void setHyphenationFrequency(int hyphenationFrequency) {
		hyphenation_frequency = hyphenationFrequency;
	}

	public boolean getIncludeFontPadding() { return include_padding; }

	public void setIncludeFontPadding(boolean includePadding) {
		include_padding = includePadding;
	}

	public float getLineSpacingExtra() { return 0.f; }

	public float getLineSpacingMultiplier() { return 1.f; }

	public TextDirectionHeuristic getTextDirectionHeuristic() {
		return TextDirectionHeuristics.LTR;
	}

	public Bundle getInputExtras(boolean key) {
		return new Bundle();
	}

	public void setError(CharSequence error) {
		System.out.println("ERROR: " + error);
	}

	public int getTotalPaddingLeft() { return 0; }

	public int getTotalPaddingTop() { return 0; }

	public int getTotalPaddingRight() { return 0; }

	public int getTotalPaddingBottom() { return 0; }

	public int getImeOptions() { return 0; }

	public void setShadowLayer(float radius, float dx, float dy, int color) {}

	public int getBreakStrategy() {
		return break_strategy;
	}

	public void setBreakStrategy(int strategy) {
		break_strategy = strategy;
	}

	public void clearComposingText() {}

	public void setKeyListener(KeyListener keyListener) {}

	public int getAutoLinkMask() { return 0; }

	public void setWidth(int width) {}
	public void setHeight(int height) {}

	public void setFreezesText(boolean freezesText) {}

	public void setLetterSpacing(float letterSpacing) {}

	public void setMarqueeRepeatLimit(int marqueeLimit) {}

	public void setShowSoftInputOnFocus(boolean showSoftInputOnFocus) {}

	public void setCustomInsertionActionModeCallback(ActionMode.Callback actionModeCallback) {}

	public int getJustificationMode() {
		return 0; // JUSTIFICATION_MODE_NONE
	}

	public void setImeActionLabel(CharSequence label, int actionId) {}

	public int getExtendedPaddingBottom() { return 0; }

	public boolean hasSelection() { return false; }

	@UnsupportedAppUsage /* androidx ACTVAutoSizeHelper seems to love this */
	/* Copyright (C) 2006 The Android Open Source Project */
	private Layout.Alignment getLayoutAlignment() {
		Layout.Alignment alignment;
		switch (getTextAlignment()) {
			case TEXT_ALIGNMENT_GRAVITY:
				switch (gravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) {
					case Gravity.START:
						alignment = Layout.Alignment.ALIGN_NORMAL;
						break;
					case Gravity.END:
						alignment = Layout.Alignment.ALIGN_OPPOSITE;
						break;
					case Gravity.LEFT:
						alignment = Layout.Alignment.ALIGN_LEFT;
						break;
					case Gravity.RIGHT:
						alignment = Layout.Alignment.ALIGN_RIGHT;
						break;
					case Gravity.CENTER_HORIZONTAL:
						alignment = Layout.Alignment.ALIGN_CENTER;
						break;
					default:
						alignment = Layout.Alignment.ALIGN_NORMAL;
						break;
				}
				break;
			case TEXT_ALIGNMENT_TEXT_START:
				alignment = Layout.Alignment.ALIGN_NORMAL;
				break;
			case TEXT_ALIGNMENT_TEXT_END:
				alignment = Layout.Alignment.ALIGN_OPPOSITE;
				break;
			case TEXT_ALIGNMENT_CENTER:
				alignment = Layout.Alignment.ALIGN_CENTER;
				break;
			case TEXT_ALIGNMENT_VIEW_START:
				alignment = (getLayoutDirection() == LAYOUT_DIRECTION_RTL) ? Layout.Alignment.ALIGN_RIGHT : Layout.Alignment.ALIGN_LEFT;
				break;
			case TEXT_ALIGNMENT_VIEW_END:
				alignment = (getLayoutDirection() == LAYOUT_DIRECTION_RTL) ? Layout.Alignment.ALIGN_LEFT : Layout.Alignment.ALIGN_RIGHT;
				break;
			case TEXT_ALIGNMENT_INHERIT:
				// This should never happen as we have already resolved the text alignment
				// but better safe than sorry so we just fall through
			default:
				alignment = Layout.Alignment.ALIGN_NORMAL;
				break;
		}
		return alignment;
	}

	public void setImeHintLocales(android.os.LocaleList hintLocales) {}

	public void beginBatchEdit() {}

	public void endBatchEdit() {}

	protected void onSelectionChanged(int selStart, int selEnd) {}

	protected void onTextChanged(CharSequence text, int start, int lengthBefore, int lengthAfter) {}

	private java.util.Locale text_locale;

	public void setTextLocale(java.util.Locale locale) {
		text_locale = locale;
	}

	public java.util.Locale getTextLocale() {
		return text_locale != null ? text_locale : java.util.Locale.getDefault();
	}

	public void setTextLocales(android.os.LocaleList locales) {
		text_locale = locales != null && locales.size() > 0 ? locales.get(0) : null;
	}

	private String font_variation_settings;

	public boolean setFontVariationSettings(String fontVariationSettings) {
		font_variation_settings = fontVariationSettings;
		return true;
	}

	public String getFontVariationSettings() {
		return font_variation_settings;
	}

	public int getAutoSizeTextType() {
		return 0;
	}

	public int getAutoSizeMinTextSize() {
		return -1;
	}

	public int getAutoSizeMaxTextSize() {
		return -1;
	}

	public int[] getAutoSizeTextAvailableSizes() {
		return new int[0];
	}

	public void setAutoSizeTextTypeWithDefaults(int autoSizeTextType) {}

	public void setAutoSizeTextTypeUniformWithConfiguration(int minSize, int maxSize, int stepGranularity, int unit) {}

	public ActionMode.Callback getCustomSelectionActionModeCallback() {
		return null;
	}

	public int getHighlightColor() {
		return 0x6633B5E5;
	}

	public int getOffsetForPosition(float x, float y) {
		return 0;
	}

	public boolean hasOnLongClickListeners() {
		return false;
	}

	public boolean isHorizontallyScrollable() {
		return false;
	}

	public void setCompoundDrawableTintMode(android.graphics.PorterDuff.Mode tintMode) {}

	public void setElegantTextHeight(boolean elegant) {}

	public void setFirstBaselineToTopHeight(int firstBaselineToTopHeight) {}

	public void setLastBaselineToBottomHeight(int lastBaselineToBottomHeight) {}

	public void setLineHeight(int unit, float lineHeight) {}

	public void setTextScaleX(float size) {}


	public void setMaxEms(int maxEms) {}

	public void setMinEms(int minEms) {}

	public ColorStateList getHintTextColors() {
		return ColorStateList.valueOf(Color.GRAY);
	}

	public Drawable getTextCursorDrawable() {
		return null;
	}

	public boolean onTextContextMenuItem(int id) {
		return false;
	}


	// GEN member:android.widget.TextView:public int getLineBounds(int, android.graphics.Rect);
	public int getLineBounds(int arg0, android.graphics.Rect arg1) { return 0; }

	// GEN member:android.widget.TextView:public android.view.textclassifier.TextClassifier getTextClassifier();
	public android.view.textclassifier.TextClassifier getTextClassifier() { return null; }

	// GEN member:android.widget.TextView:public android.text.PrecomputedText$Params getTextMetricsParams();
	public android.text.PrecomputedText.Params getTextMetricsParams() { return null; }

	// GEN member:android.widget.TextView:public void setPrivateImeOptions(java.lang.String);
	public void setPrivateImeOptions(java.lang.String arg0) {}

	// GEN member:android.widget.TextView:public void setTextClassifier(android.view.textclassifier.TextClassifier);
	public void setTextClassifier(android.view.textclassifier.TextClassifier arg0) {}
	public boolean onPreDraw() { return true; }

	// GEN member:android.widget.TextView:public int getCompoundPaddingEnd();
	public int getCompoundPaddingEnd() { return 0; }

	// GEN member:android.widget.TextView:public int getCompoundPaddingStart();
	public int getCompoundPaddingStart() { return 0; }

	// GEN member:android.widget.TextView:public int getShadowColor();
	public int getShadowColor() { return 0; }

	// GEN member:android.widget.TextView:public float getShadowDx();
	public float getShadowDx() { return 0; }

	// GEN member:android.widget.TextView:public float getShadowDy();
	public float getShadowDy() { return 0; }

	// GEN member:android.widget.TextView:public float getShadowRadius();
	public float getShadowRadius() { return 0; }

	// GEN member:android.widget.TextView:public android.graphics.drawable.Drawable getTextSelectHandleLeft();
	public android.graphics.drawable.Drawable getTextSelectHandleLeft() { return null; }

	// GEN member:android.widget.TextView:public android.graphics.drawable.Drawable getTextSelectHandleRight();
	public android.graphics.drawable.Drawable getTextSelectHandleRight() { return null; }

	// GEN member:android.widget.TextView:public void setFontFeatureSettings(java.lang.String);
	public void setFontFeatureSettings(java.lang.String arg0) {}

	// GEN member:android.widget.TextView:public void setLineHeight(int);
	public void setLineHeight(int arg0) {}

	// GEN member:android.widget.TextView:public final void setTextKeepState(java.lang.CharSequence);
	public final void setTextKeepState(java.lang.CharSequence arg0) {}

	// GEN member:android.widget.TextView:public void setTextSelectHandleLeft(android.graphics.drawable.Drawable);
	public void setTextSelectHandleLeft(android.graphics.drawable.Drawable arg0) {}

	// GEN member:android.widget.TextView:public void setTextSelectHandleRight(android.graphics.drawable.Drawable);
	public void setTextSelectHandleRight(android.graphics.drawable.Drawable arg0) {}
}
