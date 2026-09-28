/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <psa/crypto.h>

#include "gtlsbackend-mbedtls.h"
#include "gtlscertificate-mbedtls.h"
#include "gtlsclientconnection-mbedtls.h"
#include "gtlsdatabase-mbedtls.h"
#include "gtlsfiledatabase-mbedtls.h"
#include "gtlsserverconnection-mbedtls.h"

struct _GTlsBackendMbedtls
{
  GObject parent_instance;

  GMutex mutex;
  GTlsDatabase *default_database;
};

static void g_tls_backend_mbedtls_interface_init (GTlsBackendInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GTlsBackendMbedtls, g_tls_backend_mbedtls, G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (G_TYPE_TLS_BACKEND,
                                                               g_tls_backend_mbedtls_interface_init);)

static gpointer
gtls_mbedtls_init (gpointer data)
{
  GTypePlugin *plugin;

  /* PSA gives mbedtls its random numbers */
  psa_crypto_init ();

  /* mbedtls keeps global state: never unload */
  plugin = g_type_get_plugin (G_TYPE_TLS_BACKEND_MBEDTLS);
  if (plugin)
    g_type_plugin_use (plugin);
  return NULL;
}

static GOnce mbedtls_inited = G_ONCE_INIT;

static void
g_tls_backend_mbedtls_init (GTlsBackendMbedtls *backend)
{
  g_once (&mbedtls_inited, gtls_mbedtls_init, NULL);
  g_mutex_init (&backend->mutex);
}

static void
g_tls_backend_mbedtls_finalize (GObject *object)
{
  GTlsBackendMbedtls *backend = G_TLS_BACKEND_MBEDTLS (object);

  g_clear_object (&backend->default_database);
  g_mutex_clear (&backend->mutex);

  G_OBJECT_CLASS (g_tls_backend_mbedtls_parent_class)->finalize (object);
}

static void
g_tls_backend_mbedtls_class_init (GTlsBackendMbedtlsClass *backend_class)
{
  G_OBJECT_CLASS (backend_class)->finalize = g_tls_backend_mbedtls_finalize;
}

static void
g_tls_backend_mbedtls_class_finalize (GTlsBackendMbedtlsClass *backend_class)
{
}

static GTlsDatabase *
g_tls_backend_mbedtls_get_default_database (GTlsBackend *backend)
{
  GTlsBackendMbedtls *self = G_TLS_BACKEND_MBEDTLS (backend);
  GTlsDatabase *result;
  GError *error = NULL;

  g_mutex_lock (&self->mutex);

  if (self->default_database)
    result = g_object_ref (self->default_database);
  else
    {
      result = G_TLS_DATABASE (g_tls_database_mbedtls_new (&error));
      if (error)
        {
          g_warning ("Failed to load TLS database: %s", error->message);
          g_clear_error (&error);
        }
      else
        self->default_database = g_object_ref (result);
    }

  g_mutex_unlock (&self->mutex);

  return result;
}

static void
g_tls_backend_mbedtls_interface_init (GTlsBackendInterface *iface)
{
  iface->get_certificate_type       = g_tls_certificate_mbedtls_get_type;
  iface->get_client_connection_type = g_tls_client_connection_mbedtls_get_type;
  iface->get_server_connection_type = g_tls_server_connection_mbedtls_get_type;
  iface->get_file_database_type     = g_tls_file_database_mbedtls_get_type;
  iface->get_default_database       = g_tls_backend_mbedtls_get_default_database;
}

void
g_tls_backend_mbedtls_register (GIOModule *module)
{
  g_tls_backend_mbedtls_register_type (G_TYPE_MODULE (module));
  if (!module)
    g_io_extension_point_register (G_TLS_BACKEND_EXTENSION_POINT_NAME);
  g_io_extension_point_implement (G_TLS_BACKEND_EXTENSION_POINT_NAME,
                                  g_tls_backend_mbedtls_get_type (),
                                  "mbedtls",
                                  0);
}
