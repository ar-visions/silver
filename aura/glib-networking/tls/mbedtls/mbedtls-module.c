/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "gtlsbackend-mbedtls.h"
#include "visibility.h"

GLIB_NETWORKING_EXPORT void
g_io_mbedtls_load (GIOModule *module)
{
  g_tls_backend_mbedtls_register (module);

  bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

GLIB_NETWORKING_EXPORT void
g_io_mbedtls_unload (GIOModule *module)
{
}

GLIB_NETWORKING_EXPORT gchar **
g_io_mbedtls_query (void)
{
  gchar *eps[] = {
    G_TLS_BACKEND_EXTENSION_POINT_NAME,
    NULL
  };
  return g_strdupv (eps);
}
