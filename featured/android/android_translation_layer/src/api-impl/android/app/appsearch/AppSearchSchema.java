package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class AppSearchSchema implements android.os.Parcelable {
	// GEN member:android.app.appsearch.AppSearchSchema:public static final android.os.Parcelable$Creator<android.app.appsearch.AppSearchSchema> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.AppSearchSchema> CREATOR = null;

	// GEN member:android.app.appsearch.AppSearchSchema:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.app.appsearch.AppSearchSchema:public java.lang.String getSchemaType();
	public java.lang.String getSchemaType() { return null; }

	// GEN member:android.app.appsearch.AppSearchSchema:public java.util.List<android.app.appsearch.AppSearchSchema$PropertyConfig> getProperties();
	public java.util.List<android.app.appsearch.AppSearchSchema.PropertyConfig> getProperties() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.AppSearchSchema:public java.util.List<java.lang.String> getParentTypes();
	public java.util.List<java.lang.String> getParentTypes() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.AppSearchSchema:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	// GEN member:android.app.appsearch.AppSearchSchema:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.app.appsearch.AppSearchSchema:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.AppSearchSchema:public final int describeContents();
	public final int describeContents() { return 0; }

	protected AppSearchSchema() {}

	// GEN class:android.app.appsearch.AppSearchSchema$BooleanPropertyConfig
	public final class BooleanPropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		protected BooleanPropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.BooleanPropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$BooleanPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BooleanPropertyConfig build();
		public android.app.appsearch.AppSearchSchema.BooleanPropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.app.appsearch.AppSearchSchema$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$Builder:public android.app.appsearch.AppSearchSchema$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$Builder:public android.app.appsearch.AppSearchSchema$Builder addProperty(android.app.appsearch.AppSearchSchema$PropertyConfig);
		public android.app.appsearch.AppSearchSchema.Builder addProperty(android.app.appsearch.AppSearchSchema.PropertyConfig arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$Builder:public android.app.appsearch.AppSearchSchema$Builder addParentType(java.lang.String);
		public android.app.appsearch.AppSearchSchema.Builder addParentType(java.lang.String arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$Builder:public android.app.appsearch.AppSearchSchema build();
		public android.app.appsearch.AppSearchSchema build() { return null; }
	
		protected Builder() {}
	}


	// GEN class:android.app.appsearch.AppSearchSchema$BytesPropertyConfig
	public final class BytesPropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		protected BytesPropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.BytesPropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$BytesPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$BytesPropertyConfig build();
		public android.app.appsearch.AppSearchSchema.BytesPropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig
	public final class DocumentPropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig:public java.lang.String getSchemaType();
		public java.lang.String getSchemaType() { return null; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig:public boolean shouldIndexNestedProperties();
		public boolean shouldIndexNestedProperties() { return false; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig:public java.util.List<java.lang.String> getIndexableNestedProperties();
		public java.util.List<java.lang.String> getIndexableNestedProperties() { return new java.util.ArrayList<>(); }
	
		protected DocumentPropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder(java.lang.String, java.lang.String);
		public Builder(java.lang.String arg0, java.lang.String arg1) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.DocumentPropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder setShouldIndexNestedProperties(boolean);
		public android.app.appsearch.AppSearchSchema.DocumentPropertyConfig.Builder setShouldIndexNestedProperties(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder addIndexableNestedProperties(java.lang.String...);
		public android.app.appsearch.AppSearchSchema.DocumentPropertyConfig.Builder addIndexableNestedProperties(java.lang.String... arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder addIndexableNestedProperties(java.util.Collection<java.lang.String>);
		public android.app.appsearch.AppSearchSchema.DocumentPropertyConfig.Builder addIndexableNestedProperties(java.util.Collection<java.lang.String> arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DocumentPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DocumentPropertyConfig build();
		public android.app.appsearch.AppSearchSchema.DocumentPropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.app.appsearch.AppSearchSchema$DoublePropertyConfig
	public final class DoublePropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		protected DoublePropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.DoublePropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$DoublePropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$DoublePropertyConfig build();
		public android.app.appsearch.AppSearchSchema.DoublePropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.app.appsearch.AppSearchSchema$LongPropertyConfig
	public final class LongPropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig:public static final int INDEXING_TYPE_NONE = 0;
		public static final int INDEXING_TYPE_NONE = 0;
	
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig:public static final int INDEXING_TYPE_RANGE = 1;
		public static final int INDEXING_TYPE_RANGE = 1;
	
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig:public int getIndexingType();
		public int getIndexingType() { return 0; }
	
		protected LongPropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.LongPropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder setIndexingType(int);
		public android.app.appsearch.AppSearchSchema.LongPropertyConfig.Builder setIndexingType(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$LongPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$LongPropertyConfig build();
		public android.app.appsearch.AppSearchSchema.LongPropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.app.appsearch.AppSearchSchema$PropertyConfig
	public abstract class PropertyConfig {
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public static final int CARDINALITY_OPTIONAL = 2;
		public static final int CARDINALITY_OPTIONAL = 2;
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public static final int CARDINALITY_REPEATED = 1;
		public static final int CARDINALITY_REPEATED = 1;
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public static final int CARDINALITY_REQUIRED = 3;
		public static final int CARDINALITY_REQUIRED = 3;
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public java.lang.String getName();
		public java.lang.String getName() { return null; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public int getCardinality();
		public int getCardinality() { return 0; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$PropertyConfig:public int hashCode();
		public int hashCode() { return 0; }
	
		protected PropertyConfig() {}
	}


	// GEN class:android.app.appsearch.AppSearchSchema$StringPropertyConfig
	public final class StringPropertyConfig extends android.app.appsearch.AppSearchSchema.PropertyConfig {
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int INDEXING_TYPE_EXACT_TERMS = 1;
		public static final int INDEXING_TYPE_EXACT_TERMS = 1;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int INDEXING_TYPE_NONE = 0;
		public static final int INDEXING_TYPE_NONE = 0;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int INDEXING_TYPE_PREFIXES = 2;
		public static final int INDEXING_TYPE_PREFIXES = 2;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int JOINABLE_VALUE_TYPE_NONE = 0;
		public static final int JOINABLE_VALUE_TYPE_NONE = 0;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int JOINABLE_VALUE_TYPE_QUALIFIED_ID = 1;
		public static final int JOINABLE_VALUE_TYPE_QUALIFIED_ID = 1;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int TOKENIZER_TYPE_NONE = 0;
		public static final int TOKENIZER_TYPE_NONE = 0;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int TOKENIZER_TYPE_PLAIN = 1;
		public static final int TOKENIZER_TYPE_PLAIN = 1;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int TOKENIZER_TYPE_RFC822 = 3;
		public static final int TOKENIZER_TYPE_RFC822 = 3;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public static final int TOKENIZER_TYPE_VERBATIM = 2;
		public static final int TOKENIZER_TYPE_VERBATIM = 2;
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public int getIndexingType();
		public int getIndexingType() { return 0; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public int getTokenizerType();
		public int getTokenizerType() { return 0; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig:public int getJoinableValueType();
		public int getJoinableValueType() { return 0; }
	
		protected StringPropertyConfig() {}
	
	// GEN class:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder setCardinality(int);
		public android.app.appsearch.AppSearchSchema.StringPropertyConfig.Builder setCardinality(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder setIndexingType(int);
		public android.app.appsearch.AppSearchSchema.StringPropertyConfig.Builder setIndexingType(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder setTokenizerType(int);
		public android.app.appsearch.AppSearchSchema.StringPropertyConfig.Builder setTokenizerType(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder setJoinableValueType(int);
		public android.app.appsearch.AppSearchSchema.StringPropertyConfig.Builder setJoinableValueType(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.AppSearchSchema$StringPropertyConfig$Builder:public android.app.appsearch.AppSearchSchema$StringPropertyConfig build();
		public android.app.appsearch.AppSearchSchema.StringPropertyConfig build() { return null; }
	
		protected Builder() {}
	}

}

}
