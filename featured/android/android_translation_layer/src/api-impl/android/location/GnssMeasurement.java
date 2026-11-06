package android.location;

// android.jar's shape, made by gen.py
public final class GnssMeasurement implements android.os.Parcelable {
	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_CYCLE_SLIP = 4;
	public static final int ADR_STATE_CYCLE_SLIP = 4;

	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_HALF_CYCLE_REPORTED = 16;
	public static final int ADR_STATE_HALF_CYCLE_REPORTED = 16;

	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_HALF_CYCLE_RESOLVED = 8;
	public static final int ADR_STATE_HALF_CYCLE_RESOLVED = 8;

	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_RESET = 2;
	public static final int ADR_STATE_RESET = 2;

	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_UNKNOWN = 0;
	public static final int ADR_STATE_UNKNOWN = 0;

	// GEN member:android.location.GnssMeasurement:public static final int ADR_STATE_VALID = 1;
	public static final int ADR_STATE_VALID = 1;

	// GEN member:android.location.GnssMeasurement:public static final android.os.Parcelable$Creator<android.location.GnssMeasurement> CREATOR;
	public static final android.os.Parcelable.Creator<android.location.GnssMeasurement> CREATOR = null;

	// GEN member:android.location.GnssMeasurement:public static final int MULTIPATH_INDICATOR_DETECTED = 1;
	public static final int MULTIPATH_INDICATOR_DETECTED = 1;

	// GEN member:android.location.GnssMeasurement:public static final int MULTIPATH_INDICATOR_NOT_DETECTED = 2;
	public static final int MULTIPATH_INDICATOR_NOT_DETECTED = 2;

	// GEN member:android.location.GnssMeasurement:public static final int MULTIPATH_INDICATOR_UNKNOWN = 0;
	public static final int MULTIPATH_INDICATOR_UNKNOWN = 0;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_2ND_CODE_LOCK = 65536;
	public static final int STATE_2ND_CODE_LOCK = 65536;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_BDS_D2_BIT_SYNC = 256;
	public static final int STATE_BDS_D2_BIT_SYNC = 256;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_BDS_D2_SUBFRAME_SYNC = 512;
	public static final int STATE_BDS_D2_SUBFRAME_SYNC = 512;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_BIT_SYNC = 2;
	public static final int STATE_BIT_SYNC = 2;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_CODE_LOCK = 1;
	public static final int STATE_CODE_LOCK = 1;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GAL_E1BC_CODE_LOCK = 1024;
	public static final int STATE_GAL_E1BC_CODE_LOCK = 1024;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GAL_E1B_PAGE_SYNC = 4096;
	public static final int STATE_GAL_E1B_PAGE_SYNC = 4096;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GAL_E1C_2ND_CODE_LOCK = 2048;
	public static final int STATE_GAL_E1C_2ND_CODE_LOCK = 2048;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GLO_STRING_SYNC = 64;
	public static final int STATE_GLO_STRING_SYNC = 64;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GLO_TOD_DECODED = 128;
	public static final int STATE_GLO_TOD_DECODED = 128;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_GLO_TOD_KNOWN = 32768;
	public static final int STATE_GLO_TOD_KNOWN = 32768;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_MSEC_AMBIGUOUS = 16;
	public static final int STATE_MSEC_AMBIGUOUS = 16;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_SBAS_SYNC = 8192;
	public static final int STATE_SBAS_SYNC = 8192;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_SUBFRAME_SYNC = 4;
	public static final int STATE_SUBFRAME_SYNC = 4;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_SYMBOL_SYNC = 32;
	public static final int STATE_SYMBOL_SYNC = 32;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_TOW_DECODED = 8;
	public static final int STATE_TOW_DECODED = 8;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_TOW_KNOWN = 16384;
	public static final int STATE_TOW_KNOWN = 16384;

	// GEN member:android.location.GnssMeasurement:public static final int STATE_UNKNOWN = 0;
	public static final int STATE_UNKNOWN = 0;

	// GEN member:android.location.GnssMeasurement:public int getSvid();
	public int getSvid() { return 0; }

