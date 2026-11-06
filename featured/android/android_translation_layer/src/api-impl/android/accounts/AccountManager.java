package android.accounts;

import android.app.Activity;
import android.atl.ATLPaths;
import android.content.Context;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/*
 * Accounts are kept in ATL's data folder, shared by every app run
 * there as on Android: accounts/<type>/<name>/<key>, one file a
 * value (key: account, password, userdata:<k>, authtoken:<t>,
 * visibility:<package>), each name %-encoded.
 */
public class AccountManager {

	public static final int VISIBILITY_UNDEFINED = 0;
	public static final int VISIBILITY_VISIBLE = 1;
	public static final int VISIBILITY_USER_MANAGED_VISIBLE = 2;
	public static final int VISIBILITY_NOT_VISIBLE = 3;
	public static final int VISIBILITY_USER_MANAGED_NOT_VISIBLE = 4;

	public static final int ERROR_CODE_REMOTE_EXCEPTION = 1;
	public static final int ERROR_CODE_NETWORK_ERROR = 3;
	public static final int ERROR_CODE_CANCELED = 4;
	public static final int ERROR_CODE_INVALID_RESPONSE = 5;
	public static final int ERROR_CODE_UNSUPPORTED_OPERATION = 6;
	public static final int ERROR_CODE_BAD_ARGUMENTS = 7;
	public static final int ERROR_CODE_BAD_REQUEST = 8;
	public static final int ERROR_CODE_BAD_AUTHENTICATION = 9;

	public static final String KEY_ACCOUNTS = "accounts";
	public static final String KEY_AUTHENTICATOR_TYPES = "authenticator_types";
	public static final String KEY_USERDATA = "userdata";
	public static final String KEY_AUTHTOKEN = "authtoken";
	public static final String KEY_PASSWORD = "password";
	public static final String KEY_ACCOUNT_NAME = "authAccount";
	public static final String KEY_ACCOUNT_TYPE = "accountType";
	public static final String KEY_ERROR_CODE = "errorCode";
	public static final String KEY_ERROR_MESSAGE = "errorMessage";
	public static final String KEY_INTENT = "intent";
	public static final String KEY_ACCOUNT_AUTHENTICATOR_RESPONSE = "accountAuthenticatorResponse";
	public static final String KEY_ACCOUNT_MANAGER_RESPONSE = "accountManagerResponse";
	public static final String KEY_BOOLEAN_RESULT = "booleanResult";
	public static final String KEY_CALLER_UID = "callerUid";
	public static final String KEY_CALLER_PID = "callerPid";
	public static final String KEY_ANDROID_PACKAGE_NAME = "androidPackageName";
	public static final String KEY_NOTIFY_ON_FAILURE = "notifyOnAuthFailure";
	public static final String KEY_LAST_AUTHENTICATED_TIME = "lastAuthenticatedTime";
	public static final String ACTION_AUTHENTICATOR_INTENT = "android.accounts.AccountAuthenticator";
	public static final String AUTHENTICATOR_META_DATA_NAME = "android.accounts.AccountAuthenticator";
	public static final String AUTHENTICATOR_ATTRIBUTES_NAME = "account-authenticator";
	public static final String LOGIN_ACCOUNTS_CHANGED_ACTION = "android.accounts.LOGIN_ACCOUNTS_CHANGED";
	public static final String ACTION_ACCOUNT_REMOVED = "android.accounts.action.ACCOUNT_REMOVED";
	public static final String PACKAGE_NAME_KEY_LEGACY_VISIBLE = "android:accounts:key_legacy_visible";
	public static final String PACKAGE_NAME_KEY_LEGACY_NOT_VISIBLE = "android:accounts:key_legacy_not_visible";

	private static final AccountManager instance = new AccountManager();
	private final HashMap<OnAccountsUpdateListener, Handler> listeners = new HashMap<>();

	public static AccountManager get(Context context) {
		return instance;
	}

	// %-encoded, so a name is one plain file name
	private static String enc(String s) {
		StringBuilder b = new StringBuilder();
		for (byte c : s.getBytes(StandardCharsets.UTF_8)) {
			int u = c & 0xff;
			if (u < 0x21 || u > 0x7e || u == '%' || u == '/')
				b.append(String.format("%%%02X", u));
			else
				b.append((char)u);
		}
		return b.toString();
	}

