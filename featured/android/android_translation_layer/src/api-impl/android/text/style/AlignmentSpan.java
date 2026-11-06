package android.text.style;

import android.text.Layout;

public interface AlignmentSpan extends ParagraphStyle {
	Layout.Alignment getAlignment();

	public static class Standard implements AlignmentSpan, android.text.ParcelableSpan {
		private final Layout.Alignment alignment;

		public Standard(Layout.Alignment alignment) { this.alignment = alignment; }

		public Layout.Alignment getAlignment() { return alignment; }
	}
}
