package android.atl;

import java.util.Map;

// SIGUSR2 to the process: every thread's Java stack on stderr
public class ATLDebug {
	public static void dumpThreads() {
		StringBuilder sb = new StringBuilder("---- threads ----\n");
		for (Map.Entry<Thread, StackTraceElement[]> e : Thread.getAllStackTraces().entrySet()) {
			Thread t = e.getKey();
			sb.append('"').append(t.getName()).append("\" ").append(t.getState()).append('\n');
			for (StackTraceElement s : e.getValue())
				sb.append("    at ").append(s).append('\n');
		}
		sb.append("---- end threads ----\n");
		System.err.print(sb);
	}
}
