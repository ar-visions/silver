package android.app;

import android.accounts.AccountManager;
import android.annotation.Nullable;
import android.annotation.UnsupportedAppUsage;
import android.app.SearchManager;
import android.app.job.JobScheduler;
import android.atl.ATLLoadedApp;
import android.atl.ATLLoadedAppManager;
import android.bluetooth.BluetoothManager;
import android.content.*;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageParser;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.hardware.SensorManager;
import android.hardware.camera2.CameraManager;
import android.hardware.display.ColorDisplayManager;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.hardware.usb.UsbManager;
import android.location.LocationManager;
import android.media.AudioManager;
import android.media.MediaRouter;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.*;
import android.os.storage.StorageManager;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.util.Slog;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.WindowManager;
import android.view.WindowManagerImpl;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.CaptioningManager;
import android.view.inputmethod.InputMethodManager;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;

public final class ContextImpl extends Context {
	private static final int ATL_FLAG_NO_CODE = 0x00000001;
	private static final int ATL_FLAG_RESTRICTED = 0x00000002;
	private static final int ATL_FLAG_PROTECTED_STORAGE = 0x00000004;
	private final static String TAG = "ContextImpl";
	private final Resources resources;
	private final ATLLoadedApp atl_loaded_app;
	/**
	 * While not being part of the official Android API, some applications use it to reset/reload the theme in the context.
	 */
	@UnsupportedAppUsage
	private Resources.Theme mTheme;
	/**
	 * While not being part of the official Android API, some application use it to get the theme resource ID.
	 */
	@UnsupportedAppUsage
	private int mThemeResource;

	private final LayoutInflater layout_inflater = new LayoutInflater(this);
	private final JobScheduler job_scheduler = new JobScheduler(this);

	private final int atl_flags;

	public ContextImpl(Resources resources, ATLLoadedApp loadedApp, Resources.Theme theme) {
		this(resources, loadedApp, 0, 0);
		getTheme().setTo(theme);
	}

	private ContextImpl(Resources resources, ATLLoadedApp loadedApp, Resources.Theme theme, int flags) {
		this(resources, loadedApp, 0, flags);
		getTheme().setTo(theme);
	}

	public ContextImpl(Resources resources, ATLLoadedApp loadedApp, int themeResource) {
		this(resources, loadedApp, themeResource, 0);
	}

	private ContextImpl(Resources resources, ATLLoadedApp loadedApp, int themeResource, int flags) {
		this.resources = resources;
		this.atl_loaded_app = loadedApp;
		mThemeResource = themeResource;
		this.atl_flags = flags;
	}

	@Nullable
	private ATLLoadedApp atl_get_intent_target(Intent intent) {
		ATLLoadedApp targetApp;
		String targetPackage = intent.getComponent() != null ? intent.getComponent().getPackageName() : null;
		if (targetPackage != null || (targetPackage = intent.getPackage()) != null) {
			if (targetPackage.equals(this.atl_loaded_app.pkg.packageName)) {
				targetApp = this.atl_loaded_app;
			} else {
				targetApp = ATLLoadedAppManager.getAppFromPackageName(targetPackage);
			}
		} else {
			targetApp = this.atl_loaded_app;
		}
		return targetApp;
	}

	@Override
	public ApplicationInfo getApplicationInfo() {
		return atl_loaded_app.pkg.applicationInfo;
	}

	@Override
	public void setTheme(int resId) {
		mThemeResource = resId;
		if (mTheme != null) {
			mTheme.applyStyle(resId, true);
		}
	}

	@Override
	public boolean isRestricted() {
		return (this.atl_flags & ATL_FLAG_RESTRICTED) != 0;
	}

	@Override
	public boolean stopService(Intent intent) {
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		if (targetApp == null) {
			return false;
		}
		return targetApp.stopService(intent);
	}

	@Override
	public Resources.Theme getTheme() {
		if (mTheme == null) {
			mTheme = resources.newTheme();
			if (mThemeResource != 0) {
				mTheme.applyStyle(mThemeResource, true);
			}
		}
		return mTheme;
	}

