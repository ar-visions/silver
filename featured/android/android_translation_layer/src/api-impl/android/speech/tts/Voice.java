package android.speech.tts;

import java.util.Locale;
import java.util.Set;

public class Voice implements android.os.Parcelable {
	private final String name;
	private final Locale locale;
	private final int quality;
	private final int latency;
	private final boolean requiresNetworkConnection;
	private final Set<String> features;

	public Voice(String name, Locale locale, int quality, int latency, boolean requiresNetworkConnection, Set<String> features) {
		this.name = name;
		this.locale = locale;
		this.quality = quality;
		this.latency = latency;
		this.requiresNetworkConnection = requiresNetworkConnection;
		this.features = features;
	}

	public String getName() { return name; }
	public Locale getLocale() { return locale; }
	public int getQuality() { return quality; }
	public int getLatency() { return latency; }
	public boolean isNetworkConnectionRequired() { return requiresNetworkConnection; }
	public Set<String> getFeatures() { return features; }
}
