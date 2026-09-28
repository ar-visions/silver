/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include "gtlsconnection-mbedtls.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_CLIENT_CONNECTION_MBEDTLS (g_tls_client_connection_mbedtls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsClientConnectionMbedtls, g_tls_client_connection_mbedtls, G, TLS_CLIENT_CONNECTION_MBEDTLS, GTlsConnectionMbedtls)

G_END_DECLS
