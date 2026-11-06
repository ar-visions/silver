package android.provider;

import android.content.ContentResolver;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import java.io.IOException;

public class MediaStore {

	public static class Images {

		public static class Media implements android.provider.MediaStore.Images.ImageColumns {

			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/images/media");
			public static final Uri INTERNAL_CONTENT_URI = Uri.parse("content://media/internal/images/media");
		
	// GEN member:android.provider.MediaStore$Images$Media:public static android.graphics.Bitmap getBitmap(android.content.ContentResolver, android.net.Uri) throws java.io.FileNotFoundException, java.io.IOException;
	public static android.graphics.Bitmap getBitmap(android.content.ContentResolver arg0, android.net.Uri arg1) throws java.io.FileNotFoundException, java.io.IOException { return null; }

	// GEN member:android.provider.MediaStore$Images$Media:public static android.net.Uri getContentUri(java.lang.String);
	public static android.net.Uri getContentUri(java.lang.String arg0) { return null; }
}

		public static class Thumbnails implements android.provider.BaseColumns {

			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/images/media");

			public static Cursor queryMiniThumbnail(ContentResolver contentResolver, long id, int kind, String[] projection) {
				return null;
			}

			public static Bitmap getThumbnail(ContentResolver contentResolver, long imageId, long groupId, int kind, BitmapFactory.Options options) throws IOException {
				Uri uri = Media.EXTERNAL_CONTENT_URI.buildUpon().appendPath(String.valueOf(imageId)).build();
				try (ParcelFileDescriptor fd = contentResolver.openFileDescriptor(uri, "r")) {
					return BitmapFactory.decodeFileDescriptor(fd.getFileDescriptor(), null, options);
				}
			}
		}
	
		public interface ImageColumns extends android.provider.MediaStore.MediaColumns {
			public static final java.lang.String DESCRIPTION = "description";
			public static final java.lang.String EXPOSURE_TIME = "exposure_time";
			public static final java.lang.String F_NUMBER = "f_number";
			public static final java.lang.String ISO = "iso";
			public static final java.lang.String IS_PRIVATE = "isprivate";
			public static final java.lang.String LATITUDE = "latitude";
			public static final java.lang.String LONGITUDE = "longitude";
			public static final java.lang.String MINI_THUMB_MAGIC = "mini_thumb_magic";
			public static final java.lang.String PICASA_ID = "picasa_id";
			public static final java.lang.String SCENE_CAPTURE_TYPE = "scene_capture_type";
		}
	}

	public static class Video {

		public static class Media implements android.provider.MediaStore.Video.VideoColumns {

			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/video/media");
			public static final Uri INTERNAL_CONTENT_URI = Uri.parse("content://media/internal/video/media");
		}

		public static class Thumbnails implements android.provider.BaseColumns {

			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/video/media");

			public static Bitmap getThumbnail(ContentResolver contentResolver, long videoId, int kind, BitmapFactory.Options options) throws IOException {
				Uri uri = Media.EXTERNAL_CONTENT_URI.buildUpon().appendPath(String.valueOf(videoId)).build();
				try (ParcelFileDescriptor fd = contentResolver.openFileDescriptor(uri, "r")) {
					return BitmapFactory.decodeFileDescriptor(fd.getFileDescriptor(), null, options);
				}
			}
		}
	
		public interface VideoColumns extends android.provider.MediaStore.MediaColumns {
			public static final java.lang.String BOOKMARK = "bookmark";
			public static final java.lang.String CATEGORY = "category";
			public static final java.lang.String COLOR_RANGE = "color_range";
			public static final java.lang.String COLOR_STANDARD = "color_standard";
			public static final java.lang.String COLOR_TRANSFER = "color_transfer";
			public static final java.lang.String DESCRIPTION = "description";
			public static final java.lang.String IS_PRIVATE = "isprivate";
			public static final java.lang.String LANGUAGE = "language";
			public static final java.lang.String LATITUDE = "latitude";
			public static final java.lang.String LONGITUDE = "longitude";
			public static final java.lang.String MINI_THUMB_MAGIC = "mini_thumb_magic";
			public static final java.lang.String TAGS = "tags";
		}
	}

	public static class Audio {

		public static class Media implements android.provider.MediaStore.Audio.AudioColumns {

			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/audio/media");
		}

		public static class Artists implements android.provider.BaseColumns, android.provider.MediaStore.Audio.ArtistColumns {
			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/audio/artists");
		}

		public static class Albums implements android.provider.BaseColumns, android.provider.MediaStore.Audio.AlbumColumns {
			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/audio/albums");
		}

		public static class Genres implements android.provider.BaseColumns, android.provider.MediaStore.Audio.GenresColumns {
			public static final Uri EXTERNAL_CONTENT_URI = Uri.parse("content://media/external/audio/genres");
		}
	
		public interface AlbumColumns {
			public static final java.lang.String ALBUM = "album";
			public static final java.lang.String ALBUM_ART = "album_art";
			public static final java.lang.String ALBUM_ID = "album_id";
			public static final java.lang.String ALBUM_KEY = "album_key";
			public static final java.lang.String ARTIST = "artist";
			public static final java.lang.String ARTIST_ID = "artist_id";
			public static final java.lang.String ARTIST_KEY = "artist_key";
			public static final java.lang.String FIRST_YEAR = "minyear";
			public static final java.lang.String LAST_YEAR = "maxyear";
			public static final java.lang.String NUMBER_OF_SONGS = "numsongs";
			public static final java.lang.String NUMBER_OF_SONGS_FOR_ARTIST = "numsongs_by_artist";
		}
	
