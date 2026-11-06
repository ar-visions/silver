package android.text;

public interface Editable extends CharSequence, android.text.GetChars, android.text.Spannable, java.lang.Appendable {

	public class Factory {
		public static Factory getInstance() {
			return new Factory();
		}

		public Editable newEditable(CharSequence source) {
			return new SpannableStringBuilder(source);
		}
	}

	public Editable replace(int start, int end, CharSequence source, int destoff, int destlen);

	public Editable replace(int start, int end, CharSequence text);

	public InputFilter[] getFilters();

	public void setFilters(InputFilter[] filters);

	public Editable delete(int start, int end);

	public Editable insert(int where, CharSequence text);

	public void clear();

	public <T> T[] getSpans(int queryStart, int queryEnd, Class<T> kind);

	// GEN member:android.text.Editable:public abstract void clearSpans();
	default void clearSpans() {}

	// GEN member:android.text.Editable:public abstract android.text.Editable append(char);
	default android.text.Editable append(char arg0) { return null; }
}
