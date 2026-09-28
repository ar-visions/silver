/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include "gtlscertificate-mbedtls.h"
#include "gtlsdatabase-mbedtls.h"

typedef struct
{
  GMutex mutex;
  mbedtls_x509_crt trust;
} GTlsDatabaseMbedtlsPrivate;

static void g_tls_database_mbedtls_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GTlsDatabaseMbedtls, g_tls_database_mbedtls, G_TYPE_TLS_DATABASE,
                         G_ADD_PRIVATE (GTlsDatabaseMbedtls);
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_tls_database_mbedtls_initable_iface_init);)

static void
g_tls_database_mbedtls_init (GTlsDatabaseMbedtls *self)
{
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);

  g_mutex_init (&priv->mutex);
  mbedtls_x509_crt_init (&priv->trust);
}

static void
g_tls_database_mbedtls_finalize (GObject *object)
{
  GTlsDatabaseMbedtls *self = G_TLS_DATABASE_MBEDTLS (object);
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);

  mbedtls_x509_crt_free (&priv->trust);
  g_mutex_clear (&priv->mutex);

  G_OBJECT_CLASS (g_tls_database_mbedtls_parent_class)->finalize (object);
}

static gboolean
same_name (const mbedtls_x509_buf *a,
           const guint8           *b,
           gsize                   b_len)
{
  return a->len == b_len && memcmp (a->p, b, b_len) == 0;
}

static GTlsCertificate *
g_tls_database_mbedtls_lookup_certificate_issuer (GTlsDatabase             *database,
                                                  GTlsCertificate          *certificate,
                                                  GTlsInteraction          *interaction,
                                                  GTlsDatabaseLookupFlags   flags,
                                                  GCancellable             *cancellable,
                                                  GError                  **error)
{
  GTlsDatabaseMbedtls *self = G_TLS_DATABASE_MBEDTLS (database);
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);
  mbedtls_x509_crt *cert;
  const mbedtls_x509_crt *ca;
  GTlsCertificate *issuer = NULL;

  if (!G_IS_TLS_CERTIFICATE_MBEDTLS (certificate))
    return NULL;
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  /* no private keys are kept here */
  if (flags & G_TLS_DATABASE_LOOKUP_KEYPAIR)
    return NULL;

  cert = g_tls_certificate_mbedtls_get_cert (G_TLS_CERTIFICATE_MBEDTLS (certificate));

  g_mutex_lock (&priv->mutex);
  for (ca = &priv->trust; ca && ca->raw.p && !issuer; ca = ca->next)
    if (same_name (&ca->subject_raw, cert->issuer_raw.p, cert->issuer_raw.len))
      issuer = g_tls_certificate_mbedtls_new (ca->raw.p, ca->raw.len, NULL);
  g_mutex_unlock (&priv->mutex);

  return issuer;
}

static GList *
g_tls_database_mbedtls_lookup_certificates_issued_by (GTlsDatabase             *database,
                                                      GByteArray               *issuer_raw_dn,
                                                      GTlsInteraction          *interaction,
                                                      GTlsDatabaseLookupFlags   flags,
                                                      GCancellable             *cancellable,
                                                      GError                  **error)
{
  GTlsDatabaseMbedtls *self = G_TLS_DATABASE_MBEDTLS (database);
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);
  const mbedtls_x509_crt *ca;
  GList *issued = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  if (flags & G_TLS_DATABASE_LOOKUP_KEYPAIR)
    return NULL;

  g_mutex_lock (&priv->mutex);
  for (ca = &priv->trust; ca && ca->raw.p; ca = ca->next)
    if (same_name (&ca->issuer_raw, issuer_raw_dn->data, issuer_raw_dn->len))
      {
        GTlsCertificate *cert = g_tls_certificate_mbedtls_new (ca->raw.p, ca->raw.len, NULL);

        if (cert)
          issued = g_list_prepend (issued, cert);
      }
  g_mutex_unlock (&priv->mutex);

  return g_list_reverse (issued);
}

