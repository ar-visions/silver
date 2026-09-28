/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include <gio/gio.h>
#include <mbedtls/ssl.h>

#include "gtlsconnection-base.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_CONNECTION_MBEDTLS (g_tls_connection_mbedtls_get_type ())

G_DECLARE_DERIVABLE_TYPE (GTlsConnectionMbedtls, g_tls_connection_mbedtls, G, TLS_CONNECTION_MBEDTLS, GTlsConnectionBase)

struct _GTlsConnectionMbedtlsClass
{
  GTlsConnectionBaseClass parent_class;
};

mbedtls_ssl_context *g_tls_connection_mbedtls_get_ssl    (GTlsConnectionMbedtls *self);

mbedtls_ssl_config  *g_tls_connection_mbedtls_get_config (GTlsConnectionMbedtls *self);

gboolean             g_tls_connection_mbedtls_set_own_certificate (GTlsConnectionMbedtls  *self,
                                                                   GError                **error);

G_END_DECLS
