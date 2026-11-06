package android.speech.tts;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

public class TextToSpeech {
	public static final int ERROR = -1;
	public static final int SUCCESS = 0;
	public static final int LANG_NOT_SUPPORTED = -2;
	public static final int QUEUE_FLUSH = 0;
	public static final int QUEUE_ADD = 1;

	// no engine here: init always reports ERROR
	public TextToSpeech(Context context, TextToSpeech.OnInitListener listener, String engine) {
		this(context, listener);
	}

	public TextToSpeech(Context context, TextToSpeech.OnInitListener listener) {
		new Handler(Looper.getMainLooper()).post(new Runnable() {
			@Override
			public void run() {
				listener.onInit(ERROR);
			}
		});
	}

	public int setOnUtteranceCompletedListener(TextToSpeech.OnUtteranceCompletedListener listener) {
		return ERROR;
	}

	public int setOnUtteranceProgressListener(UtteranceProgressListener listener) {
		return ERROR;
	}

	public int speak(CharSequence text, int queueMode, Bundle params, String utteranceId) {
		return ERROR;
	}

	public int speak(String text, int queueMode, java.util.HashMap<String, String> params) {
		return ERROR;
	}

	public boolean isSpeaking() {
		return false;
	}

	public int setLanguage(java.util.Locale loc) {
		return LANG_NOT_SUPPORTED;
	}

	public int isLanguageAvailable(java.util.Locale loc) {
		return LANG_NOT_SUPPORTED;
	}

	public int setSpeechRate(float speechRate) {
		return ERROR;
	}

	public int setPitch(float pitch) {
		return ERROR;
	}

	public java.util.Set<java.util.Locale> getAvailableLanguages() {
		return new java.util.HashSet<>();
	}

	public java.util.Set<Voice> getVoices() {
		return new java.util.HashSet<>();
	}

	public int setVoice(Voice voice) {
		return ERROR;
	}

	// no engine is installed
	public String getDefaultEngine() {
		return "";
	}

	public int synthesizeToFile(CharSequence text, Bundle params, java.io.File file, String utteranceId) {
		return ERROR;
	}

	public void shutdown() {
	}

	public int stop() {
		return ERROR;
	}

	public static interface OnInitListener {
		abstract void onInit(int status);
	}

	public static interface OnUtteranceCompletedListener {
		public abstract void onUtteranceCompleted(String utteranceId);
	}

	// GEN member:android.speech.tts.TextToSpeech:public java.util.Locale getLanguage();
	public java.util.Locale getLanguage() { return null; }

	// GEN member:android.speech.tts.TextToSpeech:public android.speech.tts.Voice getVoice();
	public android.speech.tts.Voice getVoice() { return null; }
}
