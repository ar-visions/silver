package android.location;

import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public class Address implements android.os.Parcelable {
	private Locale locale;
	private String featureName;
	private String addressLine1;
	private String addressLine2;
	private String addressLine3;
	private String adminArea;
	private String subAdminArea;
	private String locality;
	private String subLocality;
	private String thoroughfare;
	private String subThoroughfare;
	private String premises;
	private String countryName;
	private String countryCode;
	private String postalCode;
	private String phone;
	private String url;
	private double latitude;
	private double longitude;
	private boolean hasLatitude;
	private boolean hasLongitude;

	public Address(Locale locale) {
		this.locale = locale;
	}

	public Locale getLocale() {
		return locale;
	}

	public String getAddressLine(int index) {
		switch (index) {
			case 0:
				return addressLine1;
			case 1:
				return addressLine2;
			case 2:
				return addressLine3;
			default:
				return null;
		}
	}

	public void setAddressLine(int index, String line) {
		switch (index) {
			case 0:
				addressLine1 = line;
				break;
			case 1:
				addressLine2 = line;
				break;
			case 2:
				addressLine3 = line;
				break;
		}
	}

	public String getFeatureName() { return featureName; }
	public void setFeatureName(String featureName) { this.featureName = featureName; }

	public String getAdminArea() { return adminArea; }
	public void setAdminArea(String adminArea) { this.adminArea = adminArea; }

	public String getSubAdminArea() { return subAdminArea; }
	public void setSubAdminArea(String subAdminArea) { this.subAdminArea = subAdminArea; }

	public String getLocality() { return locality; }
	public void setLocality(String locality) { this.locality = locality; }

	public String getSubLocality() { return subLocality; }
	public void setSubLocality(String subLocality) { this.subLocality = subLocality; }

	public String getThoroughfare() { return thoroughfare; }
	public void setThoroughfare(String thoroughfare) { this.thoroughfare = thoroughfare; }

	public String getSubThoroughfare() { return subThoroughfare; }
	public void setSubThoroughfare(String subThoroughfare) { this.subThoroughfare = subThoroughfare; }

	public String getPremises() { return premises; }
	public void setPremises(String premises) { this.premises = premises; }

	public String getPostalCode() { return postalCode; }
	public void setPostalCode(String postalCode) { this.postalCode = postalCode; }

	public String getCountryName() { return countryName; }
	public void setCountryName(String countryName) { this.countryName = countryName; }

	public String getCountryCode() { return countryCode; }
	public void setCountryCode(String countryCode) { this.countryCode = countryCode; }

	public String getPhone() { return phone; }
	public void setPhone(String phone) { this.phone = phone; }

	public String getUrl() { return url; }
	public void setUrl(String url) { this.url = url; }

	public boolean hasLatitude() { return hasLatitude; }
	public double getLatitude() { return latitude; }
	public void setLatitude(double latitude) {
		this.latitude = latitude;
		this.hasLatitude = true;
	}

	public boolean hasLongitude() { return hasLongitude; }
	public double getLongitude() { return longitude; }
	public void setLongitude(double longitude) {
		this.longitude = longitude;
		this.hasLongitude = true;
	}

	public void clearLatitude() {
		hasLatitude = false;
	}

	public void clearLongitude() {
		hasLongitude = false;
	}

	public int getMaxAddressLineIndex() {
		int max = -1;
		if (addressLine1 != null)
			max = 0;
		if (addressLine2 != null)
			max = 1;
		if (addressLine3 != null)
			max = 2;
		return max;
	}

	@Override
	public String toString() {
		return "Address[" + locale + "]";
	}
}
