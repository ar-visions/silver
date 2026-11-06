package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class SearchSpec implements android.os.Parcelable {
	// GEN member:android.app.appsearch.SearchSpec:public static final android.os.Parcelable$Creator<android.app.appsearch.SearchSpec> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.SearchSpec> CREATOR = null;

	// GEN member:android.app.appsearch.SearchSpec:public static final int GROUPING_TYPE_PER_NAMESPACE = 2;
	public static final int GROUPING_TYPE_PER_NAMESPACE = 2;

	// GEN member:android.app.appsearch.SearchSpec:public static final int GROUPING_TYPE_PER_PACKAGE = 1;
	public static final int GROUPING_TYPE_PER_PACKAGE = 1;

	// GEN member:android.app.appsearch.SearchSpec:public static final int GROUPING_TYPE_PER_SCHEMA = 4;
	public static final int GROUPING_TYPE_PER_SCHEMA = 4;

	// GEN member:android.app.appsearch.SearchSpec:public static final int ORDER_ASCENDING = 1;
	public static final int ORDER_ASCENDING = 1;

	// GEN member:android.app.appsearch.SearchSpec:public static final int ORDER_DESCENDING = 0;
	public static final int ORDER_DESCENDING = 0;

	// GEN member:android.app.appsearch.SearchSpec:public static final java.lang.String PROJECTION_SCHEMA_TYPE_WILDCARD = "*";
	public static final java.lang.String PROJECTION_SCHEMA_TYPE_WILDCARD = "*";

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_ADVANCED_RANKING_EXPRESSION = 9;
	public static final int RANKING_STRATEGY_ADVANCED_RANKING_EXPRESSION = 9;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_CREATION_TIMESTAMP = 2;
	public static final int RANKING_STRATEGY_CREATION_TIMESTAMP = 2;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_DOCUMENT_SCORE = 1;
	public static final int RANKING_STRATEGY_DOCUMENT_SCORE = 1;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_JOIN_AGGREGATE_SCORE = 8;
	public static final int RANKING_STRATEGY_JOIN_AGGREGATE_SCORE = 8;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_NONE = 0;
	public static final int RANKING_STRATEGY_NONE = 0;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_RELEVANCE_SCORE = 3;
	public static final int RANKING_STRATEGY_RELEVANCE_SCORE = 3;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_SYSTEM_USAGE_COUNT = 6;
	public static final int RANKING_STRATEGY_SYSTEM_USAGE_COUNT = 6;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_SYSTEM_USAGE_LAST_USED_TIMESTAMP = 7;
	public static final int RANKING_STRATEGY_SYSTEM_USAGE_LAST_USED_TIMESTAMP = 7;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_USAGE_COUNT = 4;
	public static final int RANKING_STRATEGY_USAGE_COUNT = 4;

	// GEN member:android.app.appsearch.SearchSpec:public static final int RANKING_STRATEGY_USAGE_LAST_USED_TIMESTAMP = 5;
	public static final int RANKING_STRATEGY_USAGE_LAST_USED_TIMESTAMP = 5;

	// GEN member:android.app.appsearch.SearchSpec:public static final java.lang.String SCHEMA_TYPE_WILDCARD = "*";
	public static final java.lang.String SCHEMA_TYPE_WILDCARD = "*";

	// GEN member:android.app.appsearch.SearchSpec:public static final int TERM_MATCH_EXACT_ONLY = 1;
	public static final int TERM_MATCH_EXACT_ONLY = 1;

	// GEN member:android.app.appsearch.SearchSpec:public static final int TERM_MATCH_PREFIX = 2;
	public static final int TERM_MATCH_PREFIX = 2;

	// GEN member:android.app.appsearch.SearchSpec:public int getTermMatch();
	public int getTermMatch() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.List<java.lang.String> getFilterSchemas();
	public java.util.List<java.lang.String> getFilterSchemas() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getFilterProperties();
	public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getFilterProperties() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.List<java.lang.String> getFilterNamespaces();
	public java.util.List<java.lang.String> getFilterNamespaces() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.List<java.lang.String> getFilterPackageNames();
	public java.util.List<java.lang.String> getFilterPackageNames() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public int getResultCountPerPage();
	public int getResultCountPerPage() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getRankingStrategy();
	public int getRankingStrategy() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getOrder();
	public int getOrder() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getSnippetCount();
	public int getSnippetCount() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getSnippetCountPerProperty();
	public int getSnippetCountPerProperty() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getMaxSnippetSize();
	public int getMaxSnippetSize() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getProjections();
	public java.util.Map<java.lang.String, java.util.List<java.lang.String>> getProjections() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public java.util.Map<java.lang.String, java.util.Map<java.lang.String, java.lang.Double>> getPropertyWeights();
	public java.util.Map<java.lang.String, java.util.Map<java.lang.String, java.lang.Double>> getPropertyWeights() { return new java.util.HashMap<>(); }

	// GEN member:android.app.appsearch.SearchSpec:public int getResultGroupingTypeFlags();
	public int getResultGroupingTypeFlags() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public int getResultGroupingLimit();
	public int getResultGroupingLimit() { return 0; }

	// GEN member:android.app.appsearch.SearchSpec:public android.app.appsearch.JoinSpec getJoinSpec();
	public android.app.appsearch.JoinSpec getJoinSpec() { return null; }

	// GEN member:android.app.appsearch.SearchSpec:public java.lang.String getAdvancedRankingExpression();
	public java.lang.String getAdvancedRankingExpression() { return null; }

	// GEN member:android.app.appsearch.SearchSpec:public java.lang.String getSearchSourceLogTag();
	public java.lang.String getSearchSourceLogTag() { return null; }

	// GEN member:android.app.appsearch.SearchSpec:public boolean isNumericSearchEnabled();
	public boolean isNumericSearchEnabled() { return false; }

	// GEN member:android.app.appsearch.SearchSpec:public boolean isVerbatimSearchEnabled();
	public boolean isVerbatimSearchEnabled() { return false; }

	// GEN member:android.app.appsearch.SearchSpec:public boolean isListFilterQueryLanguageEnabled();
	public boolean isListFilterQueryLanguageEnabled() { return false; }

	// GEN member:android.app.appsearch.SearchSpec:public boolean isListFilterHasPropertyFunctionEnabled();
	public boolean isListFilterHasPropertyFunctionEnabled() { return false; }

	// GEN member:android.app.appsearch.SearchSpec:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.SearchSpec:public final int describeContents();
	public final int describeContents() { return 0; }

	protected SearchSpec() {}

	// GEN class:android.app.appsearch.SearchSpec$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder();
		public Builder() {}
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setTermMatch(int);
		public android.app.appsearch.SearchSpec.Builder setTermMatch(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterSchemas(java.lang.String...);
		public android.app.appsearch.SearchSpec.Builder addFilterSchemas(java.lang.String... arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterSchemas(java.util.Collection<java.lang.String>);
		public android.app.appsearch.SearchSpec.Builder addFilterSchemas(java.util.Collection<java.lang.String> arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterProperties(java.lang.String, java.util.Collection<java.lang.String>);
		public android.app.appsearch.SearchSpec.Builder addFilterProperties(java.lang.String arg0, java.util.Collection<java.lang.String> arg1) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterNamespaces(java.lang.String...);
		public android.app.appsearch.SearchSpec.Builder addFilterNamespaces(java.lang.String... arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterNamespaces(java.util.Collection<java.lang.String>);
		public android.app.appsearch.SearchSpec.Builder addFilterNamespaces(java.util.Collection<java.lang.String> arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterPackageNames(java.lang.String...);
		public android.app.appsearch.SearchSpec.Builder addFilterPackageNames(java.lang.String... arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addFilterPackageNames(java.util.Collection<java.lang.String>);
		public android.app.appsearch.SearchSpec.Builder addFilterPackageNames(java.util.Collection<java.lang.String> arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setResultCountPerPage(int);
		public android.app.appsearch.SearchSpec.Builder setResultCountPerPage(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setRankingStrategy(int);
		public android.app.appsearch.SearchSpec.Builder setRankingStrategy(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setRankingStrategy(java.lang.String);
		public android.app.appsearch.SearchSpec.Builder setRankingStrategy(java.lang.String arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setSearchSourceLogTag(java.lang.String);
		public android.app.appsearch.SearchSpec.Builder setSearchSourceLogTag(java.lang.String arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setOrder(int);
		public android.app.appsearch.SearchSpec.Builder setOrder(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setSnippetCount(int);
		public android.app.appsearch.SearchSpec.Builder setSnippetCount(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setSnippetCountPerProperty(int);
		public android.app.appsearch.SearchSpec.Builder setSnippetCountPerProperty(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setMaxSnippetSize(int);
		public android.app.appsearch.SearchSpec.Builder setMaxSnippetSize(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder addProjection(java.lang.String, java.util.Collection<java.lang.String>);
		public android.app.appsearch.SearchSpec.Builder addProjection(java.lang.String arg0, java.util.Collection<java.lang.String> arg1) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setResultGrouping(int, int);
		public android.app.appsearch.SearchSpec.Builder setResultGrouping(int arg0, int arg1) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setPropertyWeights(java.lang.String, java.util.Map<java.lang.String, java.lang.Double>);
		public android.app.appsearch.SearchSpec.Builder setPropertyWeights(java.lang.String arg0, java.util.Map<java.lang.String, java.lang.Double> arg1) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setJoinSpec(android.app.appsearch.JoinSpec);
		public android.app.appsearch.SearchSpec.Builder setJoinSpec(android.app.appsearch.JoinSpec arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setNumericSearchEnabled(boolean);
		public android.app.appsearch.SearchSpec.Builder setNumericSearchEnabled(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setVerbatimSearchEnabled(boolean);
		public android.app.appsearch.SearchSpec.Builder setVerbatimSearchEnabled(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setListFilterQueryLanguageEnabled(boolean);
		public android.app.appsearch.SearchSpec.Builder setListFilterQueryLanguageEnabled(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec$Builder setListFilterHasPropertyFunctionEnabled(boolean);
		public android.app.appsearch.SearchSpec.Builder setListFilterHasPropertyFunctionEnabled(boolean arg0) { return this; }
	
		// GEN member:android.app.appsearch.SearchSpec$Builder:public android.app.appsearch.SearchSpec build();
		public android.app.appsearch.SearchSpec build() { return null; }
	}

}