		public interface ArtistColumns {
			public static final java.lang.String ARTIST = "artist";
			public static final java.lang.String ARTIST_KEY = "artist_key";
			public static final java.lang.String NUMBER_OF_ALBUMS = "number_of_albums";
			public static final java.lang.String NUMBER_OF_TRACKS = "number_of_tracks";
		}
	
		public interface AudioColumns extends android.provider.MediaStore.MediaColumns {
			public static final java.lang.String ALBUM_ID = "album_id";
			public static final java.lang.String ALBUM_KEY = "album_key";
			public static final java.lang.String ARTIST_ID = "artist_id";
			public static final java.lang.String ARTIST_KEY = "artist_key";
			public static final java.lang.String BOOKMARK = "bookmark";
			public static final java.lang.String GENRE = "genre";
			public static final java.lang.String GENRE_ID = "genre_id";
			public static final java.lang.String GENRE_KEY = "genre_key";
			public static final java.lang.String IS_ALARM = "is_alarm";
			public static final java.lang.String IS_AUDIOBOOK = "is_audiobook";
			public static final java.lang.String IS_MUSIC = "is_music";
			public static final java.lang.String IS_NOTIFICATION = "is_notification";
			public static final java.lang.String IS_PODCAST = "is_podcast";
			public static final java.lang.String IS_RECORDING = "is_recording";
			public static final java.lang.String IS_RINGTONE = "is_ringtone";
			public static final java.lang.String TITLE_KEY = "title_key";
			public static final java.lang.String TITLE_RESOURCE_URI = "title_resource_uri";
			public static final java.lang.String TRACK = "track";
			public static final java.lang.String YEAR = "year";
		}
	
		public interface GenresColumns {
			public static final java.lang.String NAME = "name";
		}
	}

	public static class Files {

		public static Uri getContentUri(String type) {
			return Uri.parse("content://media/files/" + type);
		}
	}

	// GEN member:android.provider.MediaStore:public static int getPickImagesMaxLimit();
	public static int getPickImagesMaxLimit() { return 0; }

	// GEN member:android.provider.MediaStore:public static android.net.Uri setRequireOriginal(android.net.Uri);
	public static android.net.Uri setRequireOriginal(android.net.Uri arg0) { return null; }

	public interface MediaColumns extends android.provider.BaseColumns {
		public static final java.lang.String ALBUM = "album";
		public static final java.lang.String ALBUM_ARTIST = "album_artist";
		public static final java.lang.String ARTIST = "artist";
		public static final java.lang.String AUTHOR = "author";
		public static final java.lang.String BITRATE = "bitrate";
		public static final java.lang.String BUCKET_DISPLAY_NAME = "bucket_display_name";
		public static final java.lang.String BUCKET_ID = "bucket_id";
		public static final java.lang.String CAPTURE_FRAMERATE = "capture_framerate";
		public static final java.lang.String CD_TRACK_NUMBER = "cd_track_number";
		public static final java.lang.String COMPILATION = "compilation";
		public static final java.lang.String COMPOSER = "composer";
		public static final java.lang.String DATA = "_data";
		public static final java.lang.String DATE_ADDED = "date_added";
		public static final java.lang.String DATE_EXPIRES = "date_expires";
		public static final java.lang.String DATE_MODIFIED = "date_modified";
		public static final java.lang.String DATE_TAKEN = "datetaken";
		public static final java.lang.String DISC_NUMBER = "disc_number";
		public static final java.lang.String DISPLAY_NAME = "_display_name";
		public static final java.lang.String DOCUMENT_ID = "document_id";
		public static final java.lang.String DURATION = "duration";
		public static final java.lang.String GENERATION_ADDED = "generation_added";
		public static final java.lang.String GENERATION_MODIFIED = "generation_modified";
		public static final java.lang.String GENRE = "genre";
		public static final java.lang.String HEIGHT = "height";
		public static final java.lang.String INSTANCE_ID = "instance_id";
		public static final java.lang.String IS_DOWNLOAD = "is_download";
		public static final java.lang.String IS_DRM = "is_drm";
		public static final java.lang.String IS_FAVORITE = "is_favorite";
		public static final java.lang.String IS_PENDING = "is_pending";
		public static final java.lang.String IS_TRASHED = "is_trashed";
		public static final java.lang.String MIME_TYPE = "mime_type";
		public static final java.lang.String NUM_TRACKS = "num_tracks";
		public static final java.lang.String ORIENTATION = "orientation";
		public static final java.lang.String ORIGINAL_DOCUMENT_ID = "original_document_id";
		public static final java.lang.String OWNER_PACKAGE_NAME = "owner_package_name";
		public static final java.lang.String RELATIVE_PATH = "relative_path";
		public static final java.lang.String RESOLUTION = "resolution";
		public static final java.lang.String SIZE = "_size";
		public static final java.lang.String TITLE = "title";
		public static final java.lang.String VOLUME_NAME = "volume_name";
		public static final java.lang.String WIDTH = "width";
		public static final java.lang.String WRITER = "writer";
		public static final java.lang.String XMP = "xmp";
		public static final java.lang.String YEAR = "year";
	}
}
