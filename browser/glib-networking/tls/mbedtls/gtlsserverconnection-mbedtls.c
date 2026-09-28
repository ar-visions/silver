/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include "gtlsserverconnection-mbedtls.h"

enum
{
  PROP_0,
  PROP_AUTHENTICATION_MODE,
};

struct _GTlsServerConnectionMbedtls
{
  GTlsConnectionMbedtls parent_instance;

  GTlsAuthenticationMode authentication_mode;
};

static void g_tls_server_connection_mbedtls_server_connection_interface_init (GTlsServerConnectionInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsServerConnectionMbedtls, g_tls_server_connection_mbedtls, G_TYPE_TLS_CONNECTION_MBEDTLS,
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_SERVER_CONNECTION,
                                                      g_tls_server_connection_mbedtls_server_connection_interface_init));

static void
g_tls_server_connection_mbedtls_init (GTlsServerConnectionMbedtls *self)
{
}

static void
g_tls_server_connection_mbedtls_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  GTlsServerConnectionMbedtls *self = G_TLS_SERVER_CONNECTION_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      g_value_set_enum (value, self->authentication_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_server_connection_mbedtls_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  GTlsServerConnectionMbedtls *self = G_TLS_SERVER_CONNECTION_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_AUTHENTICATION_MODE:
      self->authentication_mode = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_server_connection_mbedtls_prepare_handshake (GTlsConnectionBase  *tls,
                                                   gchar              **advertised_protocols)
{
  GTlsServerConnectionMbedtls *self = G_TLS_SERVER_CONNECTION_MBEDTLS (tls);
  mbedtls_ssl_config *conf = g_tls_connection_mbedtls_get_config (G_TLS_CONNECTION_MBEDTLS (self));

  /* ask for a client certificate; the base decides on it */
  mbedtls_ssl_conf_authmode (conf, self->authentication_mode == G_TLS_AUTHENTICATION_NONE ?
                                   MBEDTLS_SSL_VERIFY_NONE : MBEDTLS_SSL_VERIFY_OPTIONAL);

  G_TLS_CONNECTION_BASE_CLASS (g_tls_server_connection_mbedtls_parent_class)->prepare_handshake (tls, advertised_protocols);
}

static void
g_tls_server_connection_mbedtls_class_init (GTlsServerConnectionMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);

  gobject_class->get_property = g_tls_server_connection_mbedtls_get_property;
  gobject_class->set_property = g_tls_server_connection_mbedtls_set_property;

  base_class->prepare_handshake = g_tls_server_connection_mbedtls_prepare_handshake;

  g_object_class_override_property (gobject_class, PROP_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_tls_server_connection_mbedtls_server_connection_interface_init (GTlsServerConnectionInterface *iface)
{
}
