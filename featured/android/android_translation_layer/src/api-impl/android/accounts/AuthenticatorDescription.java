package android.accounts;

import android.os.Parcel;
import android.os.Parcelable;

// an account type's authenticator: its package and resources
public class AuthenticatorDescription implements Parcelable {
	public final String type;
	public final int labelId;
	public final int iconId;
	public final int smallIconId;
	public final int accountPreferencesId;
	public final String packageName;
	public final boolean customTokens;

	public AuthenticatorDescription(String type, String packageName, int labelId, int iconId, int smallIconId, int prefId, boolean customTokens) {
		this.type = type;
		this.packageName = packageName;
		this.labelId = labelId;
		this.iconId = iconId;
		this.smallIconId = smallIconId;
		this.accountPreferencesId = prefId;
		this.customTokens = customTokens;
	}

	public AuthenticatorDescription(String type, String packageName, int labelId, int iconId, int smallIconId, int prefId) {
		this(type, packageName, labelId, iconId, smallIconId, prefId, false);
	}

	public static AuthenticatorDescription newKey(String type) {
		return new AuthenticatorDescription(type, null, 0, 0, 0, 0, false);
	}

	@Override
	public void writeToParcel(Parcel out, int flags) {
		out.writeString(type);
		out.writeString(packageName);
		out.writeInt(labelId);
		out.writeInt(iconId);
		out.writeInt(smallIconId);
		out.writeInt(accountPreferencesId);
		out.writeInt(customTokens ? 1 : 0);
	}

	public static final Parcelable.Creator<AuthenticatorDescription> CREATOR =
		new Parcelable.Creator<AuthenticatorDescription>() {
			public AuthenticatorDescription createFromParcel(Parcel in) {
				return new AuthenticatorDescription(in.readString(), in.readString(), in.readInt(), in.readInt(), in.readInt(), in.readInt(), in.readInt() != 0);
			}

			public AuthenticatorDescription[] newArray(int size) {
				return new AuthenticatorDescription[size];
			}
		};

	@Override
	public int hashCode() {
		return type.hashCode();
	}

	@Override
	public boolean equals(Object o) {
		return o instanceof AuthenticatorDescription && type.equals(((AuthenticatorDescription)o).type);
	}

	@Override
	public String toString() {
		return "AuthenticatorDescription {type=" + type + "}";
	}
}
