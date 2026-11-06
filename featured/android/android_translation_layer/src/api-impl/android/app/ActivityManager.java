package android.app;

import android.atl.ATLLoadedApp;
import android.content.Context;
import android.content.pm.ConfigurationInfo;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Process;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

public class ActivityManager {

	public static class RunningAppProcessInfo implements android.os.Parcelable {
		public static final int IMPORTANCE_FOREGROUND = 100;
		public static final int IMPORTANCE_FOREGROUND_SERVICE = 125;
		public static final int IMPORTANCE_VISIBLE = 200;
		public static final int IMPORTANCE_SERVICE = 300;
		public static final int IMPORTANCE_CACHED = 400;
		public static final int IMPORTANCE_GONE = 1000;
		public int importance = IMPORTANCE_FOREGROUND;
		public int lru;
		public int importanceReasonCode;
		public int lastTrimLevel;
		public int pid;
		public int uid;
		public String processName;
		public String[] pkgList;

		public RunningAppProcessInfo() {}

		public RunningAppProcessInfo(String processName, int pid, String[] pkgList) {
			this.processName = processName;
			this.pid = pid;
			this.pkgList = pkgList;
		}

		// this process: the app being run, its uid
		private RunningAppProcessInfo(int pid, String processName) {
			this.pid = pid;
			this.processName = processName;
			this.pkgList = new String[] {processName};
			this.uid = ATLLoadedApp.getPrimaryApplication().pkg.applicationInfo.uid;
		}
	
	// GEN member:android.app.ActivityManager$RunningAppProcessInfo:public android.content.ComponentName importanceReasonComponent;
	public android.content.ComponentName importanceReasonComponent;
}

	public static class TaskDescription implements android.os.Parcelable {
		public TaskDescription(String name) {}
		public TaskDescription(String name, Bitmap icon, int color) {}
	}

	public List<RunningAppProcessInfo> getRunningAppProcesses() {
		return Arrays.asList(new RunningAppProcessInfo(Process.myPid(),
		                                               ATLLoadedApp.getPrimaryApplication().pkg.packageName));
	}

	public boolean isLowRamDevice() { return false; }
	public static boolean isRunningInTestHarness() { return false; }

	public static class MemoryInfo implements Parcelable {
		public long availMem;

		public long totalMem;

		public long threshold;

		public boolean lowMemory = false;

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			dest.writeLong(availMem);
			dest.writeLong(totalMem);
			dest.writeLong(threshold);
			dest.writeInt(lowMemory ? 1 : 0);
		}

		public void readFromParcel(Parcel source) {
			availMem = source.readLong();
			totalMem = source.readLong();
			threshold = source.readLong();
			lowMemory = source.readInt() != 0;
		}