	@Override
	public Object getSystemService(String name) {
		switch (name) {
			case "window":
				return new WindowManagerImpl();
			case "clipboard":
				return new ClipboardManager();
			case "sensor":
				return new SensorManager();
			case "connectivity":
				return new ConnectivityManager();
			case "keyguard":
				return new KeyguardManager();
			case "phone":
				return new TelephonyManager();
			case "audio":
				return new AudioManager();
			case "activity":
				return new ActivityManager();
			case "account":
				return AccountManager.get(this);
			case "usb":
				return new UsbManager();
			case "vibrator":
				return (vibrator != null) ? vibrator : (vibrator = new Vibrator());
			case "power":
				return new PowerManager();
			case "display":
				return new DisplayManager();
			case "media_router":
				return new MediaRouter();
			case "notification":
				return new NotificationManager();
			case "alarm":
				return new AlarmManager();
			case "input":
				return new InputManager();
			case "location":
				return new LocationManager();
			case "uimode":
				return new UiModeManager();
			case "input_method":
				return new InputMethodManager();
			case "accessibility":
				return new AccessibilityManager();
			case "layout_inflater":
				return layout_inflater;
			case "wifi":
				return new WifiManager();
			case "bluetooth":
				return new BluetoothManager();
			case "jobscheduler":
				return job_scheduler;
			case "appops":
				return new AppOpsManager();
			case "user":
				return new UserManager();
			case "captioning":
				return new CaptioningManager();
			case "statusbar":
				return new StatusBarManager();
			case "camera":
				return new CameraManager();
			case "color_display":
				return new ColorDisplayManager();
			case "search":
				return new SearchManager();
			case "storage":
				return new StorageManager();
			case "batterymanager":
				return new BatteryManager();
			case "download":
				return new DownloadManager();
			default:
				Slog.e(TAG, "!!!!!!! getSystemService: case >" + name + "< is not implemented yet");
				return null;
		}
	}

	// the services by class, as Android's SystemServiceRegistry names them
	private static final java.util.Map<Class<?>, String> SERVICE_NAMES = new java.util.HashMap<>();
	static {
		SERVICE_NAMES.put(WindowManager.class, "window");
		SERVICE_NAMES.put(ClipboardManager.class, "clipboard");
		SERVICE_NAMES.put(SensorManager.class, "sensor");
		SERVICE_NAMES.put(ConnectivityManager.class, "connectivity");
		SERVICE_NAMES.put(KeyguardManager.class, "keyguard");
		SERVICE_NAMES.put(TelephonyManager.class, "phone");
		SERVICE_NAMES.put(AudioManager.class, "audio");
		SERVICE_NAMES.put(ActivityManager.class, "activity");
		SERVICE_NAMES.put(AccountManager.class, "account");
		SERVICE_NAMES.put(UsbManager.class, "usb");
		SERVICE_NAMES.put(Vibrator.class, "vibrator");
		SERVICE_NAMES.put(PowerManager.class, "power");
		SERVICE_NAMES.put(DisplayManager.class, "display");
		SERVICE_NAMES.put(MediaRouter.class, "media_router");
		SERVICE_NAMES.put(NotificationManager.class, "notification");
		SERVICE_NAMES.put(AlarmManager.class, "alarm");
		SERVICE_NAMES.put(InputManager.class, "input");
		SERVICE_NAMES.put(LocationManager.class, "location");
		SERVICE_NAMES.put(UiModeManager.class, "uimode");
		SERVICE_NAMES.put(InputMethodManager.class, "input_method");
		SERVICE_NAMES.put(AccessibilityManager.class, "accessibility");
		SERVICE_NAMES.put(LayoutInflater.class, "layout_inflater");
		SERVICE_NAMES.put(WifiManager.class, "wifi");
		SERVICE_NAMES.put(BluetoothManager.class, "bluetooth");
		SERVICE_NAMES.put(JobScheduler.class, "jobscheduler");
		SERVICE_NAMES.put(AppOpsManager.class, "appops");
		SERVICE_NAMES.put(UserManager.class, "user");
		SERVICE_NAMES.put(CaptioningManager.class, "captioning");
		SERVICE_NAMES.put(StatusBarManager.class, "statusbar");
		SERVICE_NAMES.put(CameraManager.class, "camera");
		SERVICE_NAMES.put(ColorDisplayManager.class, "color_display");
		SERVICE_NAMES.put(SearchManager.class, "search");
		SERVICE_NAMES.put(StorageManager.class, "storage");
		SERVICE_NAMES.put(BatteryManager.class, "batterymanager");
		SERVICE_NAMES.put(DownloadManager.class, "download");
	}

	public static String systemServiceName(Class<?> serviceClass) {
		return SERVICE_NAMES.get(serviceClass);
	}

	@Override
	public String getSystemServiceName(Class<?> serviceClass) {
		return systemServiceName(serviceClass);
	}

	@Override
	public Object getSystemService(Class<?> serviceClass) throws InstantiationException, IllegalAccessException, InvocationTargetException {
		String name = systemServiceName(serviceClass);
		if (name != null)
			return getSystemService(name);
		// a manager this registry does not know: its own public no-arg constructor, else none
		for (java.lang.reflect.Constructor<?> c : serviceClass.getConstructors()) {
			if (c.getParameterCount() == 0)
				return c.newInstance();
		}
		Slog.e(TAG, "!!!!!!! getSystemService: class >" + serviceClass.getName() + "< is not implemented yet");
		return null;
	}

