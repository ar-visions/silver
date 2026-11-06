package android.widget;

// android.jar's shape, made by ifacegen.py
public interface WrapperListAdapter extends android.widget.ListAdapter {
	default android.widget.ListAdapter getWrappedAdapter() { return null; }
}
