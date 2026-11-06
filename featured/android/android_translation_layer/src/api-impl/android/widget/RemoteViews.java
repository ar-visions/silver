package android.widget;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;

public class RemoteViews implements android.os.Parcelable, android.view.LayoutInflater.Filter {

	public RemoteViews(String packageName, int layoutId) {}

	public View apply(Context context, ViewGroup parent) {
		return null;
	}

	public void setProgressBar(int viewId, int max, int progress, boolean indeterminate) {}

	public void setTextViewText(int viewId, CharSequence text) {}

	public void setImageViewResource(int viewId, int resId) {}

	public void setContentDescription(int viewId, CharSequence text) {}

	public void setOnClickPendingIntent(int viewId, PendingIntent pendingIntent) {}

	public void setViewVisibility(int viewId, int visibility) {}

	public void setImageViewBitmap(int viewId, Bitmap bitmap) {}

	public void removeAllViews(int viewId) {}

	public RemoteViews clone() {
		return new RemoteViews(null, 0);
	}

	public void addView(int viewId, RemoteViews child) {}

	public void setViewPadding(int viewId, int left, int top, int right, int bottom) {}

	// GEN class:android.widget.RemoteViews$DrawInstructions
	public final class DrawInstructions {
		protected DrawInstructions() {}
	
	// GEN class:android.widget.RemoteViews$DrawInstructions$Builder
	public final class Builder {
		// GEN member:android.widget.RemoteViews$DrawInstructions$Builder:public android.widget.RemoteViews$DrawInstructions$Builder(java.util.List<byte[]>);
		public Builder(java.util.List<byte[]> arg0) {}
	
		// GEN member:android.widget.RemoteViews$DrawInstructions$Builder:public android.widget.RemoteViews$DrawInstructions build();
		public android.widget.RemoteViews.DrawInstructions build() { return null; }
	
		protected Builder() {}
	}

}


