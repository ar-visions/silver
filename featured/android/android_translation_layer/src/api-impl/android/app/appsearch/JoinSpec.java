package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class JoinSpec implements android.os.Parcelable {
	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_AVG_RANKING_SIGNAL = 3;
	public static final int AGGREGATION_SCORING_AVG_RANKING_SIGNAL = 3;

	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_MAX_RANKING_SIGNAL = 4;
	public static final int AGGREGATION_SCORING_MAX_RANKING_SIGNAL = 4;

	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_MIN_RANKING_SIGNAL = 2;
	public static final int AGGREGATION_SCORING_MIN_RANKING_SIGNAL = 2;

	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_OUTER_RESULT_RANKING_SIGNAL = 0;
	public static final int AGGREGATION_SCORING_OUTER_RESULT_RANKING_SIGNAL = 0;

	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_RESULT_COUNT = 1;
	public static final int AGGREGATION_SCORING_RESULT_COUNT = 1;

	// GEN member:android.app.appsearch.JoinSpec:public static final int AGGREGATION_SCORING_SUM_RANKING_SIGNAL = 5;
	public static final int AGGREGATION_SCORING_SUM_RANKING_SIGNAL = 5;

	// GEN member:android.app.appsearch.JoinSpec:public static final android.os.Parcelable$Creator<android.app.appsearch.JoinSpec> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.JoinSpec> CREATOR = null;

	// GEN member:android.app.appsearch.JoinSpec:public java.lang.String getNestedQuery();
	public java.lang.String getNestedQuery() { return null; }

	// GEN member:android.app.appsearch.JoinSpec:public java.lang.String getChildPropertyExpression();
	public java.lang.String getChildPropertyExpression() { return null; }

	// GEN member:android.app.appsearch.JoinSpec:public int getMaxJoinedResultCount();
	public int getMaxJoinedResultCount() { return 0; }

	// GEN member:android.app.appsearch.JoinSpec:public android.app.appsearch.SearchSpec getNestedSearchSpec();
	public android.app.appsearch.SearchSpec getNestedSearchSpec() { return null; }

	// GEN member:android.app.appsearch.JoinSpec:public int getAggregationScoringStrategy();
	public int getAggregationScoringStrategy() { return 0; }

	// GEN member:android.app.appsearch.JoinSpec:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.JoinSpec:public final int describeContents();
	public final int describeContents() { return 0; }

	protected JoinSpec() {}

	// GEN class:android.app.appsearch.JoinSpec$Builder
	public final class Builder {
		// GEN member:android.app.appsearch.JoinSpec$Builder:public android.app.appsearch.JoinSpec$Builder(java.lang.String);
		public Builder(java.lang.String arg0) {}
	
		// GEN member:android.app.appsearch.JoinSpec$Builder:public android.app.appsearch.JoinSpec$Builder setNestedSearch(java.lang.String, android.app.appsearch.SearchSpec);
		public android.app.appsearch.JoinSpec.Builder setNestedSearch(java.lang.String arg0, android.app.appsearch.SearchSpec arg1) { return this; }
	
		// GEN member:android.app.appsearch.JoinSpec$Builder:public android.app.appsearch.JoinSpec$Builder setMaxJoinedResultCount(int);
		public android.app.appsearch.JoinSpec.Builder setMaxJoinedResultCount(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.JoinSpec$Builder:public android.app.appsearch.JoinSpec$Builder setAggregationScoringStrategy(int);
		public android.app.appsearch.JoinSpec.Builder setAggregationScoringStrategy(int arg0) { return this; }
	
		// GEN member:android.app.appsearch.JoinSpec$Builder:public android.app.appsearch.JoinSpec build();
		public android.app.appsearch.JoinSpec build() { return null; }
	
		protected Builder() {}
	}

}
