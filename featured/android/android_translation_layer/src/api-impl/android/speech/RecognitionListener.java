package android.speech;

// android.jar's shape, made by gen.py
public interface RecognitionListener {
	// GEN member:android.speech.RecognitionListener:public abstract void onReadyForSpeech(android.os.Bundle);
	default void onReadyForSpeech(android.os.Bundle arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onBeginningOfSpeech();
	default void onBeginningOfSpeech() {}

	// GEN member:android.speech.RecognitionListener:public abstract void onRmsChanged(float);
	default void onRmsChanged(float arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onBufferReceived(byte[]);
	default void onBufferReceived(byte[] arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onEndOfSpeech();
	default void onEndOfSpeech() {}

	// GEN member:android.speech.RecognitionListener:public abstract void onError(int);
	default void onError(int arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onResults(android.os.Bundle);
	default void onResults(android.os.Bundle arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onPartialResults(android.os.Bundle);
	default void onPartialResults(android.os.Bundle arg0) {}

	// GEN member:android.speech.RecognitionListener:public default void onSegmentResults(android.os.Bundle);
	default void onSegmentResults(android.os.Bundle arg0) {}

	// GEN member:android.speech.RecognitionListener:public default void onEndOfSegmentedSession();
	default void onEndOfSegmentedSession() {}

	// GEN member:android.speech.RecognitionListener:public default void onLanguageDetection(android.os.Bundle);
	default void onLanguageDetection(android.os.Bundle arg0) {}

	// GEN member:android.speech.RecognitionListener:public abstract void onEvent(int, android.os.Bundle);
	default void onEvent(int arg0, android.os.Bundle arg1) {}
}
