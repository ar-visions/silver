package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class SearchResult implements android.os.Parcelable {
	// GEN member:android.app.appsearch.SearchResult:public static final android.os.Parcelable$Creator<android.app.appsearch.SearchResult> CREATOR;
	public static final android.os.Parcelable.Creator<android.app.appsearch.SearchResult> CREATOR = null;

	// GEN member:android.app.appsearch.SearchResult:public android.app.appsearch.GenericDocument getGenericDocument();
	public android.app.appsearch.GenericDocument getGenericDocument() { return null; }

	// GEN member:android.app.appsearch.SearchResult:public java.util.List<android.app.appsearch.SearchResult$MatchInfo> getMatchInfos();
	public java.util.List<android.app.appsearch.SearchResult.MatchInfo> getMatchInfos() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SearchResult:public java.lang.String getPackageName();
	public java.lang.String getPackageName() { return null; }

	// GEN member:android.app.appsearch.SearchResult:public java.lang.String getDatabaseName();
	public java.lang.String getDatabaseName() { return null; }

	// GEN member:android.app.appsearch.SearchResult:public double getRankingSignal();
	public double getRankingSignal() { return 0; }

	// GEN member:android.app.appsearch.SearchResult:public java.util.List<android.app.appsearch.SearchResult> getJoinedResults();
	public java.util.List<android.app.appsearch.SearchResult> getJoinedResults() { return new java.util.ArrayList<>(); }

	// GEN member:android.app.appsearch.SearchResult:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.app.appsearch.SearchResult:public final int describeContents();
	public final int describeContents() { return 0; }

	protected SearchResult() {}

	// GEN class:android.app.appsearch.SearchResult$MatchInfo
	public final class MatchInfo implements android.os.Parcelable {
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public java.lang.String getPropertyPath();
		public java.lang.String getPropertyPath() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public java.lang.String getFullText();
		public java.lang.String getFullText() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public android.app.appsearch.SearchResult$MatchRange getExactMatchRange();
		public android.app.appsearch.SearchResult.MatchRange getExactMatchRange() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public java.lang.CharSequence getExactMatch();
		public java.lang.CharSequence getExactMatch() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public android.app.appsearch.SearchResult$MatchRange getSubmatchRange();
		public android.app.appsearch.SearchResult.MatchRange getSubmatchRange() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public java.lang.CharSequence getSubmatch();
		public java.lang.CharSequence getSubmatch() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public android.app.appsearch.SearchResult$MatchRange getSnippetRange();
		public android.app.appsearch.SearchResult.MatchRange getSnippetRange() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public java.lang.CharSequence getSnippet();
		public java.lang.CharSequence getSnippet() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		// GEN member:android.app.appsearch.SearchResult$MatchInfo:public final int describeContents();
		public final int describeContents() { return 0; }
	
		protected MatchInfo() {}
	}


	// GEN class:android.app.appsearch.SearchResult$MatchRange
	public final class MatchRange {
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public android.app.appsearch.SearchResult$MatchRange(int, int);
		public MatchRange(int arg0, int arg1) {}
	
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public int getStart();
		public int getStart() { return 0; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public int getEnd();
		public int getEnd() { return 0; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public boolean equals(java.lang.Object);
		public boolean equals(java.lang.Object arg0) { return false; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		// GEN member:android.app.appsearch.SearchResult$MatchRange:public int hashCode();
		public int hashCode() { return 0; }
	
		protected MatchRange() {}
	}

}
