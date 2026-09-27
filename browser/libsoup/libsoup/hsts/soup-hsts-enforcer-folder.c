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

#include "soup-hsts-enforcer-folder.h"
#include "soup.h"

/* one folder per site, holding a readable hsts.txt */

enum {
	PROP_0,

	PROP_DIRECTORY,

	LAST_PROPERTY
};

static GParamSpec *properties[LAST_PROPERTY] = { NULL, };

struct _SoupHSTSEnforcerFolder {
	SoupHSTSEnforcer parent;
};

typedef struct {
	char *directory;
	gboolean loading;
} SoupHSTSEnforcerFolderPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SoupHSTSEnforcerFolder, soup_hsts_enforcer_folder, SOUP_TYPE_HSTS_ENFORCER)

static void load (SoupHSTSEnforcer *hsts_enforcer);

static void
soup_hsts_enforcer_folder_init (SoupHSTSEnforcerFolder *folder)
{
}

static void
soup_hsts_enforcer_folder_finalize (GObject *object)
{
	SoupHSTSEnforcerFolderPrivate *priv =
		soup_hsts_enforcer_folder_get_instance_private (SOUP_HSTS_ENFORCER_FOLDER (object));

	g_free (priv->directory);

	G_OBJECT_CLASS (soup_hsts_enforcer_folder_parent_class)->finalize (object);
}

static void
soup_hsts_enforcer_folder_set_property (GObject *object, guint prop_id,
					const GValue *value, GParamSpec *pspec)
{
	SoupHSTSEnforcerFolderPrivate *priv =
		soup_hsts_enforcer_folder_get_instance_private (SOUP_HSTS_ENFORCER_FOLDER (object));

	switch (prop_id) {
	case PROP_DIRECTORY:
		priv->directory = g_value_dup_string (value);
		load (SOUP_HSTS_ENFORCER (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
soup_hsts_enforcer_folder_get_property (GObject *object, guint prop_id,
					GValue *value, GParamSpec *pspec)
{
	SoupHSTSEnforcerFolderPrivate *priv =
		soup_hsts_enforcer_folder_get_instance_private (SOUP_HSTS_ENFORCER_FOLDER (object));

	switch (prop_id) {
	case PROP_DIRECTORY:
		g_value_set_string (value, priv->directory);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

SoupHSTSEnforcer *
soup_hsts_enforcer_folder_new (const char *directory)
{
	g_return_val_if_fail (directory != NULL, NULL);

	return g_object_new (SOUP_TYPE_HSTS_ENFORCER_FOLDER,
			     "directory", directory,
			     NULL);
}

static gboolean
safe_site (const char *domain)
{
	return *domain && strcmp (domain, ".") != 0 && strcmp (domain, "..") != 0 &&
		!strchr (domain, '/') && !strchr (domain, '\\');
}

static void
load_site (SoupHSTSEnforcer *hsts_enforcer, const char *directory, const char *site)
{
	char *path = g_build_filename (directory, site, "hsts.txt", NULL);
	char *contents = NULL;
	char **lines;
	gulong max_age = 0;
	gint64 expires = 0;
	gboolean subdomains = FALSE;
	int i;

	if (!g_file_get_contents (path, &contents, NULL, NULL)) {
		g_free (path);
		return;
	}

	lines = g_strsplit (contents, "\n", -1);
	for (i = 0; lines[i]; i++) {
		char **kv = g_strsplit (lines[i], "\t", 2);
		if (g_strv_length (kv) == 2) {
			if (strcmp (kv[0], "max-age") == 0)
				max_age = strtoul (kv[1], NULL, 10);
			else if (strcmp (kv[0], "expires") == 0)
				expires = g_ascii_strtoll (kv[1], NULL, 10);
			else if (strcmp (kv[0], "subdomains") == 0)
				subdomains = strcmp (kv[1], "1") == 0;
		}
		g_strfreev (kv);
	}
	g_strfreev (lines);
	g_free (contents);
	g_free (path);

	if (expires > g_get_real_time () / G_USEC_PER_SEC) {
		GDateTime *when = g_date_time_new_from_unix_utc (expires);
		SoupHSTSPolicy *policy = soup_hsts_policy_new_full (site, max_age, when, subdomains);
		soup_hsts_enforcer_set_policy (hsts_enforcer, policy);
		soup_hsts_policy_free (policy);
		g_date_time_unref (when);
	}
}

static void
load (SoupHSTSEnforcer *hsts_enforcer)
{
	SoupHSTSEnforcerFolderPrivate *priv =
		soup_hsts_enforcer_folder_get_instance_private (SOUP_HSTS_ENFORCER_FOLDER (hsts_enforcer));
	GDir *dir = g_dir_open (priv->directory, 0, NULL);
	const char *site;

	if (!dir)
		return;

	priv->loading = TRUE;
	while ((site = g_dir_read_name (dir))) {
		if (safe_site (site))
			load_site (hsts_enforcer, priv->directory, site);
	}
	priv->loading = FALSE;
	g_dir_close (dir);
}

static void
soup_hsts_enforcer_folder_changed (SoupHSTSEnforcer *hsts_enforcer,
				   SoupHSTSPolicy   *old_policy,
				   SoupHSTSPolicy   *new_policy)
{
	SoupHSTSEnforcerFolderPrivate *priv =
		soup_hsts_enforcer_folder_get_instance_private (SOUP_HSTS_ENFORCER_FOLDER (hsts_enforcer));
	SoupHSTSPolicy *policy = new_policy ? new_policy : old_policy;
	const char *site = soup_hsts_policy_get_domain (policy);
	char *folder, *path;

	if (priv->loading || soup_hsts_policy_is_session_policy (policy) || !safe_site (site))
		return;

	folder = g_build_filename (priv->directory, site, NULL);
	path = g_build_filename (folder, "hsts.txt", NULL);
	if (new_policy && soup_hsts_policy_get_expires (new_policy)) {
		char *text = g_strdup_printf ("max-age\t%lu\nexpires\t%" G_GINT64_FORMAT "\nsubdomains\t%d\n",
					      soup_hsts_policy_get_max_age (new_policy),
					      g_date_time_to_unix (soup_hsts_policy_get_expires (new_policy)),
					      soup_hsts_policy_includes_subdomains (new_policy) ? 1 : 0);
		g_mkdir_with_parents (folder, 0700);
		g_file_set_contents (path, text, -1, NULL);
		g_free (text);
	} else {
		g_unlink (path);
		g_rmdir (folder);
	}
	g_free (path);
	g_free (folder);
}

static gboolean
soup_hsts_enforcer_folder_is_persistent (SoupHSTSEnforcer *hsts_enforcer)
{
	return TRUE;
}

static void
soup_hsts_enforcer_folder_class_init (SoupHSTSEnforcerFolderClass *folder_class)
{
	SoupHSTSEnforcerClass *hsts_enforcer_class =
		SOUP_HSTS_ENFORCER_CLASS (folder_class);
	GObjectClass *object_class = G_OBJECT_CLASS (folder_class);

	hsts_enforcer_class->is_persistent = soup_hsts_enforcer_folder_is_persistent;
	hsts_enforcer_class->changed       = soup_hsts_enforcer_folder_changed;

	object_class->finalize     = soup_hsts_enforcer_folder_finalize;
	object_class->set_property = soup_hsts_enforcer_folder_set_property;
	object_class->get_property = soup_hsts_enforcer_folder_get_property;

        properties[PROP_DIRECTORY] =
		g_param_spec_string ("directory",
				     "Directory",
				     "Folder of per-site policy files",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, LAST_PROPERTY, properties);
}
