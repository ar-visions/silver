package android.accounts;

import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;

/*
 * Where an authenticator's answer goes. One process here: the
 * answer is handed to a listener in it, or kept for whoever asks.
 */
public class AccountAuthenticatorResponse implements Parcelable {
	public interface Listener {
		void onResult(Bundle result);
		void onError(int errorCode, String errorMessage);
	}

	private Listener listener;
	private Bundle result;

	public AccountAuthenticatorResponse() {}

	public AccountAuthenticatorResponse(Listener listener) {
		this.listener = listener;
	}

	public AccountAuthenticatorResponse(Parcel parcel) {}

	public void onResult(Bundle result) {
		this.result = result;
		if (listener != null)
			listener.onResult(result);
	}

	public void onRequestContinued() {}

	public void onError(int errorCode, String errorMessage) {
		Bundle b = new Bundle();
		b.putInt(AccountManager.KEY_ERROR_CODE, errorCode);
		b.putString(AccountManager.KEY_ERROR_MESSAGE, errorMessage);
		this.result = b;
		if (listener != null)
			listener.onError(errorCode, errorMessage);
	}

	public Bundle getResult() {
		return result;
	}

	@Override
	public void writeToParcel(Parcel dest, int flags) {}

	public static final Parcelable.Creator<AccountAuthenticatorResponse> CREATOR =
		new Parcelable.Creator<AccountAuthenticatorResponse>() {
			public AccountAuthenticatorResponse createFromParcel(Parcel source) {
				return new AccountAuthenticatorResponse(source);
			}

			public AccountAuthenticatorResponse[] newArray(int size) {
				return new AccountAuthenticatorResponse[size];
			}
		};
}
