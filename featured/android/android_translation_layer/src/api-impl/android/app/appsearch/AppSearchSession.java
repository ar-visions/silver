package android.app.appsearch;

// android.jar's shape, made by gen.py
public final class AppSearchSession implements java.io.Closeable {
	// GEN member:android.app.appsearch.AppSearchSession:public void setSchema(android.app.appsearch.SetSchemaRequest, java.util.concurrent.Executor, java.util.concurrent.Executor, java.util.function.Consumer<android.app.appsearch.AppSearchResult<android.app.appsearch.SetSchemaResponse>>);
	public void setSchema(android.app.appsearch.SetSchemaRequest arg0, java.util.concurrent.Executor arg1, java.util.concurrent.Executor arg2, java.util.function.Consumer<android.app.appsearch.AppSearchResult<android.app.appsearch.SetSchemaResponse>> arg3) {}

	// GEN member:android.app.appsearch.AppSearchSession:public void getSchema(java.util.concurrent.Executor, java.util.function.Consumer<android.app.appsearch.AppSearchResult<android.app.appsearch.GetSchemaResponse>>);
	public void getSchema(java.util.concurrent.Executor arg0, java.util.function.Consumer<android.app.appsearch.AppSearchResult<android.app.appsearch.GetSchemaResponse>> arg1) {}

	// GEN member:android.app.appsearch.AppSearchSession:public void getNamespaces(java.util.concurrent.Executor, java.util.function.Consumer<android.app.appsearch.AppSearchResult<java.util.Set<java.lang.String>>>);
	public void getNamespaces(java.util.concurrent.Executor arg0, java.util.function.Consumer<android.app.appsearch.AppSearchResult<java.util.Set<java.lang.String>>> arg1) {}

	// GEN member:android.app.appsearch.AppSearchSession:public android.app.appsearch.SearchResults search(java.lang.String, android.app.appsearch.SearchSpec);
	public android.app.appsearch.SearchResults search(java.lang.String arg0, android.app.appsearch.SearchSpec arg1) { return null; }

	// GEN member:android.app.appsearch.AppSearchSession:public void remove(java.lang.String, android.app.appsearch.SearchSpec, java.util.concurrent.Executor, java.util.function.Consumer<android.app.appsearch.AppSearchResult<java.lang.Void>>);
	public void remove(java.lang.String arg0, android.app.appsearch.SearchSpec arg1, java.util.concurrent.Executor arg2, java.util.function.Consumer<android.app.appsearch.AppSearchResult<java.lang.Void>> arg3) {}

	// GEN member:android.app.appsearch.AppSearchSession:public void close();
	public void close() {}

	protected AppSearchSession() {}
}