		public static final Parcelable.Creator<MemoryInfo> CREATOR = new Parcelable.Creator<MemoryInfo>() {
			public MemoryInfo createFromParcel(Parcel in) {
				MemoryInfo info = new MemoryInfo();
				info.readFromParcel(in);
				return info;
			}

			public MemoryInfo[] newArray(int size) {
				return new MemoryInfo[size];
			}
		};
	}

	// the machine's own memory, as /proc/meminfo has it
	public void getMemoryInfo(MemoryInfo outInfo) {
		long total = 0, avail = 0;
		try (java.io.BufferedReader r = new java.io.BufferedReader(new java.io.FileReader("/proc/meminfo"))) {
			String line;
			while ((line = r.readLine()) != null) {
				if (line.startsWith("MemTotal:"))
					total = kbOf(line);
				else if (line.startsWith("MemAvailable:"))
					avail = kbOf(line);
			}
		} catch (java.io.IOException e) {
		}
		outInfo.totalMem = total;
		outInfo.availMem = avail;
		outInfo.threshold = total / 16;
		outInfo.lowMemory = avail < outInfo.threshold;
	}

	private static long kbOf(String line) {
		String[] words = line.trim().split("\\s+");
		return words.length > 1 ? Long.parseLong(words[1]) * 1024 : 0;
	}

	public ConfigurationInfo getDeviceConfigurationInfo() {
		return new ConfigurationInfo();
	}

	// the heap ART gives this process, in MB, as Android sizes it
	public int getMemoryClass() {
		return (int)Math.max(16, Runtime.getRuntime().maxMemory() >> 20);
	}

	public int getLargeMemoryClass() {
		return getMemoryClass() * 2;
	}

	public static void getMyMemoryState(RunningAppProcessInfo outInfo) {}

	public boolean clearApplicationUserData() { return false; }

	public static class ProcessErrorStateInfo implements Parcelable {
		public int condition;
		public String processName;
		public int pid;
		public int uid;
		public String tag;
		public String shortMsg;
		public String longMsg;
		public byte[] crashData;
		public int uid_mm;
		public int etype;
		public int euid;
		public int epid;
		public String eproc;
		public String edata;
		public Map<String, Object> extras;

		public static final int CRASHED = 1;
		public static final int NOT_RESPONDING = 2;
		public static final int NO_ERROR = 0;

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			dest.writeInt(condition);
			dest.writeString(processName);
		}

		public void readFromParcel(Parcel source) {
			condition = source.readInt();
			processName = source.readString();
		}

		public static final Parcelable.Creator<ProcessErrorStateInfo> CREATOR = new Parcelable.Creator<ProcessErrorStateInfo>() {
			public ProcessErrorStateInfo createFromParcel(Parcel in) {
				ProcessErrorStateInfo info = new ProcessErrorStateInfo();
				info.readFromParcel(in);
				return info;
			}

			public ProcessErrorStateInfo[] newArray(int size) {
				return new ProcessErrorStateInfo[size];
			}
		};
	}

	public static class AppTask {
	// GEN member:android.app.ActivityManager$AppTask:public android.app.ActivityManager$RecentTaskInfo getTaskInfo();
	public android.app.ActivityManager.RecentTaskInfo getTaskInfo() { return null; }

	// GEN member:android.app.ActivityManager$AppTask:public void setExcludeFromRecents(boolean);
	public void setExcludeFromRecents(boolean arg0) {}
}
	public List<ActivityManager.AppTask> getAppTasks() {
		return new ArrayList<>();
	}

	public static class RecentTaskInfo implements Parcelable {
		public int id;
		public int persistentId;
		public android.content.Intent baseIntent;
		public android.content.ComponentName origActivity;
		public CharSequence description;
		public int userId;

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			dest.writeInt(id);
			dest.writeLong(persistentId);
		}

		public void readFromParcel(Parcel source) {
			id = source.readInt();
			persistentId = (int)source.readLong();
		}

		public static final Parcelable.Creator<RecentTaskInfo> CREATOR = new Parcelable.Creator<RecentTaskInfo>() {
			public RecentTaskInfo createFromParcel(Parcel in) {
				RecentTaskInfo info = new RecentTaskInfo();
				info.readFromParcel(in);
				return info;
			}

			public RecentTaskInfo[] newArray(int size) {
				return new RecentTaskInfo[size];
			}
		};
	}

	public static class RunningTaskInfo implements Parcelable {
		public int id;
		public android.content.ComponentName baseActivity;
		public android.content.ComponentName topActivity;
		public int numActivities;
		public int numRunning;
		public android.graphics.Bitmap thumbnail;
		public CharSequence description;

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			dest.writeInt(id);
			dest.writeInt(numActivities);
			dest.writeInt(numRunning);
		}

		public void readFromParcel(Parcel source) {
			id = source.readInt();
			numActivities = source.readInt();
			numRunning = source.readInt();
		}

		public static final Parcelable.Creator<RunningTaskInfo> CREATOR = new Parcelable.Creator<RunningTaskInfo>() {
			public RunningTaskInfo createFromParcel(Parcel in) {
				RunningTaskInfo info = new RunningTaskInfo();
				info.readFromParcel(in);
				return info;
			}

			public RunningTaskInfo[] newArray(int size) {
				return new RunningTaskInfo[size];
			}
		};
	}

	public List<RecentTaskInfo> getRecentTasks(int maxNum, int flags) {
		return new ArrayList<>();
	}

	public List<RunningTaskInfo> getRunningTasks(int maxNum) {
		return new ArrayList<>();
	}

	public List<ActivityManager.ProcessErrorStateInfo> getProcessesInErrorState() {
		return new ArrayList<>();
	}

	public static class RunningServiceInfo implements Parcelable {
		public android.content.ComponentName service;
		public int pid;
		public int uid;
		public String process;
		public boolean foreground;
		public long activeSince;
		public boolean started;
		public int clientCount;
		public int clientPackageCount;
		public int crashCount;
		public long restarting;
		public int flags;
		public String clientPackage;

		public RunningServiceInfo() {
		}

		public int describeContents() {
			return 0;
		}

		public void writeToParcel(Parcel dest, int flags) {
			return;
		}

		public void readFromParcel(Parcel source) {
			return;
		}
	}

	public List<RunningServiceInfo> getRunningServices(int maxNum)
	    throws SecurityException {
		return new ArrayList<>();
	}

	public List<ApplicationExitInfo> getHistoricalProcessExitReasons(String pkgname, int pid, int maxNum) {
		return Collections.emptyList();
	}

	public static boolean isUserAMonkey() { return false; }

	public void moveTaskToFront(int taskId, int flags, Bundle options) {
	}

	public static boolean isLowMemoryKillReportSupported() {
		return false;
	}

	// GEN member:android.app.ActivityManager:public static boolean isRunningInUserTestHarness();
	public static boolean isRunningInUserTestHarness() { return false; }

	// GEN member:android.app.ActivityManager:public java.util.List<android.app.ApplicationStartInfo> getHistoricalProcessStartReasons(int);
	public java.util.List<android.app.ApplicationStartInfo> getHistoricalProcessStartReasons(int arg0) { return new java.util.ArrayList<>(); }

	// GEN member:android.app.ActivityManager:public android.os.Debug$MemoryInfo[] getProcessMemoryInfo(int[]);
	public android.os.Debug.MemoryInfo[] getProcessMemoryInfo(int[] arg0) { return new android.os.Debug.MemoryInfo[0]; }

	// GEN member:android.app.ActivityManager:public void setProcessStateSummary(byte[]);
	public void setProcessStateSummary(byte[] arg0) {}
}
