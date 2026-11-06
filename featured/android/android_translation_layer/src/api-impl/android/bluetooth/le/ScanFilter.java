package android.bluetooth.le;

// android.jar's shape, made by gen.py
public final class ScanFilter implements android.os.Parcelable {
	// GEN member:android.bluetooth.le.ScanFilter:public static final android.os.Parcelable$Creator<android.bluetooth.le.ScanFilter> CREATOR;
	public static final android.os.Parcelable.Creator<android.bluetooth.le.ScanFilter> CREATOR = null;

	// GEN member:android.bluetooth.le.ScanFilter:public int describeContents();
	public int describeContents() { return 0; }

	// GEN member:android.bluetooth.le.ScanFilter:public void writeToParcel(android.os.Parcel, int);
	public void writeToParcel(android.os.Parcel arg0, int arg1) {}

	// GEN member:android.bluetooth.le.ScanFilter:public java.lang.String getDeviceName();
	public java.lang.String getDeviceName() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public android.os.ParcelUuid getServiceUuid();
	public android.os.ParcelUuid getServiceUuid() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public android.os.ParcelUuid getServiceUuidMask();
	public android.os.ParcelUuid getServiceUuidMask() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public android.os.ParcelUuid getServiceSolicitationUuid();
	public android.os.ParcelUuid getServiceSolicitationUuid() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public android.os.ParcelUuid getServiceSolicitationUuidMask();
	public android.os.ParcelUuid getServiceSolicitationUuidMask() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public java.lang.String getDeviceAddress();
	public java.lang.String getDeviceAddress() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getServiceData();
	public byte[] getServiceData() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getServiceDataMask();
	public byte[] getServiceDataMask() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public android.os.ParcelUuid getServiceDataUuid();
	public android.os.ParcelUuid getServiceDataUuid() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public int getManufacturerId();
	public int getManufacturerId() { return 0; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getManufacturerData();
	public byte[] getManufacturerData() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getManufacturerDataMask();
	public byte[] getManufacturerDataMask() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public int getAdvertisingDataType();
	public int getAdvertisingDataType() { return 0; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getAdvertisingData();
	public byte[] getAdvertisingData() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public byte[] getAdvertisingDataMask();
	public byte[] getAdvertisingDataMask() { return new byte[0]; }

	// GEN member:android.bluetooth.le.ScanFilter:public boolean matches(android.bluetooth.le.ScanResult);
	public boolean matches(android.bluetooth.le.ScanResult arg0) { return false; }

	// GEN member:android.bluetooth.le.ScanFilter:public java.lang.String toString();
	public java.lang.String toString() { return null; }

	// GEN member:android.bluetooth.le.ScanFilter:public int hashCode();
	public int hashCode() { return 0; }

	// GEN member:android.bluetooth.le.ScanFilter:public boolean equals(java.lang.Object);
	public boolean equals(java.lang.Object arg0) { return false; }

	protected ScanFilter() {}

	// GEN class:android.bluetooth.le.ScanFilter$Builder
	public final class Builder {
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder();
		public Builder() {}
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setDeviceName(java.lang.String);
		public android.bluetooth.le.ScanFilter.Builder setDeviceName(java.lang.String arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setDeviceAddress(java.lang.String);
		public android.bluetooth.le.ScanFilter.Builder setDeviceAddress(java.lang.String arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceUuid(android.os.ParcelUuid);
		public android.bluetooth.le.ScanFilter.Builder setServiceUuid(android.os.ParcelUuid arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceUuid(android.os.ParcelUuid, android.os.ParcelUuid);
		public android.bluetooth.le.ScanFilter.Builder setServiceUuid(android.os.ParcelUuid arg0, android.os.ParcelUuid arg1) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceSolicitationUuid(android.os.ParcelUuid);
		public android.bluetooth.le.ScanFilter.Builder setServiceSolicitationUuid(android.os.ParcelUuid arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceSolicitationUuid(android.os.ParcelUuid, android.os.ParcelUuid);
		public android.bluetooth.le.ScanFilter.Builder setServiceSolicitationUuid(android.os.ParcelUuid arg0, android.os.ParcelUuid arg1) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceData(android.os.ParcelUuid, byte[]);
		public android.bluetooth.le.ScanFilter.Builder setServiceData(android.os.ParcelUuid arg0, byte[] arg1) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setServiceData(android.os.ParcelUuid, byte[], byte[]);
		public android.bluetooth.le.ScanFilter.Builder setServiceData(android.os.ParcelUuid arg0, byte[] arg1, byte[] arg2) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setManufacturerData(int, byte[]);
		public android.bluetooth.le.ScanFilter.Builder setManufacturerData(int arg0, byte[] arg1) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setManufacturerData(int, byte[], byte[]);
		public android.bluetooth.le.ScanFilter.Builder setManufacturerData(int arg0, byte[] arg1, byte[] arg2) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setAdvertisingDataTypeWithData(int, byte[], byte[]);
		public android.bluetooth.le.ScanFilter.Builder setAdvertisingDataTypeWithData(int arg0, byte[] arg1, byte[] arg2) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter$Builder setAdvertisingDataType(int);
		public android.bluetooth.le.ScanFilter.Builder setAdvertisingDataType(int arg0) { return this; }
	
		// GEN member:android.bluetooth.le.ScanFilter$Builder:public android.bluetooth.le.ScanFilter build();
		public android.bluetooth.le.ScanFilter build() { return null; }
	}

}
