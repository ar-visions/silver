package android.content;

import android.atl.ATLLoadedApp;
import android.atl.ATLMediaContentProvider;
import android.atl.ATLProvider;
import android.atl.annotation.Export;
import android.content.pm.PackageParser;
import android.content.pm.ProviderInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.Predicate;

public abstract class ContentProvider implements android.content.ComponentCallbacks2 {

	static final HashMap<String, ATLProvider> atl_providers = new HashMap<>();

	@Export
	static void createContentProviders() {
		atl_providers.put("media", new ATLProvider(new ATLMediaContentProvider()));
		ATLLoadedApp primary = ATLLoadedApp.getPrimaryApplication();
		for (PackageParser.Provider provider_parsed : primary.pkg.providers) {
			String process_name = provider_parsed.info.processName;
			if (process_name != null && process_name.contains(":")) {
				/* NOTE: even if it doesn't contain `:`, if it's not null we probably
				 * need to check what it's requesting; `:` means it wants us to spawn
				 * a new process, which we currently don't support */
				System.out.println("not creating provider " + provider_parsed.className + ", it wants to be started in a new process (" + process_name + ")");
				continue;
			}
			atl_providers.put(provider_parsed.info.authority,
			                  new ATLProvider(primary, provider_parsed));
		}
		// getContentProvider() initializes the content providers,
		// we also opportunistically remove the ones that failed to load
		atl_providers.values().removeIf(new Predicate<ATLProvider>() {
			@Override
			public boolean test(ATLProvider atlProvider) {
				return atlProvider.getContentProvider() == null;
			}
		});
	}

	// apps whose providers others reach: Play Services and the store
	private static final String[] shared_packages = {
	    "com.google.android.gms", "com.android.vending", "com.google.android.gsf"};
	private static boolean shared_loaded;

	static ContentProvider atl_get_content_provider(String authority) {
		if (authority == null)
			return null;
		ATLProvider atlProvider = atl_providers.get(authority);
		if (atlProvider == null && !shared_loaded) {
			shared_loaded = true;
			add_shared_providers();
			atlProvider = atl_providers.get(authority);
		}
		return atlProvider == null ? null : atlProvider.getContentProvider();
	}

	// another installed app's providers, loaded into this process
	private static void add_shared_providers() {
		for (String name : shared_packages) {
			if (!new java.io.File(android.atl.ATLPaths.installed_apks_dir, name + ".apk").isFile())
				continue;
			ATLLoadedApp app = android.atl.ATLLoadedAppManager.getAppFromPackageName(name);
			if (app == null)
				continue;
			for (PackageParser.Provider provider_parsed : app.pkg.providers) {
				if (provider_parsed.info.authority == null)
					continue;
				for (String one : provider_parsed.info.authority.split(";")) {
					if (!atl_providers.containsKey(one))
						atl_providers.put(one, new ATLProvider(app, provider_parsed));
				}
			}
		}
	}

	private Context attached_context;

	public boolean onCreate() { return false; }

	public Context getContext() {
		if (attached_context != null)
			return attached_context;
		return ATLLoadedApp.getPrimaryApplication().getApplication();
	}

	public Bundle call(String method, String arg, Bundle extras) {
		return null;
	}

	public Bundle call(String authority, String method, String arg, Bundle extras) {
		return call(method, arg, extras);
	}

	public abstract Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder);

	public abstract Uri insert(Uri uri, ContentValues values);

	public abstract int update(Uri uri, ContentValues values, String selection, String[] selectionArgs);

	public abstract int delete(Uri uri, String selection, String[] selectionArgs);

	public abstract String getType(Uri uri);

	public abstract ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException;

	public AssetFileDescriptor openAssetFile(Uri uri, String mode) throws FileNotFoundException {
		ParcelFileDescriptor fd = openFile(uri, mode);
		return fd != null ? new AssetFileDescriptor(fd, 0, -1) : null;
	}

	public void attachInfo(Context context, ProviderInfo provider) {
		attached_context = context;
	}

	public String getCallingPackage() {
		return ATLLoadedApp.getPrimaryApplication().pkg.packageName;
	}
}
