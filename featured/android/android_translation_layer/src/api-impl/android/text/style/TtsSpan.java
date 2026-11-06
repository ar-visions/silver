package android.text.style;

import android.os.PersistableBundle;

// kept for what reads it; nothing speaks it here
public class TtsSpan implements android.text.ParcelableSpan {
	public static final String TYPE_VERBATIM = "android.type.verbatim";
	public static final String ARG_VERBATIM = "android.arg.verbatim";

	private final String type;
	private final PersistableBundle args;

	public TtsSpan(String type, PersistableBundle args) {
		this.type = type;
		this.args = args;
	}

	public String getType() { return type; }
	public PersistableBundle getArgs() { return args; }

	public static class Builder<C extends Builder<?>> {
		private final String type;
		private final PersistableBundle args = new PersistableBundle();

		public Builder(String type) { this.type = type; }

		@SuppressWarnings("unchecked")
		public C setStringArgument(String arg, String value) {
			args.putString(arg, value);
			return (C)this;
		}

		public TtsSpan build() { return new TtsSpan(type, args); }
	}

	public static class SemioticUnitBuilder<C extends SemioticUnitBuilder<?>> extends Builder<C> {
		public SemioticUnitBuilder(String type) { super(type); }
	}

	public static class VerbatimBuilder extends SemioticUnitBuilder<VerbatimBuilder> {
		public VerbatimBuilder() { super(TYPE_VERBATIM); }

		public VerbatimBuilder(String verbatim) {
			this();
			setVerbatim(verbatim);
		}

		public VerbatimBuilder setVerbatim(String verbatim) {
			return setStringArgument(ARG_VERBATIM, verbatim);
		}
	}

	// GEN class:android.text.style.TtsSpan$SemioticClassBuilder
	public class SemioticClassBuilder<C extends android.text.style.TtsSpan.SemioticClassBuilder<?>> extends android.text.style.TtsSpan.Builder<C> {
		// GEN member:android.text.style.TtsSpan$SemioticClassBuilder:public android.text.style.TtsSpan$SemioticClassBuilder(java.lang.String);
		public SemioticClassBuilder(java.lang.String arg0) { super((java.lang.String)null); }
	
		// GEN member:android.text.style.TtsSpan$SemioticClassBuilder:public C setGender(java.lang.String);
		public C setGender(java.lang.String arg0) { return null; }
	
		// GEN member:android.text.style.TtsSpan$SemioticClassBuilder:public C setAnimacy(java.lang.String);
		public C setAnimacy(java.lang.String arg0) { return null; }
	
		// GEN member:android.text.style.TtsSpan$SemioticClassBuilder:public C setMultiplicity(java.lang.String);
		public C setMultiplicity(java.lang.String arg0) { return null; }
	
		// GEN member:android.text.style.TtsSpan$SemioticClassBuilder:public C setCase(java.lang.String);
		public C setCase(java.lang.String arg0) { return null; }
	
		protected SemioticClassBuilder() { super((java.lang.String)null); }
	}


	// GEN class:android.text.style.TtsSpan$TextBuilder
	public class TextBuilder extends android.text.style.TtsSpan.SemioticClassBuilder<android.text.style.TtsSpan.TextBuilder> {
		// GEN member:android.text.style.TtsSpan$TextBuilder:public android.text.style.TtsSpan$TextBuilder();
		public TextBuilder() {}
	
		// GEN member:android.text.style.TtsSpan$TextBuilder:public android.text.style.TtsSpan$TextBuilder(java.lang.String);
		public TextBuilder(java.lang.String arg0) {}
	
		// GEN member:android.text.style.TtsSpan$TextBuilder:public android.text.style.TtsSpan$TextBuilder setText(java.lang.String);
		public android.text.style.TtsSpan.TextBuilder setText(java.lang.String arg0) { return this; }
	}

}
