package android.accounts;

import android.app.Activity;
import android.os.Bundle;

// a sign-in screen that answers its authenticator when it ends
public class AccountAuthenticatorActivity extends Activity {
	private AccountAuthenticatorResponse response;
	private Bundle result;

	public final void setAccountAuthenticatorResult(Bundle result) {
		this.result = result;
	}

	@Override
	protected void onCreate(Bundle icicle) {
		super.onCreate(icicle);
		response = getIntent() != null ? (AccountAuthenticatorResponse)getIntent().getParcelableExtra(AccountManager.KEY_ACCOUNT_AUTHENTICATOR_RESPONSE) : null;
		if (response != null)
			response.onRequestContinued();
	}

	@Override
	public void finish() {
		if (response != null) {
			if (result != null)
				response.onResult(result);
			else
				response.onError(AccountManager.ERROR_CODE_CANCELED, "canceled");
			response = null;
		}
		super.finish();
	}
}
