package android.os;

import dalvik.system.VMDebug;
import java.io.IOException;

public final class Debug {
	public static class MemoryInfo implements android.os.Parcelable {
		public int getTotalPss() {
			return 0;
		}

		public String getMemoryStat(String statName) {
			return "fixme getMemoryStat";
		}
	
	// GEN member:android.os.Debug$MemoryInfo:public java.util.Map<java.lang.String, java.lang.String> getMemoryStats();
	public java.util.Map<java.lang.String, java.lang.String> getMemoryStats() { return new java.util.HashMap<>(); }

	// GEN member:android.os.Debug$MemoryInfo:public int getTotalPrivateClean();
	public int getTotalPrivateClean() { return 0; }

	// GEN member:android.os.Debug$MemoryInfo:public int getTotalSharedDirty();
	public int getTotalSharedDirty() { return 0; }

	// GEN member:android.os.Debug$MemoryInfo:public int getTotalSwappablePss();
	public int getTotalSwappablePss() { return 0; }

	// GEN member:android.os.Debug$MemoryInfo:public int dalvikPrivateDirty;
	public int dalvikPrivateDirty;

	// GEN member:android.os.Debug$MemoryInfo:public int dalvikPss;
	public int dalvikPss;

	// GEN member:android.os.Debug$MemoryInfo:public int nativePrivateDirty;
	public int nativePrivateDirty;

	// GEN member:android.os.Debug$MemoryInfo:public int nativePss;
	public int nativePss;

	// GEN member:android.os.Debug$MemoryInfo:public int otherPrivateDirty;
	public int otherPrivateDirty;

	// GEN member:android.os.Debug$MemoryInfo:public int otherPss;
	public int otherPss;
}

	private static int globalAllocCount;
	private static int globalAllocSize;
	private static int globalFreedCount;
	private static int globalFreedSize;
	private static int globalExternalAllocCount;
	private static int globalExternalAllocSize;
	private static int globalExternalFreedCount;
	private static int globalExternalFreedSize;
	private static int threadAllocCount;
	private static int threadAllocSize;
	private static int threadExternalAllocCount;
	private static int threadExternalAllocSize;
	private static int threadGcInvocationCount;
	private static int globalGcInvocationCount;

	public static void waitForDebugger() {
	}

	public static class InstructionCount {
		public InstructionCount() {
		}
	}

	public static boolean isDebuggerConnected() {
		return false;
	}

	public static long getNativeHeapFreeSize() {
		return 0;
	}

	public static long getNativeHeapAllocatedSize() {
		return 0;
	}

	public static boolean waitingForDebugger() {
		return false;
	}

	public static long threadCpuTimeNanos() {
		return VMDebug.threadCpuTimeNanos();
	}

	public static void getMemoryInfo(MemoryInfo memoryInfo) {}

	public static void stopAllocCounting() {}

	public static void startAllocCounting() {
		globalAllocCount = 100;
		globalAllocSize = 10000;
	}

	public static long getNativeHeapSize() {
		return 0;
	}

	public static void resetAllCounts() {
		globalAllocCount = 0;
		globalAllocSize = 0;
		globalFreedCount = 0;
		globalFreedSize = 0;
		globalExternalAllocCount = 0;
		globalExternalAllocSize = 0;
		globalExternalFreedCount = 0;
		globalExternalFreedSize = 0;
		threadAllocCount = 0;
		threadAllocSize = 0;
		threadExternalAllocCount = 0;
		threadExternalAllocSize = 0;
		threadGcInvocationCount = 0;
		globalGcInvocationCount = 0;
	}

	public static int getGlobalAllocCount() {
		return globalAllocCount;
	}

	public static int getGlobalAllocSize() {
		return globalAllocSize;
	}

	public static int getGlobalFreedCount() {
		return globalFreedCount;
	}

	public static int getGlobalFreedSize() {
		return globalFreedSize;
	}

	public static int getGlobalExternalAllocCount() {
		return globalExternalAllocCount;
	}

	public static int getGlobalExternalAllocSize() {
		return globalExternalAllocSize;
	}

	public static int getGlobalExternalFreedCount() {
		return globalExternalFreedCount;
	}

	public static int getGlobalExternalFreedSize() {
		return globalExternalFreedSize;
	}

	public static int getGlobalGcInvocationCount() {
		return globalGcInvocationCount;
	}

	public static int getLoadedClassCount() {
		return 0;
	}

	public static int getThreadAllocCount() {
		return threadAllocCount;
	}

	public static int getThreadAllocSize() {
		return threadAllocSize;
	}

	public static int getThreadExternalAllocCount() {
		return threadExternalAllocCount;
	}

	public static int getThreadExternalAllocSize() {
		return threadExternalAllocSize;
	}

	public static int getThreadGcInvocationCount() {
		return threadGcInvocationCount;
	}

	public static int getBinderDeathObjectCount() {
		return 0;
	}

	public static int getBinderLocalObjectCount() {
		return 0;
	}

	public static int getBinderProxyObjectCount() {
		return 0;
	}

	public static int getBinderReceivedTransactions() {
		return 0;
	}

	public static int getBinderSentTransactions() {
		return 0;
	}

	public static void resetGlobalAllocCount() {
		globalAllocCount = 0;
	}

	public static void resetGlobalAllocSize() {
		globalAllocSize = 0;
	}

	public static void resetGlobalExternalAllocCount() {
		globalExternalAllocCount = 0;
	}

	public static void resetGlobalExternalAllocSize() {
		globalExternalAllocSize = 0;
	}

	public static void resetGlobalExternalFreedCount() {
		globalExternalFreedCount = 0;
	}

	public static void resetGlobalExternalFreedSize() {
		globalExternalFreedSize = 0;
	}

	public static void resetGlobalFreedCount() {
		globalFreedCount = 0;
	}

	public static void resetGlobalFreedSize() {
		globalFreedSize = 0;
	}

	public static void resetGlobalGcInvocationCount() {
		globalGcInvocationCount = 0;
	}

	public static void resetThreadAllocCount() {
		threadAllocCount = 0;
	}

	public static void resetThreadAllocSize() {
		threadAllocSize = 0;
	}

	public static void resetThreadExternalAllocCount() {
		threadExternalAllocCount = 0;
	}

	public static void resetThreadExternalAllocSize() {
		threadExternalAllocSize = 0;
	}

	public static void resetThreadGcInvocationCount() {
		threadGcInvocationCount = 0;
	}

	public static void dumpHprofData(String fileName) throws IOException {}

	public static void printLoadedClasses(int flags) {}

	public static void startMethodTracing() {}

	public static void startMethodTracing(String traceName) {}

	public static void startMethodTracing(String traceName, int bufferSize) {}

	public static void startMethodTracing(String traceName, int bufferSize, int flags) {}

	public static void stopMethodTracing() {}

	public static void startNativeTracing() {}

	public static void stopNativeTracing() {}
}
