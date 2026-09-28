/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define G_TYPE_TLS_BACKEND_MBEDTLS (g_tls_backend_mbedtls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsBackendMbedtls, g_tls_backend_mbedtls, G, TLS_BACKEND_MBEDTLS, GObject)

void g_tls_backend_mbedtls_register (GIOModule *module);

G_END_DECLS
