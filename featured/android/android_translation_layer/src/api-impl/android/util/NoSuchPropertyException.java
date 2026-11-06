package android.util;

// a Property.of name with no getter, setter or field
public class NoSuchPropertyException extends RuntimeException {
	public NoSuchPropertyException(String s) {
		super(s);
	}
}