	private static String dec(String s) {
		byte[] out = new byte[s.length()];
		int n = 0;
		for (int i = 0; i < s.length(); i++) {
			char c = s.charAt(i);
			if (c == '%' && i + 2 < s.length()) {
				out[n++] = (byte)Integer.parseInt(s.substring(i + 1, i + 3), 16);
				i += 2;
			} else {
				out[n++] = (byte)c;
			}
		}
		return new String(out, 0, n, StandardCharsets.UTF_8);
	}

	private static File root() {
		return new File(ATLPaths.app_data_dir_base, "accounts");
	}

	private static File folder(String type, String name) {
		return new File(new File(root(), enc(type)), enc(name));
	}

	private static synchronized String readValue(String type, String name, String key) {
		File f = new File(folder(type, name), enc(key));
		try {
			return f.isFile() ? new String(Files.readAllBytes(f.toPath()), StandardCharsets.UTF_8) : null;
		} catch (IOException e) {
			return null;
		}
	}

	// value null removes it; written whole, then renamed in
	private static synchronized boolean writeValue(String type, String name, String key, String value) {
		File dir = folder(type, name);
		File f = new File(dir, enc(key));
		if (value == null)
			return !f.exists() || f.delete();
		if (!dir.isDirectory() && !dir.mkdirs())
			return false;
		File part = new File(dir, enc(key) + ".part");
		try (FileOutputStream out = new FileOutputStream(part)) {
			out.write(value.getBytes(StandardCharsets.UTF_8));
		} catch (IOException e) {
			return false;
		}
		part.setReadable(false, false);
		part.setReadable(true, true);
		part.setWritable(false, false);
		part.setWritable(true, true);
		return part.renameTo(f);
	}

	private static synchronized boolean removeFolder(String type, String name) {
		File dir = folder(type, name);
		if (!dir.isDirectory())
			return false;
		File[] files = dir.listFiles();
		if (files != null)
			for (File f : files)
				f.delete();
		return dir.delete();
	}

	public Account[] getAccounts() {
		return getAccountsByType(null);
	}

	public Account[] getAccountsByType(String type) {
		ArrayList<Account> accounts = new ArrayList<>();
		File[] types = root().listFiles();
		if (types == null)
			return new Account[0];
		for (File t : types) {
			String accountType = dec(t.getName());
			if (type != null && !type.equals(accountType))
				continue;
			File[] names = t.listFiles();
			if (names == null)
				continue;
			for (File n : names)
				if (new File(n, "account").isFile())
					accounts.add(new Account(dec(n.getName()), accountType));
		}
		return accounts.toArray(new Account[0]);
	}

	public Account[] getAccountsByTypeForPackage(String type, String packageName) {
		return getAccountsByType(type);
	}

	public AccountManagerFuture<Account[]> getAccountsByTypeAndFeatures(final String type, String[] features, AccountManagerCallback<Account[]> callback, Handler handler) {
		return runAsync(new Callable<Account[]>() {
			@Override
			public Account[] call() {
				return getAccountsByType(type);
			}
		}, callback, handler);
	}

	public int getAccountVisibility(Account account, String packageName) {
		String v = readValue(account.type, account.name, "visibility:" + packageName);
		return v == null ? VISIBILITY_VISIBLE : Integer.parseInt(v.trim());
	}

	public boolean setAccountVisibility(Account account, String packageName, int visibility) {
		if (readValue(account.type, account.name, "account") == null)
			return false;
		return writeValue(account.type, account.name, "visibility:" + packageName, String.valueOf(visibility));
	}

	public Map<String, Integer> getPackagesAndVisibilityForAccount(Account account) {
		return new HashMap<String, Integer>();
	}

	public Map<Account, Integer> getAccountsAndVisibilityForPackage(String packageName, String accountType) {
		HashMap<Account, Integer> result = new HashMap<Account, Integer>();
		for (Account acc : getAccountsByType(accountType))
			result.put(acc, getAccountVisibility(acc, packageName));
		return result;
	}

	public boolean addAccountExplicitly(Account account, String password, Bundle extras) {
		if (readValue(account.type, account.name, "account") != null)
			return false;
		if (!writeValue(account.type, account.name, "account", ""))
			return false;
		setPassword(account, password);
		if (extras != null) {
			for (String key : extras.keySet())
				setUserData(account, key, extras.getString(key));
		}
		changed();
		return true;
	}

