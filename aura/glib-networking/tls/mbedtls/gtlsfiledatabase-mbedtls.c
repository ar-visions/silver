/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <mbedtls/error.h>

#include "gtlsfiledatabase-mbedtls.h"

enum
{
  PROP_0,
  PROP_ANCHORS,
};

struct _GTlsFileDatabaseMbedtls
{
  GTlsDatabaseMbedtls parent_instance;

  gchar *anchor_filename;
};

static void g_tls_file_database_mbedtls_file_database_interface_init (GTlsFileDatabaseInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsFileDatabaseMbedtls, g_tls_file_database_mbedtls, G_TYPE_TLS_DATABASE_MBEDTLS,
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_FILE_DATABASE,
                                                      g_tls_file_database_mbedtls_file_database_interface_init);)

static void
g_tls_file_database_mbedtls_finalize (GObject *object)
{
  GTlsFileDatabaseMbedtls *self = G_TLS_FILE_DATABASE_MBEDTLS (object);

  g_free (self->anchor_filename);

  G_OBJECT_CLASS (g_tls_file_database_mbedtls_parent_class)->finalize (object);
}

static void
g_tls_file_database_mbedtls_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GTlsFileDatabaseMbedtls *self = G_TLS_FILE_DATABASE_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_ANCHORS:
      g_value_set_string (value, self->anchor_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_file_database_mbedtls_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GTlsFileDatabaseMbedtls *self = G_TLS_FILE_DATABASE_MBEDTLS (object);
  const gchar *anchor_path;

  switch (prop_id)
    {
    case PROP_ANCHORS:
      anchor_path = g_value_get_string (value);
      if (anchor_path && !g_path_is_absolute (anchor_path))
        {
          g_warning ("The anchor file name used with a GTlsFileDatabase must be an absolute path, and not relative: %s", anchor_path);
          return;
        }
      g_free (self->anchor_filename);
      self->anchor_filename = g_strdup (anchor_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_file_database_mbedtls_init (GTlsFileDatabaseMbedtls *self)
{
}

static gboolean
g_tls_file_database_mbedtls_populate_trust_list (GTlsDatabaseMbedtls  *database,
                                                 mbedtls_x509_crt     *trust,
                                                 GError              **error)
{
  GTlsFileDatabaseMbedtls *self = G_TLS_FILE_DATABASE_MBEDTLS (database);
  int status;

  /* no file: an empty database trusts nothing */
  if (!self->anchor_filename)
    return TRUE;

  status = mbedtls_x509_crt_parse_file (trust, self->anchor_filename);
  if (status < 0 || !trust->raw.p)
    {
      gchar reason[256];

      mbedtls_strerror (status, reason, sizeof (reason));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                   _("Failed to populate trust list from %s: %s"),
                   self->anchor_filename, reason);
      return FALSE;
    }
  return TRUE;
}

static void
g_tls_file_database_mbedtls_class_init (GTlsFileDatabaseMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsDatabaseMbedtlsClass *mbedtls_class = G_TLS_DATABASE_MBEDTLS_CLASS (klass);

  gobject_class->get_property = g_tls_file_database_mbedtls_get_property;
  gobject_class->set_property = g_tls_file_database_mbedtls_set_property;
  gobject_class->finalize     = g_tls_file_database_mbedtls_finalize;

  mbedtls_class->populate_trust_list = g_tls_file_database_mbedtls_populate_trust_list;

  g_object_class_override_property (gobject_class, PROP_ANCHORS, "anchors");
}

static void
g_tls_file_database_mbedtls_file_database_interface_init (GTlsFileDatabaseInterface *iface)
{
}
