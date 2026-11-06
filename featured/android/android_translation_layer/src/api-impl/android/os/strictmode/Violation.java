package android.os.strictmode;

// what a policy saw; none are raised here
public abstract class Violation extends Throwable {
	Violation(String message) {
		super(message);
	}
}
