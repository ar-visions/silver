package android.media;

import android.view.Display;

public class MediaRouter {
	public static final int ROUTE_TYPE_LIVE_VIDEO = 0x2;

	public static class RouteInfo {
		public Display getPresentationDisplay() {
			return new Display();
		}
	
	// GEN member:android.media.MediaRouter$RouteInfo:public java.lang.CharSequence getDescription();
	public java.lang.CharSequence getDescription() { return null; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getDeviceType();
	public int getDeviceType() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public java.lang.CharSequence getName(android.content.Context);
	public java.lang.CharSequence getName(android.content.Context arg0) { return null; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getPlaybackStream();
	public int getPlaybackStream() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getPlaybackType();
	public int getPlaybackType() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getSupportedTypes();
	public int getSupportedTypes() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public java.lang.Object getTag();
	public java.lang.Object getTag() { return null; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getVolume();
	public int getVolume() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getVolumeHandling();
	public int getVolumeHandling() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public int getVolumeMax();
	public int getVolumeMax() { return 0; }

	// GEN member:android.media.MediaRouter$RouteInfo:public boolean isConnecting();
	public boolean isConnecting() { return false; }

	// GEN member:android.media.MediaRouter$RouteInfo:public boolean isEnabled();
	public boolean isEnabled() { return false; }

	// GEN member:android.media.MediaRouter$RouteInfo:public void requestSetVolume(int);
	public void requestSetVolume(int arg0) {}

	// GEN member:android.media.MediaRouter$RouteInfo:public void requestUpdateVolume(int);
	public void requestUpdateVolume(int arg0) {}
}

	public RouteInfo getSelectedRoute(int type) {
		return new RouteInfo();
	}

	// GEN class:android.media.MediaRouter$Callback
	public abstract class Callback {
		// GEN member:android.media.MediaRouter$Callback:public android.media.MediaRouter$Callback();
		public Callback() {}
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteSelected(android.media.MediaRouter, int, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteSelected(android.media.MediaRouter arg0, int arg1, android.media.MediaRouter.RouteInfo arg2);
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteUnselected(android.media.MediaRouter, int, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteUnselected(android.media.MediaRouter arg0, int arg1, android.media.MediaRouter.RouteInfo arg2);
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteAdded(android.media.MediaRouter, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteAdded(android.media.MediaRouter arg0, android.media.MediaRouter.RouteInfo arg1);
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteRemoved(android.media.MediaRouter, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteRemoved(android.media.MediaRouter arg0, android.media.MediaRouter.RouteInfo arg1);
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteChanged(android.media.MediaRouter, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteChanged(android.media.MediaRouter arg0, android.media.MediaRouter.RouteInfo arg1);
	
		// GEN member:android.media.MediaRouter$Callback:public abstract void onRouteVolumeChanged(android.media.MediaRouter, android.media.MediaRouter$RouteInfo);
		public abstract void onRouteVolumeChanged(android.media.MediaRouter arg0, android.media.MediaRouter.RouteInfo arg1);
	
		// GEN member:android.media.MediaRouter$Callback:public void onRoutePresentationDisplayChanged(android.media.MediaRouter, android.media.MediaRouter$RouteInfo);
		public void onRoutePresentationDisplayChanged(android.media.MediaRouter arg0, android.media.MediaRouter.RouteInfo arg1) {}
	}


	// GEN class:android.media.MediaRouter$RouteCategory
	public class RouteCategory {
		// GEN member:android.media.MediaRouter$RouteCategory:public java.lang.CharSequence getName();
		public java.lang.CharSequence getName() { return null; }
	
		// GEN member:android.media.MediaRouter$RouteCategory:public java.lang.CharSequence getName(android.content.Context);
		public java.lang.CharSequence getName(android.content.Context arg0) { return null; }
	
		// GEN member:android.media.MediaRouter$RouteCategory:public java.util.List<android.media.MediaRouter$RouteInfo> getRoutes(java.util.List<android.media.MediaRouter$RouteInfo>);
		public java.util.List<android.media.MediaRouter.RouteInfo> getRoutes(java.util.List<android.media.MediaRouter.RouteInfo> arg0) { return new java.util.ArrayList<>(); }
	
		// GEN member:android.media.MediaRouter$RouteCategory:public int getSupportedTypes();
		public int getSupportedTypes() { return 0; }
	
		// GEN member:android.media.MediaRouter$RouteCategory:public boolean isGroupable();
		public boolean isGroupable() { return false; }
	
		// GEN member:android.media.MediaRouter$RouteCategory:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		protected RouteCategory() {}
	}


	// GEN class:android.media.MediaRouter$UserRouteInfo
	public class UserRouteInfo extends android.media.MediaRouter.RouteInfo {
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setName(java.lang.CharSequence);
		public void setName(java.lang.CharSequence arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setName(int);
		public void setName(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setDescription(java.lang.CharSequence);
		public void setDescription(java.lang.CharSequence arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setStatus(java.lang.CharSequence);
		public void setStatus(java.lang.CharSequence arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setIconDrawable(android.graphics.drawable.Drawable);
		public void setIconDrawable(android.graphics.drawable.Drawable arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setIconResource(int);
		public void setIconResource(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setVolumeCallback(android.media.MediaRouter$VolumeCallback);
		public void setVolumeCallback(android.media.MediaRouter.VolumeCallback arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setPlaybackType(int);
		public void setPlaybackType(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setVolumeHandling(int);
		public void setVolumeHandling(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setVolume(int);
		public void setVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void requestSetVolume(int);
		public void requestSetVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void requestUpdateVolume(int);
		public void requestUpdateVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setVolumeMax(int);
		public void setVolumeMax(int arg0) {}
	
		// GEN member:android.media.MediaRouter$UserRouteInfo:public void setPlaybackStream(int);
		public void setPlaybackStream(int arg0) {}
	
		protected UserRouteInfo() {}
	}


	// GEN class:android.media.MediaRouter$VolumeCallback
	public abstract class VolumeCallback {
		// GEN member:android.media.MediaRouter$VolumeCallback:public android.media.MediaRouter$VolumeCallback();
		public VolumeCallback() {}
	
		// GEN member:android.media.MediaRouter$VolumeCallback:public abstract void onVolumeUpdateRequest(android.media.MediaRouter$RouteInfo, int);
		public abstract void onVolumeUpdateRequest(android.media.MediaRouter.RouteInfo arg0, int arg1);
	
		// GEN member:android.media.MediaRouter$VolumeCallback:public abstract void onVolumeSetRequest(android.media.MediaRouter$RouteInfo, int);
		public abstract void onVolumeSetRequest(android.media.MediaRouter.RouteInfo arg0, int arg1);
	}


	// GEN member:android.media.MediaRouter:public void addCallback(int, android.media.MediaRouter$Callback, int);
	public void addCallback(int arg0, android.media.MediaRouter.Callback arg1, int arg2) {}

	// GEN member:android.media.MediaRouter:public void addUserRoute(android.media.MediaRouter$UserRouteInfo);
	public void addUserRoute(android.media.MediaRouter.UserRouteInfo arg0) {}

	// GEN member:android.media.MediaRouter:public android.media.MediaRouter$RouteCategory createRouteCategory(java.lang.CharSequence, boolean);
	public android.media.MediaRouter.RouteCategory createRouteCategory(java.lang.CharSequence arg0, boolean arg1) { return null; }

	// GEN member:android.media.MediaRouter:public android.media.MediaRouter$UserRouteInfo createUserRoute(android.media.MediaRouter$RouteCategory);
	public android.media.MediaRouter.UserRouteInfo createUserRoute(android.media.MediaRouter.RouteCategory arg0) { return null; }

	// GEN member:android.media.MediaRouter:public android.media.MediaRouter$RouteInfo getDefaultRoute();
	public android.media.MediaRouter.RouteInfo getDefaultRoute() { return null; }

	// GEN member:android.media.MediaRouter:public android.media.MediaRouter$RouteInfo getRouteAt(int);
	public android.media.MediaRouter.RouteInfo getRouteAt(int arg0) { return null; }

	// GEN member:android.media.MediaRouter:public int getRouteCount();
	public int getRouteCount() { return 0; }

	// GEN member:android.media.MediaRouter:public void removeCallback(android.media.MediaRouter$Callback);
	public void removeCallback(android.media.MediaRouter.Callback arg0) {}

	// GEN member:android.media.MediaRouter:public void removeUserRoute(android.media.MediaRouter$UserRouteInfo);
	public void removeUserRoute(android.media.MediaRouter.UserRouteInfo arg0) {}

	// GEN member:android.media.MediaRouter:public void selectRoute(int, android.media.MediaRouter$RouteInfo);
	public void selectRoute(int arg0, android.media.MediaRouter.RouteInfo arg1) {}

	// GEN class:android.media.MediaRouter$RouteGroup
	public class RouteGroup extends android.media.MediaRouter.RouteInfo {
		// GEN member:android.media.MediaRouter$RouteGroup:public void addRoute(android.media.MediaRouter$RouteInfo);
		public void addRoute(android.media.MediaRouter.RouteInfo arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void addRoute(android.media.MediaRouter$RouteInfo, int);
		public void addRoute(android.media.MediaRouter.RouteInfo arg0, int arg1) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void removeRoute(android.media.MediaRouter$RouteInfo);
		public void removeRoute(android.media.MediaRouter.RouteInfo arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void removeRoute(int);
		public void removeRoute(int arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public int getRouteCount();
		public int getRouteCount() { return 0; }
	
		// GEN member:android.media.MediaRouter$RouteGroup:public android.media.MediaRouter$RouteInfo getRouteAt(int);
		public android.media.MediaRouter.RouteInfo getRouteAt(int arg0) { return null; }
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void setIconDrawable(android.graphics.drawable.Drawable);
		public void setIconDrawable(android.graphics.drawable.Drawable arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void setIconResource(int);
		public void setIconResource(int arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void requestSetVolume(int);
		public void requestSetVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public void requestUpdateVolume(int);
		public void requestUpdateVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter$RouteGroup:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		protected RouteGroup() {}
	}

}