	public boolean addAccountExplicitly(Account account, String password, Bundle extras, Map<String, Integer> visibility) {
		if (!addAccountExplicitly(account, password, extras))
			return false;
		if (visibility != null)
			for (Map.Entry<String, Integer> e : visibility.entrySet())
				setAccountVisibility(account, e.getKey(), e.getValue());
		return true;
	}

	public boolean removeAccountExplicitly(Account account) {
		boolean gone = removeFolder(account.type, account.name);
		if (gone)
			changed();
		return gone;
	}

	public AccountManagerFuture<Boolean> removeAccount(final Account account, AccountManagerCallback<Boolean> callback, Handler handler) {
		// no lambda here, dx can't handle invokedynamic for our min sdk version
		return runAsync(new Callable<Boolean>() {
			@Override
			public Boolean call() {
				return removeAccountExplicitly(account);
			}
		}, callback, handler);
	}

	public AccountManagerFuture<Bundle> removeAccount(final Account account, Activity activity, AccountManagerCallback<Bundle> callback, Handler handler) {
		return runAsync(new Callable<Bundle>() {
			@Override
			public Bundle call() {
				Bundle b = new Bundle();
				b.putBoolean(KEY_BOOLEAN_RESULT, removeAccountExplicitly(account));
				return b;
			}
		}, callback, handler);
	}

	/*
	 * Runs work off the calling thread like AOSP does, so file calls don't block the UI.
	 * The callback is invoked on the handler's thread (main thread if null) once the work is done.
	 */
	private static <V> AccountManagerFuture<V> runAsync(Callable<V> work, final AccountManagerCallback<V> callback, final Handler handler) {
		class Future extends FutureTask<V> implements AccountManagerFuture<V> {
			Future() {
				super(work);
			}

			@Override
			protected void done() {
				if (callback == null)
					return;
				(handler != null ? handler : new Handler(Looper.getMainLooper())).post(new Runnable() {
					@Override
					public void run() {
						callback.run(Future.this);
					}
				});
			}

			public V getResult() {
				try {
					return get();
				} catch (InterruptedException | ExecutionException e) {
					throw new RuntimeException(e);
				}
			}

			public V getResult(long timeout, TimeUnit unit) {
				try {
					return get(timeout, unit);
				} catch (InterruptedException | ExecutionException | TimeoutException e) {
					throw new RuntimeException(e);
				}
			}
		}
		Future future = new Future();
		AsyncTask.THREAD_POOL_EXECUTOR.execute(future);
		return future;
	}

	public String getPassword(Account account) {
		return readValue(account.type, account.name, "password");
	}

	public void setPassword(Account account, String password) {
		writeValue(account.type, account.name, "password", password);
	}

	public void clearPassword(Account account) {
		setPassword(account, null);
	}

	public String getUserData(Account account, String key) {
		return readValue(account.type, account.name, "userdata:" + key);
	}

	public void setUserData(Account account, String key, String value) {
		writeValue(account.type, account.name, "userdata:" + key, value);
	}

	public String peekAuthToken(Account account, String authTokenType) {
		return readValue(account.type, account.name, "authtoken:" + authTokenType);
	}

	public void setAuthToken(Account account, String authTokenType, String authToken) {
		writeValue(account.type, account.name, "authtoken:" + authTokenType, authToken);
	}

	// every stored token equal to it, for accounts of the type
	public void invalidateAuthToken(String accountType, String authToken) {
		if (authToken == null)
			return;
		for (Account account : getAccountsByType(accountType)) {
			File[] files = folder(account.type, account.name).listFiles();
			if (files == null)
				continue;
			for (File f : files) {
				String key = dec(f.getName());
				if (key.startsWith("authtoken:") && authToken.equals(readValue(account.type, account.name, key)))
					writeValue(account.type, account.name, key, null);
			}
		}
	}