	// GEN class:android.widget.RemoteViews$RemoteCollectionItems
	public final class RemoteCollectionItems implements android.os.Parcelable {
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public int describeContents();
		public int describeContents() { return 0; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public void writeToParcel(android.os.Parcel, int);
		public void writeToParcel(android.os.Parcel arg0, int arg1) {}
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public long getItemId(int);
		public long getItemId(int arg0) { return 0; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public android.widget.RemoteViews getItemView(int);
		public android.widget.RemoteViews getItemView(int arg0) { return null; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public int getItemCount();
		public int getItemCount() { return 0; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public int getViewTypeCount();
		public int getViewTypeCount() { return 0; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems:public boolean hasStableIds();
		public boolean hasStableIds() { return false; }
	
		protected RemoteCollectionItems() {}
	
	// GEN class:android.widget.RemoteViews$RemoteCollectionItems$Builder
	public final class Builder {
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems$Builder:public android.widget.RemoteViews$RemoteCollectionItems$Builder();
		public Builder() {}
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems$Builder:public android.widget.RemoteViews$RemoteCollectionItems$Builder addItem(long, android.widget.RemoteViews);
		public android.widget.RemoteViews.RemoteCollectionItems.Builder addItem(long arg0, android.widget.RemoteViews arg1) { return this; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems$Builder:public android.widget.RemoteViews$RemoteCollectionItems$Builder setHasStableIds(boolean);
		public android.widget.RemoteViews.RemoteCollectionItems.Builder setHasStableIds(boolean arg0) { return this; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems$Builder:public android.widget.RemoteViews$RemoteCollectionItems$Builder setViewTypeCount(int);
		public android.widget.RemoteViews.RemoteCollectionItems.Builder setViewTypeCount(int arg0) { return this; }
	
		// GEN member:android.widget.RemoteViews$RemoteCollectionItems$Builder:public android.widget.RemoteViews$RemoteCollectionItems build();
		public android.widget.RemoteViews.RemoteCollectionItems build() { return null; }
	}

}


	// GEN class:android.widget.RemoteViews$RemoteResponse
	public class RemoteResponse {
		// GEN member:android.widget.RemoteViews$RemoteResponse:public android.widget.RemoteViews$RemoteResponse();
		public RemoteResponse() {}
	
		// GEN member:android.widget.RemoteViews$RemoteResponse:public android.widget.RemoteViews$RemoteResponse addSharedElement(int, java.lang.String);
		public android.widget.RemoteViews.RemoteResponse addSharedElement(int arg0, java.lang.String arg1) { return null; }
	}


	// GEN member:android.widget.RemoteViews:public void addStableView(int, android.widget.RemoteViews, int);
	public void addStableView(int arg0, android.widget.RemoteViews arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public static final android.os.Parcelable$Creator<android.widget.RemoteViews> CREATOR;
	public static final android.os.Parcelable.Creator<android.widget.RemoteViews> CREATOR = null;

	// GEN member:android.widget.RemoteViews:public int getLayoutId();
	public int getLayoutId() { return 0; }

	// GEN member:android.widget.RemoteViews:public android.widget.RemoteViews(android.widget.RemoteViews$DrawInstructions);
	public RemoteViews(android.widget.RemoteViews.DrawInstructions arg0) { this((java.lang.String)null, (int)0); }

	// GEN member:android.widget.RemoteViews:public android.widget.RemoteViews(android.widget.RemoteViews, android.widget.RemoteViews);
	public RemoteViews(android.widget.RemoteViews arg0, android.widget.RemoteViews arg1) { this((java.lang.String)null, (int)0); }

	// GEN member:android.widget.RemoteViews:public android.widget.RemoteViews(android.widget.RemoteViews);
	public RemoteViews(android.widget.RemoteViews arg0) { this((java.lang.String)null, (int)0); }

	// GEN member:android.widget.RemoteViews:public android.widget.RemoteViews(java.lang.String, int, int);
	public RemoteViews(java.lang.String arg0, int arg1, int arg2) { this((java.lang.String)null, (int)0); }

	// GEN member:android.widget.RemoteViews:public android.widget.RemoteViews(java.util.Map<android.util.SizeF, android.widget.RemoteViews>);
	public RemoteViews(java.util.Map<android.util.SizeF, android.widget.RemoteViews> arg0) { this((java.lang.String)null, (int)0); }

	// GEN member:android.widget.RemoteViews:public void setBoolean(int, java.lang.String, boolean);
	public void setBoolean(int arg0, java.lang.String arg1, boolean arg2) {}

	// GEN member:android.widget.RemoteViews:public void setColor(int, java.lang.String, int);
	public void setColor(int arg0, java.lang.String arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setColorStateList(int, java.lang.String, int);
	public void setColorStateList(int arg0, java.lang.String arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setColorStateList(int, java.lang.String, android.content.res.ColorStateList);
	public void setColorStateList(int arg0, java.lang.String arg1, android.content.res.ColorStateList arg2) {}

	// GEN member:android.widget.RemoteViews:public void setEmptyView(int, int);
	public void setEmptyView(int arg0, int arg1) {}

	// GEN member:android.widget.RemoteViews:public void setInt(int, java.lang.String, int);
	public void setInt(int arg0, java.lang.String arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setOnCheckedChangeResponse(int, android.widget.RemoteViews$RemoteResponse);
	public void setOnCheckedChangeResponse(int arg0, android.widget.RemoteViews.RemoteResponse arg1) {}

	// GEN member:android.widget.RemoteViews:public void setOnClickFillInIntent(int, android.content.Intent);
	public void setOnClickFillInIntent(int arg0, android.content.Intent arg1) {}

	// GEN member:android.widget.RemoteViews:public void setPendingIntentTemplate(int, android.app.PendingIntent);
	public void setPendingIntentTemplate(int arg0, android.app.PendingIntent arg1) {}

	// GEN member:android.widget.RemoteViews:public void setRemoteAdapter(int, android.content.Intent);
	public void setRemoteAdapter(int arg0, android.content.Intent arg1) {}

	// GEN member:android.widget.RemoteViews:public void setRemoteAdapter(int, android.widget.RemoteViews$RemoteCollectionItems);
	public void setRemoteAdapter(int arg0, android.widget.RemoteViews.RemoteCollectionItems arg1) {}

	// GEN member:android.widget.RemoteViews:public void setTextColor(int, int);
	public void setTextColor(int arg0, int arg1) {}

	// GEN member:android.widget.RemoteViews:public void setTextViewCompoundDrawables(int, int, int, int, int);
	public void setTextViewCompoundDrawables(int arg0, int arg1, int arg2, int arg3, int arg4) {}

	// GEN member:android.widget.RemoteViews:public void setTextViewCompoundDrawablesRelative(int, int, int, int, int);
	public void setTextViewCompoundDrawablesRelative(int arg0, int arg1, int arg2, int arg3, int arg4) {}

	// GEN member:android.widget.RemoteViews:public void setTextViewTextSize(int, int, float);
	public void setTextViewTextSize(int arg0, int arg1, float arg2) {}

	// GEN member:android.widget.RemoteViews:public void setViewLayoutHeightDimen(int, int);
	public void setViewLayoutHeightDimen(int arg0, int arg1) {}

	// GEN member:android.widget.RemoteViews:public void setViewLayoutHeight(int, float, int);
	public void setViewLayoutHeight(int arg0, float arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setViewLayoutMargin(int, int, float, int);
	public void setViewLayoutMargin(int arg0, int arg1, float arg2, int arg3) {}

	// GEN member:android.widget.RemoteViews:public void setViewLayoutWidthDimen(int, int);
	public void setViewLayoutWidthDimen(int arg0, int arg1) {}

	// GEN member:android.widget.RemoteViews:public void setViewLayoutWidth(int, float, int);
	public void setViewLayoutWidth(int arg0, float arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setViewOutlinePreferredRadiusDimen(int, int);
	public void setViewOutlinePreferredRadiusDimen(int arg0, int arg1) {}

	// GEN member:android.widget.RemoteViews:public void setViewOutlinePreferredRadius(int, float, int);
	public void setViewOutlinePreferredRadius(int arg0, float arg1, int arg2) {}

	// GEN member:android.widget.RemoteViews:public void setCharSequence(int, java.lang.String, java.lang.CharSequence);
	public void setCharSequence(int arg0, java.lang.String arg1, java.lang.CharSequence arg2) {}

	// GEN member:android.widget.RemoteViews:public void setDisplayedChild(int, int);
	public void setDisplayedChild(int arg0, int arg1) {}
}
