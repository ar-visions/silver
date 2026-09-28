/* SPDX-License-Identifier: LGPL-2.1-or-later */

#pragma once

#include <gio/gio.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>

G_BEGIN_DECLS

#define G_TYPE_TLS_CERTIFICATE_MBEDTLS (g_tls_certificate_mbedtls_get_type ())

G_DECLARE_FINAL_TYPE (GTlsCertificateMbedtls, g_tls_certificate_mbedtls, G, TLS_CERTIFICATE_MBEDTLS, GTlsCertificate)

GTlsCertificate      *g_tls_certificate_mbedtls_new             (const guint8           *der,
                                                                 gsize                   length,
                                                                 GTlsCertificate        *issuer);

GTlsCertificate      *g_tls_certificate_mbedtls_build_chain     (const mbedtls_x509_crt *chain);

mbedtls_x509_crt     *g_tls_certificate_mbedtls_get_cert        (GTlsCertificateMbedtls *self);

mbedtls_pk_context   *g_tls_certificate_mbedtls_get_key         (GTlsCertificateMbedtls *self);

gboolean              g_tls_certificate_mbedtls_has_key         (GTlsCertificateMbedtls *self);

GTlsCertificateMbedtls *g_tls_certificate_mbedtls_get_issuer    (GTlsCertificateMbedtls *self);

void                  g_tls_certificate_mbedtls_set_issuer      (GTlsCertificateMbedtls *self,
                                                                 GTlsCertificateMbedtls *issuer);

void                  g_tls_certificate_mbedtls_chain_der       (GTlsCertificateMbedtls *self,
                                                                 mbedtls_x509_crt       *out);

GTlsCertificateFlags  g_tls_certificate_mbedtls_convert_flags   (uint32_t                mbedtls_flags);

GTlsCertificateFlags  g_tls_certificate_mbedtls_verify_identity (GTlsCertificateMbedtls *self,
                                                                 GSocketConnectable     *identity);

G_END_DECLS
