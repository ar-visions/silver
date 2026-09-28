/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <string.h>

#include "gtlsclientconnection-mbedtls.h"

enum
{
  PROP_0,
  PROP_VALIDATION_FLAGS,
  PROP_SERVER_IDENTITY,
  PROP_USE_SSL3,
  PROP_ACCEPTED_CAS,
  PROP_SESSION_RESUMPTION_ENABLED,
  PROP_SESSION_REUSED,
};

struct _GTlsClientConnectionMbedtls
{
  GTlsConnectionMbedtls parent_instance;

  GTlsCertificateFlags validation_flags;
  GSocketConnectable *server_identity;
  gboolean use_ssl3;
};

static void g_tls_client_connection_mbedtls_initable_interface_init (GInitableIface *iface);
static void g_tls_client_connection_mbedtls_client_connection_interface_init (GTlsClientConnectionInterface *iface);

static GInitableIface *g_tls_client_connection_mbedtls_parent_initable_iface;

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsClientConnectionMbedtls, g_tls_client_connection_mbedtls, G_TYPE_TLS_CONNECTION_MBEDTLS,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_client_connection_mbedtls_initable_interface_init)
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_CLIENT_CONNECTION,
                                                      g_tls_client_connection_mbedtls_client_connection_interface_init));

static void
g_tls_client_connection_mbedtls_init (GTlsClientConnectionMbedtls *self)
{
}

static void
g_tls_client_connection_mbedtls_finalize (GObject *object)
{
  GTlsClientConnectionMbedtls *self = G_TLS_CLIENT_CONNECTION_MBEDTLS (object);

  g_clear_object (&self->server_identity);

  G_OBJECT_CLASS (g_tls_client_connection_mbedtls_parent_class)->finalize (object);
}

static const gchar *
server_hostname (GTlsClientConnectionMbedtls *self)
{
  if (G_IS_NETWORK_ADDRESS (self->server_identity))
    return g_network_address_get_hostname (G_NETWORK_ADDRESS (self->server_identity));
  if (G_IS_NETWORK_SERVICE (self->server_identity))
    return g_network_service_get_domain (G_NETWORK_SERVICE (self->server_identity));
  return NULL;
}

/* SNI: the name without a trailing dot, never an address */
static void
set_server_name (GTlsClientConnectionMbedtls *self)
{
  const gchar *hostname = server_hostname (self);
  gchar *name;
  gsize n;

  if (!hostname || g_hostname_is_ip_address (hostname))
    return;

  name = g_hostname_to_ascii (hostname);
  if (!name)
    return;
  n = strlen (name);
  if (n > 0 && name[n - 1] == '.')
    name[n - 1] = '\0';

  mbedtls_ssl_set_hostname (g_tls_connection_mbedtls_get_ssl (G_TLS_CONNECTION_MBEDTLS (self)), name);
  g_free (name);
}

static gboolean
g_tls_client_connection_mbedtls_initable_init (GInitable     *initable,
                                               GCancellable  *cancellable,
                                               GError       **error)
{
  if (!g_tls_client_connection_mbedtls_parent_initable_iface->init (initable, cancellable, error))
    return FALSE;

  set_server_name (G_TLS_CLIENT_CONNECTION_MBEDTLS (initable));
  return TRUE;
}

static void
g_tls_client_connection_mbedtls_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  GTlsClientConnectionMbedtls *self = G_TLS_CLIENT_CONNECTION_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_VALIDATION_FLAGS:
      g_value_set_flags (value, self->validation_flags);
      break;

    case PROP_SERVER_IDENTITY:
      g_value_set_object (value, self->server_identity);
      break;

    case PROP_USE_SSL3:
      g_value_set_boolean (value, self->use_ssl3);
      break;

    case PROP_ACCEPTED_CAS:
      g_value_set_pointer (value, NULL);
      break;

    case PROP_SESSION_REUSED:
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_SESSION_RESUMPTION_ENABLED:
      g_value_set_boolean (value, g_tls_connection_base_get_session_resumption (G_TLS_CONNECTION_BASE (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_client_connection_mbedtls_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  GTlsClientConnectionMbedtls *self = G_TLS_CLIENT_CONNECTION_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_VALIDATION_FLAGS:
      self->validation_flags = g_value_get_flags (value);
      break;

    case PROP_SERVER_IDENTITY:
      g_clear_object (&self->server_identity);
      self->server_identity = g_value_dup_object (value);
      /* set after construction: the ssl context exists already */
      set_server_name (self);
      break;

    case PROP_USE_SSL3:
      self->use_ssl3 = g_value_get_boolean (value);
      break;

    case PROP_SESSION_RESUMPTION_ENABLED:
      g_tls_connection_base_set_session_resumption (G_TLS_CONNECTION_BASE (object), g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_client_connection_mbedtls_copy_session_state (GTlsClientConnection *conn,
                                                    GTlsClientConnection *source)
{
  /* sessions are not resumed yet: every connection is fresh */
}

static void
g_tls_client_connection_mbedtls_class_init (GTlsClientConnectionMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_tls_client_connection_mbedtls_get_property;
  gobject_class->set_property = g_tls_client_connection_mbedtls_set_property;
  gobject_class->finalize     = g_tls_client_connection_mbedtls_finalize;

  g_object_class_override_property (gobject_class, PROP_VALIDATION_FLAGS, "validation-flags");
  g_object_class_override_property (gobject_class, PROP_SERVER_IDENTITY, "server-identity");
  g_object_class_override_property (gobject_class, PROP_USE_SSL3, "use-ssl3");
  g_object_class_override_property (gobject_class, PROP_ACCEPTED_CAS, "accepted-cas");
  g_object_class_override_property (gobject_class, PROP_SESSION_REUSED, "session-reused");
  g_object_class_override_property (gobject_class, PROP_SESSION_RESUMPTION_ENABLED, "session-resumption-enabled");
}

static void
g_tls_client_connection_mbedtls_client_connection_interface_init (GTlsClientConnectionInterface *iface)
{
  iface->copy_session_state = g_tls_client_connection_mbedtls_copy_session_state;
}

static void
g_tls_client_connection_mbedtls_initable_interface_init (GInitableIface *iface)
{
  g_tls_client_connection_mbedtls_parent_initable_iface = g_type_interface_peek_parent (iface);
  iface->init = g_tls_client_connection_mbedtls_initable_init;
}
