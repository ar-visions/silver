/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include <gio/gio.h>
#include <mbedtls/x509_crt.h>

G_BEGIN_DECLS

#define G_TYPE_TLS_DATABASE_MBEDTLS (g_tls_database_mbedtls_get_type ())

G_DECLARE_DERIVABLE_TYPE (GTlsDatabaseMbedtls, g_tls_database_mbedtls, G, TLS_DATABASE_MBEDTLS, GTlsDatabase)

struct _GTlsDatabaseMbedtlsClass
{
  GTlsDatabaseClass parent_class;

  gboolean (*populate_trust_list) (GTlsDatabaseMbedtls  *self,
                                   mbedtls_x509_crt     *trust,
                                   GError              **error);
};

GTlsDatabaseMbedtls *g_tls_database_mbedtls_new (GError **error);

G_END_DECLS