static GTlsCertificateFlags
g_tls_database_mbedtls_verify_chain (GTlsDatabase             *database,
                                     GTlsCertificate          *chain,
                                     const gchar              *purpose,
                                     GSocketConnectable       *identity,
                                     GTlsInteraction          *interaction,
                                     GTlsDatabaseVerifyFlags   flags,
                                     GCancellable             *cancellable,
                                     GError                  **error)
{
  GTlsDatabaseMbedtls *self = G_TLS_DATABASE_MBEDTLS (database);
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);
  GTlsCertificateFlags result;
  mbedtls_x509_crt certs;
  uint32_t mbedtls_result = 0;
  int status;

  if (!G_IS_TLS_CERTIFICATE_MBEDTLS (chain))
    return G_TLS_CERTIFICATE_GENERIC_ERROR;
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_CERTIFICATE_GENERIC_ERROR;

  mbedtls_x509_crt_init (&certs);
  g_tls_certificate_mbedtls_chain_der (G_TLS_CERTIFICATE_MBEDTLS (chain), &certs);

  g_mutex_lock (&priv->mutex);
  status = mbedtls_x509_crt_verify (&certs, &priv->trust, NULL, NULL, &mbedtls_result, NULL, NULL);
  g_mutex_unlock (&priv->mutex);
  mbedtls_x509_crt_free (&certs);

  result = g_tls_certificate_mbedtls_convert_flags (mbedtls_result);
  if (status != 0 && result == 0)
    result = G_TLS_CERTIFICATE_GENERIC_ERROR;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_CERTIFICATE_GENERIC_ERROR;

  if (identity)
    result |= g_tls_certificate_mbedtls_verify_identity (G_TLS_CERTIFICATE_MBEDTLS (chain), identity);

  return result;
}

/* the system's roots: debian, then red hat, then bsd style */
static gboolean
g_tls_database_mbedtls_populate_trust_list (GTlsDatabaseMbedtls  *self,
                                            mbedtls_x509_crt     *trust,
                                            GError              **error)
{
  static const gchar *bundles[] = {
    "/etc/ssl/certs/ca-certificates.crt",
    "/etc/pki/tls/certs/ca-bundle.crt",
    "/etc/ssl/cert.pem",
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (bundles); i++)
    {
      /* a positive result counts certificates it skipped */
      if (g_file_test (bundles[i], G_FILE_TEST_EXISTS) &&
          mbedtls_x509_crt_parse_file (trust, bundles[i]) >= 0 &&
          trust->raw.p)
        return TRUE;
    }

  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                       _("No system certificate authority bundle was found"));
  return FALSE;
}

static gboolean
g_tls_database_mbedtls_initable_init (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GTlsDatabaseMbedtls *self = G_TLS_DATABASE_MBEDTLS (initable);
  GTlsDatabaseMbedtlsPrivate *priv = g_tls_database_mbedtls_get_instance_private (self);
  GTlsDatabaseMbedtlsClass *klass = G_TLS_DATABASE_MBEDTLS_GET_CLASS (self);
  gboolean ok;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  g_mutex_lock (&priv->mutex);
  ok = klass->populate_trust_list (self, &priv->trust, error);
  g_mutex_unlock (&priv->mutex);

  return ok;
}

static void
g_tls_database_mbedtls_class_init (GTlsDatabaseMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsDatabaseClass *database_class = G_TLS_DATABASE_CLASS (klass);

  gobject_class->finalize = g_tls_database_mbedtls_finalize;

  database_class->lookup_certificate_issuer = g_tls_database_mbedtls_lookup_certificate_issuer;
  database_class->lookup_certificates_issued_by = g_tls_database_mbedtls_lookup_certificates_issued_by;
  database_class->verify_chain = g_tls_database_mbedtls_verify_chain;

  klass->populate_trust_list = g_tls_database_mbedtls_populate_trust_list;
}

static void
g_tls_database_mbedtls_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_tls_database_mbedtls_initable_init;
}

GTlsDatabaseMbedtls *
g_tls_database_mbedtls_new (GError **error)
{
  return g_initable_new (G_TYPE_TLS_DATABASE_MBEDTLS, NULL, error, NULL);
}