	@Override
	public Resources getResources() {
		return resources;
	}

	@Override
	public ClassLoader getClassLoader() {
		if ((this.atl_flags & ATL_FLAG_NO_CODE) != 0) {
			return null;
		}
		return this.atl_loaded_app.class_loader;
	}

	public ComponentName startService(Intent intent) {
		// One way Messenger interface for GCM. Request per intent and response per Messenger.
		// Wrap the response part in a BroadcastReceiver for compatibility with our gcm_service
		if ("com.google.android.c2dm.intent.REGISTER".equals(intent.getAction()) && intent.getParcelableExtra("google.messenger") instanceof Messenger) {
			final Messenger messenger = (Messenger)intent.getParcelableExtra("google.messenger");
			this.registerReceiver(new BroadcastReceiver() {
				@Override
				public void onReceive(Context context, Intent resultIntent) {
					try {
						messenger.send(Message.obtain(null, 0, resultIntent));
					} catch (RemoteException e) {
						e.printStackTrace();
					}
				}
			}, new IntentFilter("com.google.android.c2dm.intent.REGISTRATION"));
		}
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		// Cloud Messaging (c2dm) intents should always be started using DBus. Delivering them to the in process gstub is pointless
		if (targetApp == null || (intent.getAction() != null && intent.getAction().startsWith("com.google.android.c2dm"))) {
			// External package. Try to start using DBus Action
			if (intent.getPackage() != null)
				nativeStartExternalService(intent);
			return null;
		}
		return targetApp.startOrBindService(intent, null);
	}

	@Override
	public boolean bindService(Intent intent, ServiceConnection serviceConnection, int flags) {
		// Two way Messenger interface for GCM. Request and response are send over Messenger.
		// Wrap it on top of the old intent interface for compatibility with our gcm_service
		if ("com.google.android.c2dm.intent.REGISTER".equals(intent.getAction())) {
			Messenger requestMessenger = new Messenger(new Handler(Looper.getMainLooper()) {
				@Override
				public void handleMessage(Message requestMessage) {
					int request_id = requestMessage.arg1;
					Messenger responseMessenger = requestMessage.replyTo;
					registerReceiver(new BroadcastReceiver() {
						@Override
						public void onReceive(Context context, Intent responseIntent) {
							try {
								// map response Intent to Message
								Message responseMessage = Message.obtain(null, 0, request_id, 0);
								Bundle data = new Bundle();
								data.putBundle("data", responseIntent.getExtras());
								responseMessage.setData(data);
								responseMessenger.send(responseMessage);
							} catch (RemoteException e) {
								e.printStackTrace();
							}
						}
					}, new IntentFilter("com.google.android.c2dm.intent.REGISTRATION"));
					// map request Message to Intent
					Intent requestIntent = new Intent("com.google.android.c2dm.intent.REGISTER");
					requestIntent.setPackage("com.google.android.gms");
					requestIntent.putExtras(requestMessage.getData().getBundle("data"));
					nativeStartExternalService(requestIntent);
				}
			});
			serviceConnection.onServiceConnected(intent.getComponent(), requestMessenger.getBinder());
			return true;
		}
		ATLLoadedApp targetApp = this.atl_get_intent_target(intent);
		if (targetApp == null) {
			return false;
		}
		return targetApp.startOrBindService(intent, serviceConnection) != null;
	}

