package android.view.textclassifier;

// android.jar's shape, made by gen.py
public interface TextClassifier {
	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String HINT_TEXT_IS_EDITABLE = "android.text_is_editable";
	java.lang.String HINT_TEXT_IS_EDITABLE = "android.text_is_editable";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String HINT_TEXT_IS_NOT_EDITABLE = "android.text_is_not_editable";
	java.lang.String HINT_TEXT_IS_NOT_EDITABLE = "android.text_is_not_editable";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_ADDRESS = "address";
	java.lang.String TYPE_ADDRESS = "address";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_DATE = "date";
	java.lang.String TYPE_DATE = "date";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_DATE_TIME = "datetime";
	java.lang.String TYPE_DATE_TIME = "datetime";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_EMAIL = "email";
	java.lang.String TYPE_EMAIL = "email";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_FLIGHT_NUMBER = "flight";
	java.lang.String TYPE_FLIGHT_NUMBER = "flight";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_OTHER = "other";
	java.lang.String TYPE_OTHER = "other";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_PHONE = "phone";
	java.lang.String TYPE_PHONE = "phone";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_UNKNOWN = "";
	java.lang.String TYPE_UNKNOWN = "";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String TYPE_URL = "url";
	java.lang.String TYPE_URL = "url";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_CLIPBOARD = "clipboard";
	java.lang.String WIDGET_TYPE_CLIPBOARD = "clipboard";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_CUSTOM_EDITTEXT = "customedit";
	java.lang.String WIDGET_TYPE_CUSTOM_EDITTEXT = "customedit";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_CUSTOM_TEXTVIEW = "customview";
	java.lang.String WIDGET_TYPE_CUSTOM_TEXTVIEW = "customview";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_CUSTOM_UNSELECTABLE_TEXTVIEW = "nosel-customview";
	java.lang.String WIDGET_TYPE_CUSTOM_UNSELECTABLE_TEXTVIEW = "nosel-customview";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_EDITTEXT = "edittext";
	java.lang.String WIDGET_TYPE_EDITTEXT = "edittext";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_EDIT_WEBVIEW = "edit-webview";
	java.lang.String WIDGET_TYPE_EDIT_WEBVIEW = "edit-webview";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_NOTIFICATION = "notification";
	java.lang.String WIDGET_TYPE_NOTIFICATION = "notification";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_TEXTVIEW = "textview";
	java.lang.String WIDGET_TYPE_TEXTVIEW = "textview";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_UNKNOWN = "unknown";
	java.lang.String WIDGET_TYPE_UNKNOWN = "unknown";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_UNSELECTABLE_TEXTVIEW = "nosel-textview";
	java.lang.String WIDGET_TYPE_UNSELECTABLE_TEXTVIEW = "nosel-textview";

	// GEN member:android.view.textclassifier.TextClassifier:public static final java.lang.String WIDGET_TYPE_WEBVIEW = "webview";
	java.lang.String WIDGET_TYPE_WEBVIEW = "webview";

	// GEN member:android.view.textclassifier.TextClassifier:public default android.view.textclassifier.TextSelection suggestSelection(android.view.textclassifier.TextSelection$Request);
	default android.view.textclassifier.TextSelection suggestSelection(android.view.textclassifier.TextSelection.Request arg0) { return null; }

	// GEN member:android.view.textclassifier.TextClassifier:public default android.view.textclassifier.TextSelection suggestSelection(java.lang.CharSequence, int, int, android.os.LocaleList);
	default android.view.textclassifier.TextSelection suggestSelection(java.lang.CharSequence arg0, int arg1, int arg2, android.os.LocaleList arg3) { return null; }

	// GEN member:android.view.textclassifier.TextClassifier:public default android.view.textclassifier.TextClassification classifyText(android.view.textclassifier.TextClassification$Request);
	default android.view.textclassifier.TextClassification classifyText(android.view.textclassifier.TextClassification.Request arg0) { return null; }

	// GEN member:android.view.textclassifier.TextClassifier:public default android.view.textclassifier.TextClassification classifyText(java.lang.CharSequence, int, int, android.os.LocaleList);
	default android.view.textclassifier.TextClassification classifyText(java.lang.CharSequence arg0, int arg1, int arg2, android.os.LocaleList arg3) { return null; }

	// GEN member:android.view.textclassifier.TextClassifier:public default int getMaxGenerateLinksTextLength();
	default int getMaxGenerateLinksTextLength() { return 0; }

	// GEN member:android.view.textclassifier.TextClassifier:public default void destroy();
	default void destroy() {}

	// GEN member:android.view.textclassifier.TextClassifier:public default boolean isDestroyed();
	default boolean isDestroyed() { return false; }

	// GEN member:android.view.textclassifier.TextClassifier:public default android.view.textclassifier.TextLinks generateLinks(android.view.textclassifier.TextLinks$Request);
	default android.view.textclassifier.TextLinks generateLinks(android.view.textclassifier.TextLinks.Request arg0) { return null; }
}
