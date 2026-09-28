/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include <gio/gio.h>

#include "gtlsdatabase-mbedtls.h"

G_BEGIN_DECLS

#define G_TYPE_TLS_FILE_DATABASE_MBEDTLS (g_tls_file_database_mbedtls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsFileDatabaseMbedtls, g_tls_file_database_mbedtls, G, TLS_FILE_DATABASE_MBEDTLS, GTlsDatabaseMbedtls)

G_END_DECLS