	@Override
	public void startActivity(Intent intent) {
		Slog.i(TAG, "startActivity(" + intent + ") called");
		if (intent.getAction() != null && intent.getAction().equals("android.intent.action.CHOOSER")) {
			intent = (Intent)intent.getExtras().get("android.intent.extra.INTENT");
		}

		String className = null;
		if (intent.getComponent() != null) {
			className = intent.getComponent().getClassName();
		} else {
			if (intent.getAction() != null && intent.getAction().equals("android.intent.action.SEND")) {
				Slog.i(TAG, "sharing intent via composeMail: " + intent);
				String text = intent.getStringExtra("android.intent.extra.TEXT");
				ParcelFileDescriptor fd = null;
				if (intent.hasExtra(Intent.EXTRA_STREAM)) {
					try {
						fd = getContentResolver().openFileDescriptor((Uri)intent.getParcelableExtra(Intent.EXTRA_STREAM), "r");
					} catch (FileNotFoundException e) {
						e.printStackTrace();
					}
				}
				final ParcelFileDescriptor fd_final = fd;
				Runnable runnable = new Runnable() {
					@Override
					public void run() {
						nativeShareFile(text, fd_final != null ? fd_final.getFd() : -1);
						if (fd_final != null) {
							try {
								fd_final.close();
							} catch (IOException e) {
								e.printStackTrace();
							}
						}
					}
				};
				if (Looper.myLooper() == Looper.getMainLooper()) {
					runnable.run();
				} else {
					new Handler(Looper.getMainLooper()).post(runnable);
				}
				return;
			} else if (intent.getData() != null) {
				Slog.i(TAG, "starting extern activity with intent: " + intent);
				if (intent.getData().getScheme().equals("content")) {
					try (ParcelFileDescriptor fd = getContentResolver().openFileDescriptor(intent.getData(), "r")) {
						if (fd != null) {
							nativeOpenFile(fd.getFd());
							return;
						}
					} catch (IOException e) {
						e.printStackTrace();
					}
				}
				Activity.nativeOpenURI(String.valueOf(intent.getData()));
				return;
			}
			for (PackageParser.Activity activity : this.atl_loaded_app.pkg.activities) {
				for (PackageParser.IntentInfo intentInfo : activity.intents) {
					if (intentInfo.matchAction(intent.getAction())) {
						className = activity.className;
						break;
					}
				}
			}
		}
		if (className == null) {
			Slog.w(TAG, "startActivity: intent could not be handled.");
			return;
		}
		final String className_ = className;
		final Intent intent_ = intent;
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				try {
					if ((intent_.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0 && intent_.getComponent() != null) {
						boolean found = Activity.nativeResumeActivity(
						    ContextImpl.this.atl_loaded_app.loadClass(intent_.getComponent().getClassName())
							.asSubclass(Activity.class),
						    intent_);
						if (found)
							return;
					}
					Activity activity = Activity.internalCreateActivity(
					    className_, ContextImpl.this.atl_loaded_app.getNativeWindow(), intent_);
					Activity.nativeStartActivity(activity);
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		});
	}

	@Override
	public Context createPackageContext(String packageName, int flags) {
		if (packageName.equals(atl_loaded_app.pkg.packageName)) {
			return this;
		}
		ATLLoadedApp primaryApplication = ATLLoadedApp.getPrimaryApplication();
		if (primaryApplication.pkg.packageName.equals(packageName)) {
			return primaryApplication.getApplication();
		}
		if (packageName.equals("android")) {
			// Shortcut for system application
			ATLLoadedApp system = ATLLoadedApp.getSystemApplication();
			return new ContextImpl(Resources.getSystem(), system,
			                       Resources.selectDefaultTheme(0, Build.VERSION.SDK_INT));
		}
		ATLLoadedApp loadedApplication = ATLLoadedAppManager.getAppFromPackageName(packageName);
		if (loadedApplication != null) {
			int atl_flags = 0;
			if ((flags & Context.CONTEXT_INCLUDE_CODE) == 0) {
				atl_flags |= ATL_FLAG_NO_CODE;
			}
			if ((flags & Context.CONTEXT_RESTRICTED) != 0) {
				atl_flags |= ATL_FLAG_RESTRICTED;
			}
			return new ContextImpl(loadedApplication.createDefaultResources(),
			                       loadedApplication, loadedApplication.pkg.applicationInfo.theme,
			                       atl_flags);
		}
		// Return the application context as a fallback
		Log.e(TAG, "!!!!!!! createPackageContext: case >" + packageName + "< is not implemented yet");
		return primaryApplication.getApplication();
	}

	@Override
	public Context createConfigurationContext(Configuration config_override) {
		Configuration config = new Configuration();
		config.setTo(getResources().getConfiguration());
		if (config_override != null)
			config.updateFrom(config_override);
		return this.atl_loaded_app.createContext(null, config, getTheme());
	}

	@Override
	public Context createDisplayContext(Display display) {
		return new ContextImpl(getResources(), this.atl_loaded_app, getTheme(), this.atl_flags);
	}

	@Override
	public boolean isDeviceProtectedStorage() {
		return (this.atl_flags & ATL_FLAG_PROTECTED_STORAGE) != 0;
	}

	@Override
	public Context createDeviceProtectedStorageContext() {
		return new ContextImpl(getResources(), this.atl_loaded_app, getTheme(),
		                       this.atl_flags | ATL_FLAG_PROTECTED_STORAGE);
	}

	@Override
	public int getThemeResId() {
		return mThemeResource;
	}

	@Override
	public ATLLoadedApp get_atl_loaded_app() {
		return atl_loaded_app;
	}
}
