package android.speech;

// android.jar's shape, made by gen.py
public class SpeechRecognizer {
	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String CONFIDENCE_SCORES = "confidence_scores";
	public static final java.lang.String CONFIDENCE_SCORES = "confidence_scores";

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String DETECTED_LANGUAGE = "detected_language";
	public static final java.lang.String DETECTED_LANGUAGE = "detected_language";

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_AUDIO = 3;
	public static final int ERROR_AUDIO = 3;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_CANNOT_CHECK_SUPPORT = 14;
	public static final int ERROR_CANNOT_CHECK_SUPPORT = 14;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_CANNOT_LISTEN_TO_DOWNLOAD_EVENTS = 15;
	public static final int ERROR_CANNOT_LISTEN_TO_DOWNLOAD_EVENTS = 15;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_CLIENT = 5;
	public static final int ERROR_CLIENT = 5;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_INSUFFICIENT_PERMISSIONS = 9;
	public static final int ERROR_INSUFFICIENT_PERMISSIONS = 9;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_LANGUAGE_NOT_SUPPORTED = 12;
	public static final int ERROR_LANGUAGE_NOT_SUPPORTED = 12;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_LANGUAGE_UNAVAILABLE = 13;
	public static final int ERROR_LANGUAGE_UNAVAILABLE = 13;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_NETWORK = 2;
	public static final int ERROR_NETWORK = 2;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_NETWORK_TIMEOUT = 1;
	public static final int ERROR_NETWORK_TIMEOUT = 1;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_NO_MATCH = 7;
	public static final int ERROR_NO_MATCH = 7;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_RECOGNIZER_BUSY = 8;
	public static final int ERROR_RECOGNIZER_BUSY = 8;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_SERVER = 4;
	public static final int ERROR_SERVER = 4;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_SERVER_DISCONNECTED = 11;
	public static final int ERROR_SERVER_DISCONNECTED = 11;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_SPEECH_TIMEOUT = 6;
	public static final int ERROR_SPEECH_TIMEOUT = 6;

	// GEN member:android.speech.SpeechRecognizer:public static final int ERROR_TOO_MANY_REQUESTS = 10;
	public static final int ERROR_TOO_MANY_REQUESTS = 10;

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String LANGUAGE_DETECTION_CONFIDENCE_LEVEL = "language_detection_confidence_level";
	public static final java.lang.String LANGUAGE_DETECTION_CONFIDENCE_LEVEL = "language_detection_confidence_level";

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_CONFIDENT = 2;
	public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_CONFIDENT = 2;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_HIGHLY_CONFIDENT = 3;
	public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_HIGHLY_CONFIDENT = 3;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_NOT_CONFIDENT = 1;
	public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_NOT_CONFIDENT = 1;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_UNKNOWN = 0;
	public static final int LANGUAGE_DETECTION_CONFIDENCE_LEVEL_UNKNOWN = 0;

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String LANGUAGE_SWITCH_RESULT = "language_switch_result";
	public static final java.lang.String LANGUAGE_SWITCH_RESULT = "language_switch_result";

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_SWITCH_RESULT_FAILED = 2;
	public static final int LANGUAGE_SWITCH_RESULT_FAILED = 2;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_SWITCH_RESULT_NOT_ATTEMPTED = 0;
	public static final int LANGUAGE_SWITCH_RESULT_NOT_ATTEMPTED = 0;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_SWITCH_RESULT_SKIPPED_NO_MODEL = 3;
	public static final int LANGUAGE_SWITCH_RESULT_SKIPPED_NO_MODEL = 3;

	// GEN member:android.speech.SpeechRecognizer:public static final int LANGUAGE_SWITCH_RESULT_SUCCEEDED = 1;
	public static final int LANGUAGE_SWITCH_RESULT_SUCCEEDED = 1;

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String RECOGNITION_PARTS = "recognition_parts";
	public static final java.lang.String RECOGNITION_PARTS = "recognition_parts";

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String RESULTS_ALTERNATIVES = "results_alternatives";
	public static final java.lang.String RESULTS_ALTERNATIVES = "results_alternatives";

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String RESULTS_RECOGNITION = "results_recognition";
	public static final java.lang.String RESULTS_RECOGNITION = "results_recognition";

	// GEN member:android.speech.SpeechRecognizer:public static final java.lang.String TOP_LOCALE_ALTERNATIVES = "top_locale_alternatives";
	public static final java.lang.String TOP_LOCALE_ALTERNATIVES = "top_locale_alternatives";

	// GEN member:android.speech.SpeechRecognizer:public static boolean isRecognitionAvailable(android.content.Context);
	public static boolean isRecognitionAvailable(android.content.Context arg0) { return false; }

	// GEN member:android.speech.SpeechRecognizer:public static boolean isOnDeviceRecognitionAvailable(android.content.Context);
	public static boolean isOnDeviceRecognitionAvailable(android.content.Context arg0) { return false; }

	// GEN member:android.speech.SpeechRecognizer:public static android.speech.SpeechRecognizer createSpeechRecognizer(android.content.Context);
	public static android.speech.SpeechRecognizer createSpeechRecognizer(android.content.Context arg0) { return null; }

	// GEN member:android.speech.SpeechRecognizer:public static android.speech.SpeechRecognizer createSpeechRecognizer(android.content.Context, android.content.ComponentName);
	public static android.speech.SpeechRecognizer createSpeechRecognizer(android.content.Context arg0, android.content.ComponentName arg1) { return null; }

	// GEN member:android.speech.SpeechRecognizer:public static android.speech.SpeechRecognizer createOnDeviceSpeechRecognizer(android.content.Context);
	public static android.speech.SpeechRecognizer createOnDeviceSpeechRecognizer(android.content.Context arg0) { return null; }

	// GEN member:android.speech.SpeechRecognizer:public void startListening(android.content.Intent);
	public void startListening(android.content.Intent arg0) {}

	// GEN member:android.speech.SpeechRecognizer:public void stopListening();
	public void stopListening() {}

	// GEN member:android.speech.SpeechRecognizer:public void cancel();
	public void cancel() {}

	// GEN member:android.speech.SpeechRecognizer:public void triggerModelDownload(android.content.Intent);
	public void triggerModelDownload(android.content.Intent arg0) {}

	// GEN member:android.speech.SpeechRecognizer:public void destroy();
	public void destroy() {}

	protected SpeechRecognizer() {}

	// GEN member:android.speech.SpeechRecognizer:public void setRecognitionListener(android.speech.RecognitionListener);
	public void setRecognitionListener(android.speech.RecognitionListener arg0) {}
}
