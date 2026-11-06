package android.text;

public class TextDirectionHeuristics {

	public static final TextDirectionHeuristic LTR = new TextDirectionHeuristic() {
		@Override
		public boolean isRtl(CharSequence text, int start, int end) {
			return false;
		}
	};

	public static final TextDirectionHeuristic RTL = new TextDirectionHeuristic() {
		@Override
		public boolean isRtl(CharSequence text, int start, int end) {
			return true;
		}
	};

	public static final TextDirectionHeuristic FIRSTSTRONG_LTR = new TextDirectionHeuristic() {
		@Override
		public boolean isRtl(CharSequence text, int start, int end) {
			return false;
		}
	};

	public static final TextDirectionHeuristic FIRSTSTRONG_RTL = new TextDirectionHeuristic() {
		@Override
		public boolean isRtl(CharSequence text, int start, int end) {
			return true;
		}
	};

	// GEN member:android.text.TextDirectionHeuristics:public static final android.text.TextDirectionHeuristic ANYRTL_LTR;
	public static final android.text.TextDirectionHeuristic ANYRTL_LTR = null;

	// GEN member:android.text.TextDirectionHeuristics:public static final android.text.TextDirectionHeuristic LOCALE;
	public static final android.text.TextDirectionHeuristic LOCALE = null;
}
