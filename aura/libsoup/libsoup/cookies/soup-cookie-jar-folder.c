/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2026 Kalen White
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>

#include "soup-cookie-jar-folder.h"
#include "soup.h"

/* one folder per site, holding a readable cookies.txt */

enum {
	PROP_0,

	PROP_DIRECTORY,

	LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

struct _SoupCookieJarFolder {
	SoupCookieJar parent;
};

typedef struct {
	char *directory;
	/* site -> GPtrArray of the site's saved cookies */
	GHashTable *sites;
} SoupCookieJarFolderPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SoupCookieJarFolder, soup_cookie_jar_folder, SOUP_TYPE_COOKIE_JAR)

static void load (SoupCookieJar *jar);

static void
soup_cookie_jar_folder_init (SoupCookieJarFolder *folder)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (folder);

	priv->sites = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
					     (GDestroyNotify)g_ptr_array_unref);
}

static void
soup_cookie_jar_folder_finalize (GObject *object)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (SOUP_COOKIE_JAR_FOLDER (object));

	g_free (priv->directory);
	g_hash_table_destroy (priv->sites);

	G_OBJECT_CLASS (soup_cookie_jar_folder_parent_class)->finalize (object);
}

static void
soup_cookie_jar_folder_set_property (GObject *object, guint prop_id,
				     const GValue *value, GParamSpec *pspec)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (SOUP_COOKIE_JAR_FOLDER (object));

	switch (prop_id) {
	case PROP_DIRECTORY:
		priv->directory = g_value_dup_string (value);
		load (SOUP_COOKIE_JAR (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
soup_cookie_jar_folder_get_property (GObject *object, guint prop_id,
				     GValue *value, GParamSpec *pspec)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (SOUP_COOKIE_JAR_FOLDER (object));

	switch (prop_id) {
	case PROP_DIRECTORY:
		g_value_set_string (value, priv->directory);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

SoupCookieJar *
soup_cookie_jar_folder_new (const char *directory, gboolean read_only)
{
	g_return_val_if_fail (directory != NULL, NULL);

	return g_object_new (SOUP_TYPE_COOKIE_JAR_FOLDER,
			     "directory", directory,
			     "read-only", read_only,
			     NULL);
}

/* a site folder name, or NULL for a domain unsafe as one */
static char *
site_for_domain (const char *domain)
{
	if (*domain == '.')
		domain++;
	if (!*domain || strcmp (domain, ".") == 0 || strcmp (domain, "..") == 0)
		return NULL;
	if (strchr (domain, '/') || strchr (domain, '\\'))
		return NULL;
	return g_strdup (domain);
}

static void
append_escaped (GString *out, const char *text)
{
	for (; *text; text++) {
		if (*text == '\\')
			g_string_append (out, "\\\\");
		else if (*text == '\t')
			g_string_append (out, "\\t");
		else if (*text == '\n')
			g_string_append (out, "\\n");
		else if (*text == '\r')
			g_string_append (out, "\\r");
		else
			g_string_append_c (out, *text);
	}
}

static char *
unescape (const char *text)
{
	GString *out = g_string_new (NULL);

	for (; *text; text++) {
		if (*text != '\\' || !text[1]) {
			g_string_append_c (out, *text);
			continue;
		}
		text++;
		if (*text == 't')
			g_string_append_c (out, '\t');
		else if (*text == 'n')
			g_string_append_c (out, '\n');
		else if (*text == 'r')
			g_string_append_c (out, '\r');
		else
			g_string_append_c (out, *text);
	}
	return g_string_free (out, FALSE);
}

static const char *
same_site_name (SoupSameSitePolicy policy)
{
	switch (policy) {
	case SOUP_SAME_SITE_POLICY_STRICT:
		return "Strict";
	case SOUP_SAME_SITE_POLICY_NONE:
		return "None";
	case SOUP_SAME_SITE_POLICY_LAX:
	default:
		return "Lax";
	}
}

static void
append_cookie (GString *out, SoupCookie *cookie)
{
	append_escaped (out, soup_cookie_get_name (cookie));
	g_string_append_c (out, '\t');
	append_escaped (out, soup_cookie_get_value (cookie));
	g_string_append (out, "\tdomain=");
	append_escaped (out, soup_cookie_get_domain (cookie));
	g_string_append (out, "; path=");
	append_escaped (out, soup_cookie_get_path (cookie));
	g_string_append_printf (out, "; expires=%" G_GINT64_FORMAT,
				g_date_time_to_unix (soup_cookie_get_expires (cookie)));
	if (soup_cookie_get_secure (cookie))
		g_string_append (out, "; secure");
	if (soup_cookie_get_http_only (cookie))
		g_string_append (out, "; httponly");
	g_string_append_printf (out, "; samesite=%s\n",
				same_site_name (soup_cookie_get_same_site_policy (cookie)));
}

static SoupCookie *
parse_cookie (const char *line, gint64 now)
{
	char **fields = g_strsplit (line, "\t", 3);
	char **attrs = NULL;
	char *name = NULL, *value = NULL, *domain = NULL, *path = NULL;
	gint64 expires = 0;
	gboolean secure = FALSE, http_only = FALSE;
	SoupSameSitePolicy same_site = SOUP_SAME_SITE_POLICY_LAX;
	SoupCookie *cookie = NULL;
	int i;

	if (g_strv_length (fields) != 3)
		goto out;

	attrs = g_strsplit (fields[2], "; ", -1);
	for (i = 0; attrs[i]; i++) {
		const char *a = attrs[i];
		if (g_str_has_prefix (a, "domain="))
			domain = unescape (a + 7);
		else if (g_str_has_prefix (a, "path="))
			path = unescape (a + 5);
		else if (g_str_has_prefix (a, "expires="))
			expires = g_ascii_strtoll (a + 8, NULL, 10);
		else if (strcmp (a, "secure") == 0)
			secure = TRUE;
		else if (strcmp (a, "httponly") == 0)
			http_only = TRUE;
		else if (strcmp (a, "samesite=Strict") == 0)
			same_site = SOUP_SAME_SITE_POLICY_STRICT;
		else if (strcmp (a, "samesite=None") == 0)
			same_site = SOUP_SAME_SITE_POLICY_NONE;
	}
	if (!domain || !path || expires <= now)
		goto out;

	name = unescape (fields[0]);
	value = unescape (fields[1]);
	cookie = soup_cookie_new (name, value, domain, path,
				  expires - now <= G_MAXINT ? (int)(expires - now) : G_MAXINT);
	soup_cookie_set_secure (cookie, secure);
	soup_cookie_set_http_only (cookie, http_only);
	soup_cookie_set_same_site_policy (cookie, same_site);

 out:
	g_free (name);
	g_free (value);
	g_free (domain);
	g_free (path);
	g_strfreev (attrs);
	g_strfreev (fields);
	return cookie;
}

static GPtrArray *
site_cookies (SoupCookieJarFolderPrivate *priv, const char *site)
{
	GPtrArray *cookies = g_hash_table_lookup (priv->sites, site);

	if (!cookies) {
		cookies = g_ptr_array_new_with_free_func ((GDestroyNotify)soup_cookie_free);
		g_hash_table_insert (priv->sites, g_strdup (site), cookies);
	}
	return cookies;
}

static void
load (SoupCookieJar *jar)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (SOUP_COOKIE_JAR_FOLDER (jar));
	GDir *dir = g_dir_open (priv->directory, 0, NULL);
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	const char *site;

	if (!dir)
		return;

	while ((site = g_dir_read_name (dir))) {
		char *path = g_build_filename (priv->directory, site, "cookies.txt", NULL);
		char *contents = NULL;
		char **lines;
		int i;

		if (g_file_get_contents (path, &contents, NULL, NULL)) {
			lines = g_strsplit (contents, "\n", -1);
			for (i = 0; lines[i]; i++) {
				SoupCookie *cookie = parse_cookie (lines[i], now);
				if (!cookie)
					continue;
				g_ptr_array_add (site_cookies (priv, site), soup_cookie_copy (cookie));
				soup_cookie_jar_add_cookie (jar, cookie);
			}
			g_strfreev (lines);
			g_free (contents);
		}
		g_free (path);
	}
	g_dir_close (dir);
}

static void
save_site (SoupCookieJarFolderPrivate *priv, const char *site)
{
	GPtrArray *cookies = g_hash_table_lookup (priv->sites, site);
	char *folder = g_build_filename (priv->directory, site, NULL);
	char *path = g_build_filename (folder, "cookies.txt", NULL);
	GString *out;
	guint i;

	if (!cookies || !cookies->len) {
		g_unlink (path);
		g_rmdir (folder);
		g_hash_table_remove (priv->sites, site);
	} else {
		out = g_string_new (NULL);
		for (i = 0; i < cookies->len; i++)
			append_cookie (out, g_ptr_array_index (cookies, i));
		g_mkdir_with_parents (folder, 0700);
		g_file_set_contents (path, out->str, out->len, NULL);
		g_string_free (out, TRUE);
	}
	g_free (path);
	g_free (folder);
}

static void
remove_cookie (GPtrArray *cookies, SoupCookie *cookie)
{
	guint i;

	for (i = 0; i < cookies->len; i++) {
		if (soup_cookie_equal (g_ptr_array_index (cookies, i), cookie)) {
			g_ptr_array_remove_index (cookies, i);
			return;
		}
	}
}

static void
soup_cookie_jar_folder_changed (SoupCookieJar *jar,
				SoupCookie    *old_cookie,
				SoupCookie    *new_cookie)
{
	SoupCookieJarFolderPrivate *priv =
		soup_cookie_jar_folder_get_instance_private (SOUP_COOKIE_JAR_FOLDER (jar));
	char *old_site = old_cookie ? site_for_domain (soup_cookie_get_domain (old_cookie)) : NULL;
	char *new_site = new_cookie ? site_for_domain (soup_cookie_get_domain (new_cookie)) : NULL;

	if (old_site)
		remove_cookie (site_cookies (priv, old_site), old_cookie);
	if (new_site && soup_cookie_get_expires (new_cookie))
		g_ptr_array_add (site_cookies (priv, new_site), soup_cookie_copy (new_cookie));

	if (old_site)
		save_site (priv, old_site);
	if (new_site && g_strcmp0 (old_site, new_site) != 0)
		save_site (priv, new_site);

	g_free (old_site);
	g_free (new_site);
}

static gboolean
soup_cookie_jar_folder_is_persistent (SoupCookieJar *jar)
{
	return TRUE;
}

static void
soup_cookie_jar_folder_class_init (SoupCookieJarFolderClass *folder_class)
{
	SoupCookieJarClass *cookie_jar_class =
		SOUP_COOKIE_JAR_CLASS (folder_class);
	GObjectClass *object_class = G_OBJECT_CLASS (folder_class);

	cookie_jar_class->is_persistent = soup_cookie_jar_folder_is_persistent;
	cookie_jar_class->changed       = soup_cookie_jar_folder_changed;

	object_class->finalize     = soup_cookie_jar_folder_finalize;
	object_class->set_property = soup_cookie_jar_folder_set_property;
	object_class->get_property = soup_cookie_jar_folder_get_property;

        properties[PROP_DIRECTORY] =
		g_param_spec_string ("directory",
				     "Directory",
				     "Folder of per-site cookie files",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}
