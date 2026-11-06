package android.media;

// android.jar's shape, made by gen.py
public final class MediaRouter2 {
	// GEN member:android.media.MediaRouter2:public static android.media.MediaRouter2 getInstance(android.content.Context);
	public static android.media.MediaRouter2 getInstance(android.content.Context arg0) { return null; }

	// GEN member:android.media.MediaRouter2:public static android.media.MediaRouter2 getInstance(android.content.Context, java.lang.String, java.util.concurrent.Executor, java.lang.Runnable);
	public static android.media.MediaRouter2 getInstance(android.content.Context arg0, java.lang.String arg1, java.util.concurrent.Executor arg2, java.lang.Runnable arg3) { return null; }

	// GEN member:android.media.MediaRouter2:public boolean showSystemOutputSwitcher();
	public boolean showSystemOutputSwitcher() { return false; }

	// GEN member:android.media.MediaRouter2:public java.util.List<android.media.MediaRoute2Info> getRoutes();
	public java.util.List<android.media.MediaRoute2Info> getRoutes() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaRouter2:public void transferTo(android.media.MediaRoute2Info);
	public void transferTo(android.media.MediaRoute2Info arg0) {}

	// GEN member:android.media.MediaRouter2:public void stop();
	public void stop() {}

	// GEN member:android.media.MediaRouter2:public android.media.MediaRouter2$RoutingController getSystemController();
	public android.media.MediaRouter2.RoutingController getSystemController() { return null; }

	// GEN member:android.media.MediaRouter2:public android.media.MediaRouter2$RoutingController getController(java.lang.String);
	public android.media.MediaRouter2.RoutingController getController(java.lang.String arg0) { return null; }

	// GEN member:android.media.MediaRouter2:public java.util.List<android.media.MediaRouter2$RoutingController> getControllers();
	public java.util.List<android.media.MediaRouter2.RoutingController> getControllers() { return new java.util.ArrayList<>(); }

	// GEN member:android.media.MediaRouter2:public void setRouteVolume(android.media.MediaRoute2Info, int);
	public void setRouteVolume(android.media.MediaRoute2Info arg0, int arg1) {}

	protected MediaRouter2() {}

	// GEN class:android.media.MediaRouter2$RoutingController
	public class RoutingController {
		// GEN member:android.media.MediaRouter2$RoutingController:public java.lang.String getId();
		public java.lang.String getId() { return null; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public android.os.Bundle getControlHints();
		public android.os.Bundle getControlHints() { return null; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public java.util.List<android.media.MediaRoute2Info> getSelectedRoutes();
		public java.util.List<android.media.MediaRoute2Info> getSelectedRoutes() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public java.util.List<android.media.MediaRoute2Info> getSelectableRoutes();
		public java.util.List<android.media.MediaRoute2Info> getSelectableRoutes() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public java.util.List<android.media.MediaRoute2Info> getDeselectableRoutes();
		public java.util.List<android.media.MediaRoute2Info> getDeselectableRoutes() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public java.util.List<android.media.MediaRoute2Info> getTransferableRoutes();
		public java.util.List<android.media.MediaRoute2Info> getTransferableRoutes() { return new java.util.ArrayList<>(); }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public boolean wasTransferInitiatedBySelf();
		public boolean wasTransferInitiatedBySelf() { return false; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public android.media.RoutingSessionInfo getRoutingSessionInfo();
		public android.media.RoutingSessionInfo getRoutingSessionInfo() { return null; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public int getVolumeHandling();
		public int getVolumeHandling() { return 0; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public int getVolumeMax();
		public int getVolumeMax() { return 0; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public int getVolume();
		public int getVolume() { return 0; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public boolean isReleased();
		public boolean isReleased() { return false; }
	
		// GEN member:android.media.MediaRouter2$RoutingController:public void selectRoute(android.media.MediaRoute2Info);
		public void selectRoute(android.media.MediaRoute2Info arg0) {}
	
		// GEN member:android.media.MediaRouter2$RoutingController:public void deselectRoute(android.media.MediaRoute2Info);
		public void deselectRoute(android.media.MediaRoute2Info arg0) {}
	
		// GEN member:android.media.MediaRouter2$RoutingController:public void setVolume(int);
		public void setVolume(int arg0) {}
	
		// GEN member:android.media.MediaRouter2$RoutingController:public void release();
		public void release() {}
	
		// GEN member:android.media.MediaRouter2$RoutingController:public java.lang.String toString();
		public java.lang.String toString() { return null; }
	
		protected RoutingController() {}
	}

}