	// a stored token, else the authenticator is not reachable here
	private Bundle tokenResult(Account account, String authTokenType) {
		Bundle b = new Bundle();
		String token = peekAuthToken(account, authTokenType);
		if (token != null) {
			b.putString(KEY_ACCOUNT_NAME, account.name);
			b.putString(KEY_ACCOUNT_TYPE, account.type);
			b.putString(KEY_AUTHTOKEN, token);
		} else {
			b.putInt(KEY_ERROR_CODE, ERROR_CODE_UNSUPPORTED_OPERATION);
			b.putString(KEY_ERROR_MESSAGE, "no stored token for " + authTokenType);
		}
		return b;
	}

	public AccountManagerFuture<Bundle> getAuthToken(final Account account, final String authTokenType, Bundle options, Activity activity, AccountManagerCallback<Bundle> callback, Handler handler) {
		return runAsync(new Callable<Bundle>() {
			@Override
			public Bundle call() {
				return tokenResult(account, authTokenType);
			}
		}, callback, handler);
	}

	public AccountManagerFuture<Bundle> getAuthToken(final Account account, final String authTokenType, Bundle options, boolean notifyAuthFailure, AccountManagerCallback<Bundle> callback, Handler handler) {
		return getAuthToken(account, authTokenType, options, (Activity)null, callback, handler);
	}

	public String blockingGetAuthToken(Account account, String authTokenType, boolean notifyAuthFailure) {
		return peekAuthToken(account, authTokenType);
	}

	public void addOnAccountsUpdatedListener(final OnAccountsUpdateListener listener,
	                                         Handler handler, boolean updateImmediately, String[] accountTypes) {
		synchronized (listeners) {
			listeners.put(listener, handler);
		}
		if (updateImmediately)
			tell(listener, handler);
	}

	public void addOnAccountsUpdatedListener(OnAccountsUpdateListener listener, Handler handler, boolean updateImmediately) {
		addOnAccountsUpdatedListener(listener, handler, updateImmediately, null);
	}

	public void removeOnAccountsUpdatedListener(OnAccountsUpdateListener listener) {
		synchronized (listeners) {
			listeners.remove(listener);
		}
	}

	private void tell(final OnAccountsUpdateListener listener, Handler handler) {
		final Account[] accounts = getAccounts();
		(handler != null ? handler : new Handler(Looper.getMainLooper())).post(new Runnable() {
			@Override
			public void run() {
				listener.onAccountsUpdated(accounts);
			}
		});
	}

	private void changed() {
		HashMap<OnAccountsUpdateListener, Handler> each;
		synchronized (listeners) {
			each = new HashMap<>(listeners);
		}
		for (Map.Entry<OnAccountsUpdateListener, Handler> e : each.entrySet())
			tell(e.getKey(), e.getValue());
	}

	public AccountManagerFuture<Boolean> hasFeatures(Account account, String[] features, AccountManagerCallback<Boolean> callback, Handler handler) {
		return runAsync(new Callable<Boolean>() {
			@Override
			public Boolean call() throws Exception {
				return false;
			}
		}, callback, handler);
	}

	// GEN member:android.accounts.AccountManager:public android.accounts.AccountManagerFuture<android.os.Bundle> addAccount(java.lang.String, java.lang.String, java.lang.String[], android.os.Bundle, android.app.Activity, android.accounts.AccountManagerCallback<android.os.Bundle>, android.os.Handler);
	public android.accounts.AccountManagerFuture<android.os.Bundle> addAccount(java.lang.String arg0, java.lang.String arg1, java.lang.String[] arg2, android.os.Bundle arg3, android.app.Activity arg4, android.accounts.AccountManagerCallback<android.os.Bundle> arg5, android.os.Handler arg6) { return null; }

	// GEN member:android.accounts.AccountManager:public java.lang.String getPreviousName(android.accounts.Account);
	public java.lang.String getPreviousName(android.accounts.Account arg0) { return null; }

	// GEN member:android.accounts.AccountManager:public android.accounts.AccountManagerFuture<android.os.Bundle> updateCredentials(android.accounts.Account, java.lang.String, android.os.Bundle, android.app.Activity, android.accounts.AccountManagerCallback<android.os.Bundle>, android.os.Handler);
	public android.accounts.AccountManagerFuture<android.os.Bundle> updateCredentials(android.accounts.Account arg0, java.lang.String arg1, android.os.Bundle arg2, android.app.Activity arg3, android.accounts.AccountManagerCallback<android.os.Bundle> arg4, android.os.Handler arg5) { return null; }
}