	// GEN member:android.location.GnssMeasurement:public int getConstellationType();
	public int getConstellationType() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getTimeOffsetNanos();
	public double getTimeOffsetNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public int getState();
	public int getState() { return 0; }

	// GEN member:android.location.GnssMeasurement:public long getReceivedSvTimeNanos();
	public long getReceivedSvTimeNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public long getReceivedSvTimeUncertaintyNanos();
	public long getReceivedSvTimeUncertaintyNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getCn0DbHz();
	public double getCn0DbHz() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasBasebandCn0DbHz();
	public boolean hasBasebandCn0DbHz() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getBasebandCn0DbHz();
	public double getBasebandCn0DbHz() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getPseudorangeRateMetersPerSecond();
	public double getPseudorangeRateMetersPerSecond() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getPseudorangeRateUncertaintyMetersPerSecond();
	public double getPseudorangeRateUncertaintyMetersPerSecond() { return 0; }

	// GEN member:android.location.GnssMeasurement:public int getAccumulatedDeltaRangeState();
	public int getAccumulatedDeltaRangeState() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getAccumulatedDeltaRangeMeters();
	public double getAccumulatedDeltaRangeMeters() { return 0; }

	// GEN member:android.location.GnssMeasurement:public double getAccumulatedDeltaRangeUncertaintyMeters();
	public double getAccumulatedDeltaRangeUncertaintyMeters() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasCarrierFrequencyHz();
	public boolean hasCarrierFrequencyHz() { return false; }

	// GEN member:android.location.GnssMeasurement:public float getCarrierFrequencyHz();
	public float getCarrierFrequencyHz() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasCarrierCycles();
	public boolean hasCarrierCycles() { return false; }

	// GEN member:android.location.GnssMeasurement:public long getCarrierCycles();
	public long getCarrierCycles() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasCarrierPhase();
	public boolean hasCarrierPhase() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getCarrierPhase();
	public double getCarrierPhase() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasCarrierPhaseUncertainty();
	public boolean hasCarrierPhaseUncertainty() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getCarrierPhaseUncertainty();
	public double getCarrierPhaseUncertainty() { return 0; }

	// GEN member:android.location.GnssMeasurement:public int getMultipathIndicator();
	public int getMultipathIndicator() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasSnrInDb();
	public boolean hasSnrInDb() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getSnrInDb();
	public double getSnrInDb() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasAutomaticGainControlLevelDb();
	public boolean hasAutomaticGainControlLevelDb() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getAutomaticGainControlLevelDb();
	public double getAutomaticGainControlLevelDb() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasCodeType();
	public boolean hasCodeType() { return false; }

	// GEN member:android.location.GnssMeasurement:public java.lang.String getCodeType();
	public java.lang.String getCodeType() { return null; }

	// GEN member:android.location.GnssMeasurement:public boolean hasFullInterSignalBiasNanos();
	public boolean hasFullInterSignalBiasNanos() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getFullInterSignalBiasNanos();
	public double getFullInterSignalBiasNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasFullInterSignalBiasUncertaintyNanos();
	public boolean hasFullInterSignalBiasUncertaintyNanos() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getFullInterSignalBiasUncertaintyNanos();
	public double getFullInterSignalBiasUncertaintyNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasSatelliteInterSignalBiasNanos();
	public boolean hasSatelliteInterSignalBiasNanos() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getSatelliteInterSignalBiasNanos();
	public double getSatelliteInterSignalBiasNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public boolean hasSatelliteInterSignalBiasUncertaintyNanos();
	public boolean hasSatelliteInterSignalBiasUncertaintyNanos() { return false; }

	// GEN member:android.location.GnssMeasurement:public double getSatelliteInterSignalBiasUncertaintyNanos();
	public double getSatelliteInterSignalBiasUncertaintyNanos() { return 0; }

	// GEN member:android.location.GnssMeasurement:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.location.GnssMeasurement:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.location.GnssMeasurement:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	protected GnssMeasurement() {}
}
