/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <mbedtls/error.h>
#include <mbedtls/x509.h>

#include "gtlscertificate-mbedtls.h"

enum
{
  PROP_0,

  PROP_CERTIFICATE,
  PROP_CERTIFICATE_PEM,
  PROP_PRIVATE_KEY,
  PROP_PRIVATE_KEY_PEM,
  PROP_ISSUER,
  PROP_PKCS11_URI,
  PROP_PRIVATE_KEY_PKCS11_URI,
  PROP_NOT_VALID_BEFORE,
  PROP_NOT_VALID_AFTER,
  PROP_SUBJECT_NAME,
  PROP_ISSUER_NAME,
  PROP_DNS_NAMES,
  PROP_IP_ADDRESSES,
  PROP_PKCS12_DATA,
  PROP_PASSWORD,
};

struct _GTlsCertificateMbedtls
{
  GTlsCertificate parent_instance;

  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
  gboolean have_cert;
  gboolean have_key;

  GTlsCertificateMbedtls *issuer;
  gchar *pkcs11_uri;
  gchar *private_key_pkcs11_uri;
  GByteArray *pkcs12_data;

  GError *construct_error;
};

static void g_tls_certificate_mbedtls_initable_iface_init (GInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsCertificateMbedtls, g_tls_certificate_mbedtls, G_TYPE_TLS_CERTIFICATE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_certificate_mbedtls_initable_iface_init);)

static void
g_tls_certificate_mbedtls_finalize (GObject *object)
{
  GTlsCertificateMbedtls *self = G_TLS_CERTIFICATE_MBEDTLS (object);

  mbedtls_x509_crt_free (&self->cert);
  mbedtls_pk_free (&self->key);
  g_clear_object (&self->issuer);
  g_free (self->pkcs11_uri);
  g_free (self->private_key_pkcs11_uri);
  g_clear_pointer (&self->pkcs12_data, g_byte_array_unref);
  g_clear_error (&self->construct_error);

  G_OBJECT_CLASS (g_tls_certificate_mbedtls_parent_class)->finalize (object);
}

/* DER wrapped as PEM: base64 in 64 column lines */
static gchar *
der_to_pem (const guint8 *der,
            gsize         length,
            const gchar  *label)
{
  gchar *b64 = g_base64_encode (der, length);
  gsize n = strlen (b64);
  GString *pem = g_string_new (NULL);
  gsize i;

  g_string_append_printf (pem, "-----BEGIN %s-----\n", label);
  for (i = 0; i < n; i += 64)
    {
      g_string_append_len (pem, b64 + i, MIN (64, n - i));
      g_string_append_c (pem, '\n');
    }
  g_string_append_printf (pem, "-----END %s-----\n", label);
  g_free (b64);
  return g_string_free (pem, FALSE);
}

static GDateTime *
date_from_x509_time (const mbedtls_x509_time *t)
{
  return g_date_time_new_utc (t->year, t->mon, t->day, t->hour, t->min, t->sec);
}

static gchar *
dn_string (const mbedtls_x509_name *dn)
{
  gchar buf[2048];

  if (mbedtls_x509_dn_gets (buf, sizeof (buf), dn) < 0)
    return NULL;
  return g_strdup (buf);
}

/* the SANs of one type: GBytes for DNS, GInetAddress for IP */
static GPtrArray *
subject_alt_names (GTlsCertificateMbedtls *self,
                   int                     type)
{
  GPtrArray *list;
  const mbedtls_x509_sequence *cur;

  list = g_ptr_array_new_with_free_func (type == MBEDTLS_X509_SAN_IP_ADDRESS ?
                                         g_object_unref : (GDestroyNotify)g_bytes_unref);

  for (cur = &self->cert.subject_alt_names; cur && cur->buf.p; cur = cur->next)
    {
      mbedtls_x509_subject_alternative_name san;

      if (mbedtls_x509_parse_subject_alt_name (&cur->buf, &san) != 0)
        continue;

      if (san.type == type && type == MBEDTLS_X509_SAN_DNS_NAME)
        g_ptr_array_add (list, g_bytes_new (san.san.unstructured_name.p,
                                            san.san.unstructured_name.len));
      else if (san.type == type && type == MBEDTLS_X509_SAN_IP_ADDRESS)
        {
          gsize len = san.san.unstructured_name.len;

          if (len == 4 || len == 16)
            g_ptr_array_add (list, g_inet_address_new_from_bytes (san.san.unstructured_name.p,
                                                                  len == 4 ? G_SOCKET_FAMILY_IPV4 : G_SOCKET_FAMILY_IPV6));
        }

      mbedtls_x509_free_subject_alt_name (&san);
    }

  return list;
}

static GByteArray *
export_key_der (GTlsCertificateMbedtls *self)
{
  guint8 buf[16384];
  int n;

  if (!self->have_key)
    return NULL;
  n = mbedtls_pk_write_key_der (&self->key, buf, sizeof (buf));
  if (n <= 0)
    return NULL;
  /* written at the end of the buffer */
  return g_byte_array_append (g_byte_array_new (), buf + sizeof (buf) - n, n);
}

static gchar *
export_key_pem (GTlsCertificateMbedtls *self)
{
  guint8 buf[16384];

  if (!self->have_key)
    return NULL;
  if (mbedtls_pk_write_key_pem (&self->key, buf, sizeof (buf)) != 0)
    return NULL;
  return g_strdup ((gchar *)buf);
}

static void
g_tls_certificate_mbedtls_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GTlsCertificateMbedtls *self = G_TLS_CERTIFICATE_MBEDTLS (object);

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      if (self->have_cert)
        g_value_take_boxed (value, g_byte_array_append (g_byte_array_new (),
                                                        self->cert.raw.p, self->cert.raw.len));
      break;

    case PROP_CERTIFICATE_PEM:
      if (self->have_cert)
        g_value_take_string (value, der_to_pem (self->cert.raw.p, self->cert.raw.len, "CERTIFICATE"));
      break;

    case PROP_PRIVATE_KEY:
      g_value_take_boxed (value, export_key_der (self));
      break;

    case PROP_PRIVATE_KEY_PEM:
      g_value_take_string (value, export_key_pem (self));
      break;

    case PROP_ISSUER:
      g_value_set_object (value, self->issuer);
      break;

    case PROP_PKCS11_URI:
      g_value_set_string (value, self->pkcs11_uri);
      break;

    case PROP_PRIVATE_KEY_PKCS11_URI:
      g_value_set_string (value, self->private_key_pkcs11_uri);
      break;

    case PROP_NOT_VALID_BEFORE:
      if (self->have_cert)
        g_value_take_boxed (value, date_from_x509_time (&self->cert.valid_from));
      break;

    case PROP_NOT_VALID_AFTER:
      if (self->have_cert)
        g_value_take_boxed (value, date_from_x509_time (&self->cert.valid_to));
      break;

    case PROP_SUBJECT_NAME:
      if (self->have_cert)
        g_value_take_string (value, dn_string (&self->cert.subject));
      break;

    case PROP_ISSUER_NAME:
      if (self->have_cert)
        g_value_take_string (value, dn_string (&self->cert.issuer));
      break;

    case PROP_DNS_NAMES:
      if (self->have_cert)
        g_value_take_boxed (value, subject_alt_names (self, MBEDTLS_X509_SAN_DNS_NAME));
      break;

    case PROP_IP_ADDRESSES:
      if (self->have_cert)
        g_value_take_boxed (value, subject_alt_names (self, MBEDTLS_X509_SAN_IP_ADDRESS));
      break;

    case PROP_PKCS12_DATA:
      g_value_set_boxed (value, self->pkcs12_data);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_construct_error (GTlsCertificateMbedtls *self,
                     const gchar            *what,
                     int                     status)
{
  gchar reason[256];

  if (self->construct_error)
    return;
  mbedtls_strerror (status, reason, sizeof (reason));
  self->construct_error = g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                                       _("Could not parse %s: %s"), what, reason);
}

static void
set_cert (GTlsCertificateMbedtls *self,
          const guint8           *data,
          gsize                   length,
          const gchar            *what)
{
  int status;

  if (self->have_cert)
    {
      g_critical ("GTlsCertificate: a certificate was already set during construction");
      return;
    }
  status = mbedtls_x509_crt_parse (&self->cert, data, length);
  if (status == 0)
    self->have_cert = TRUE;
  else
    set_construct_error (self, what, status);
}

static void
set_key (GTlsCertificateMbedtls *self,
         const guint8           *data,
         gsize                   length,
         const gchar            *what)
{
  int status;

  if (self->have_key)
    {
      g_critical ("GTlsCertificate: a private key was already set during construction");
      return;
    }
  status = mbedtls_pk_parse_key (&self->key, data, length, NULL, 0);
  if (status == 0)
    self->have_key = TRUE;
  else
    set_construct_error (self, what, status);
}

static void
g_tls_certificate_mbedtls_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GTlsCertificateMbedtls *self = G_TLS_CERTIFICATE_MBEDTLS (object);
  GByteArray *bytes;
  const gchar *string;

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      bytes = g_value_get_boxed (value);
      if (bytes)
        set_cert (self, bytes->data, bytes->len, "DER certificate");
      break;

    case PROP_CERTIFICATE_PEM:
      string = g_value_get_string (value);
      /* PEM parsing counts the terminating NUL */
      if (string)
        set_cert (self, (const guint8 *)string, strlen (string) + 1, "PEM certificate");
      break;

    case PROP_PRIVATE_KEY:
      bytes = g_value_get_boxed (value);
      if (bytes)
        set_key (self, bytes->data, bytes->len, "DER private key");
      break;

    case PROP_PRIVATE_KEY_PEM:
      string = g_value_get_string (value);
      if (string)
        set_key (self, (const guint8 *)string, strlen (string) + 1, "PEM private key");
      break;

    case PROP_ISSUER:
      self->issuer = g_value_dup_object (value);
      break;

    case PROP_PKCS11_URI:
      self->pkcs11_uri = g_value_dup_string (value);
      if (self->pkcs11_uri && !self->construct_error)
        self->construct_error = g_error_new_literal (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                                                     _("PKCS #11 certificates are not supported"));
      break;

    case PROP_PRIVATE_KEY_PKCS11_URI:
      self->private_key_pkcs11_uri = g_value_dup_string (value);
      break;

    case PROP_PKCS12_DATA:
      self->pkcs12_data = g_value_dup_boxed (value);
      if (self->pkcs12_data && !self->construct_error)
        self->construct_error = g_error_new_literal (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                                                     _("PKCS #12 certificates are not supported"));
      break;

    case PROP_PASSWORD:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_certificate_mbedtls_init (GTlsCertificateMbedtls *self)
{
  mbedtls_x509_crt_init (&self->cert);
  mbedtls_pk_init (&self->key);
}

static gboolean
g_tls_certificate_mbedtls_initable_init (GInitable     *initable,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
  GTlsCertificateMbedtls *self = G_TLS_CERTIFICATE_MBEDTLS (initable);

  if (self->construct_error)
    {
      g_propagate_error (error, self->construct_error);
      self->construct_error = NULL;
      return FALSE;
    }
  if (!self->have_cert)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("No certificate data provided"));
      return FALSE;
    }
  return TRUE;
}

GTlsCertificateFlags
g_tls_certificate_mbedtls_convert_flags (uint32_t mbedtls_flags)
{
  GTlsCertificateFlags flags = 0;

  if (mbedtls_flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED)
    flags |= G_TLS_CERTIFICATE_UNKNOWN_CA;
  if (mbedtls_flags & MBEDTLS_X509_BADCERT_CN_MISMATCH)
    flags |= G_TLS_CERTIFICATE_BAD_IDENTITY;
  if (mbedtls_flags & MBEDTLS_X509_BADCERT_FUTURE)
    flags |= G_TLS_CERTIFICATE_NOT_ACTIVATED;
  if (mbedtls_flags & MBEDTLS_X509_BADCERT_EXPIRED)
    flags |= G_TLS_CERTIFICATE_EXPIRED;
  if (mbedtls_flags & MBEDTLS_X509_BADCERT_REVOKED)
    flags |= G_TLS_CERTIFICATE_REVOKED;
  if (mbedtls_flags & (MBEDTLS_X509_BADCERT_BAD_MD | MBEDTLS_X509_BADCERT_BAD_PK | MBEDTLS_X509_BADCERT_BAD_KEY))
    flags |= G_TLS_CERTIFICATE_INSECURE;
  if (mbedtls_flags & (MBEDTLS_X509_BADCERT_MISSING | MBEDTLS_X509_BADCERT_SKIP_VERIFY |
                       MBEDTLS_X509_BADCERT_OTHER | MBEDTLS_X509_BADCERT_KEY_USAGE |
                       MBEDTLS_X509_BADCERT_EXT_KEY_USAGE | MBEDTLS_X509_BADCERT_NS_CERT_TYPE))
    flags |= G_TLS_CERTIFICATE_GENERIC_ERROR;

  return flags;
}

/* RFC 6125: "*." covers exactly one left-most label */
static gboolean
dns_name_matches (const gchar *pattern,
                  gsize        pattern_len,
                  const gchar *host)
{
  gchar *name = g_ascii_strdown (pattern, pattern_len);
  gboolean match;

  if (g_str_has_prefix (name, "*."))
    {
      const gchar *dot = strchr (host, '.');

      match = dot && dot != host && g_ascii_strcasecmp (dot + 1, name + 2) == 0;
    }
  else
    match = g_ascii_strcasecmp (name, host) == 0;

  g_free (name);
  return match;
}

GTlsCertificateFlags
g_tls_certificate_mbedtls_verify_identity (GTlsCertificateMbedtls *self,
                                           GSocketConnectable     *identity)
{
  const gchar *hostname = NULL;
  gchar *owned = NULL;
  GTlsCertificateFlags flags = G_TLS_CERTIFICATE_BAD_IDENTITY;
  GPtrArray *names;
  guint i;

  if (G_IS_NETWORK_ADDRESS (identity))
    hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (identity));
  else if (G_IS_NETWORK_SERVICE (identity))
    hostname = g_network_service_get_domain (G_NETWORK_SERVICE (identity));
  else if (G_IS_INET_SOCKET_ADDRESS (identity))
    hostname = owned = g_inet_address_to_string (g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (identity)));

  if (!hostname)
    return flags;

  if (g_hostname_is_ip_address (hostname))
    {
      GInetAddress *want = g_inet_address_new_from_string (hostname);

      names = subject_alt_names (self, MBEDTLS_X509_SAN_IP_ADDRESS);
      for (i = 0; want && i < names->len; i++)
        if (g_inet_address_equal (want, names->pdata[i]))
          flags = 0;
      g_ptr_array_unref (names);
      g_clear_object (&want);
    }
  else
    {
      gchar *ascii = g_hostname_to_ascii (hostname);
      gchar *host = g_ascii_strdown (ascii ? ascii : hostname, -1);
      gsize n = strlen (host);

      if (n > 0 && host[n - 1] == '.')
        host[n - 1] = '\0';

      /* browsers match SANs only, never the subject CN */
      names = subject_alt_names (self, MBEDTLS_X509_SAN_DNS_NAME);
      for (i = 0; i < names->len; i++)
        {
          gsize len;
          const gchar *pattern = g_bytes_get_data (names->pdata[i], &len);

          if (dns_name_matches (pattern, len, host))
            flags = 0;
        }
      g_ptr_array_unref (names);
      g_free (host);
      g_free (ascii);
    }

  g_free (owned);
  return flags;
}

void
g_tls_certificate_mbedtls_chain_der (GTlsCertificateMbedtls *self,
                                     mbedtls_x509_crt       *out)
{
  for (; self; self = self->issuer)
    mbedtls_x509_crt_parse_der (out, self->cert.raw.p, self->cert.raw.len);
}

static GTlsCertificateFlags
g_tls_certificate_mbedtls_verify (GTlsCertificate    *cert,
                                  GSocketConnectable *identity,
                                  GTlsCertificate    *trusted_ca)
{
  GTlsCertificateMbedtls *self = G_TLS_CERTIFICATE_MBEDTLS (cert);
  GTlsCertificateFlags flags = 0;

  if (trusted_ca)
    {
      mbedtls_x509_crt chain;
      uint32_t result = 0;
      int status;

      mbedtls_x509_crt_init (&chain);
      g_tls_certificate_mbedtls_chain_der (self, &chain);
      status = mbedtls_x509_crt_verify (&chain, &G_TLS_CERTIFICATE_MBEDTLS (trusted_ca)->cert,
                                        NULL, NULL, &result, NULL, NULL);
      mbedtls_x509_crt_free (&chain);

      flags = g_tls_certificate_mbedtls_convert_flags (result);
      if (status != 0 && flags == 0)
        flags = G_TLS_CERTIFICATE_GENERIC_ERROR;
    }

  if (identity)
    flags |= g_tls_certificate_mbedtls_verify_identity (self, identity);

  return flags;
}

static void
g_tls_certificate_mbedtls_class_init (GTlsCertificateMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsCertificateClass *certificate_class = G_TLS_CERTIFICATE_CLASS (klass);

  gobject_class->get_property = g_tls_certificate_mbedtls_get_property;
  gobject_class->set_property = g_tls_certificate_mbedtls_set_property;
  gobject_class->finalize     = g_tls_certificate_mbedtls_finalize;

  certificate_class->verify = g_tls_certificate_mbedtls_verify;

  g_object_class_override_property (gobject_class, PROP_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CERTIFICATE_PEM, "certificate-pem");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY, "private-key");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY_PEM, "private-key-pem");
  g_object_class_override_property (gobject_class, PROP_ISSUER, "issuer");
  g_object_class_override_property (gobject_class, PROP_PKCS11_URI, "pkcs11-uri");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY_PKCS11_URI, "private-key-pkcs11-uri");
  g_object_class_override_property (gobject_class, PROP_NOT_VALID_BEFORE, "not-valid-before");
  g_object_class_override_property (gobject_class, PROP_NOT_VALID_AFTER, "not-valid-after");
  g_object_class_override_property (gobject_class, PROP_SUBJECT_NAME, "subject-name");
  g_object_class_override_property (gobject_class, PROP_ISSUER_NAME, "issuer-name");
  g_object_class_override_property (gobject_class, PROP_DNS_NAMES, "dns-names");
  g_object_class_override_property (gobject_class, PROP_IP_ADDRESSES, "ip-addresses");
  g_object_class_override_property (gobject_class, PROP_PKCS12_DATA, "pkcs12-data");
  g_object_class_override_property (gobject_class, PROP_PASSWORD, "password");
}

static void
g_tls_certificate_mbedtls_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_tls_certificate_mbedtls_initable_init;
}

GTlsCertificate *
g_tls_certificate_mbedtls_new (const guint8    *der,
                               gsize            length,
                               GTlsCertificate *issuer)
{
  GTlsCertificateMbedtls *self = g_object_new (G_TYPE_TLS_CERTIFICATE_MBEDTLS,
                                               "issuer", issuer,
                                               NULL);

  set_cert (self, der, length, "DER certificate");
  if (!self->have_cert)
    {
      g_object_unref (self);
      return NULL;
    }
  return G_TLS_CERTIFICATE (self);
}

static gboolean
issued_by (const mbedtls_x509_crt *child,
           const mbedtls_x509_crt *parent)
{
  return child->issuer_raw.len == parent->subject_raw.len &&
         memcmp (child->issuer_raw.p, parent->subject_raw.p, child->issuer_raw.len) == 0;
}

GTlsCertificate *
g_tls_certificate_mbedtls_build_chain (const mbedtls_x509_crt *chain)
{
  GPtrArray *certs = g_ptr_array_new_with_free_func (g_object_unref);
  const mbedtls_x509_crt *cur;
  GTlsCertificate *leaf = NULL;
  guint i, j;

  for (cur = chain; cur && cur->raw.p; cur = cur->next)
    {
      GTlsCertificate *cert = g_tls_certificate_mbedtls_new (cur->raw.p, cur->raw.len, NULL);

      if (cert)
        g_ptr_array_add (certs, cert);
    }

  /* each certificate's issuer is the one whose subject it names */
  for (i = 0; i < certs->len; i++)
    {
      GTlsCertificateMbedtls *child = certs->pdata[i];

      if (issued_by (&child->cert, &child->cert))
        continue;
      for (j = 0; j < certs->len; j++)
        {
          GTlsCertificateMbedtls *parent = certs->pdata[j];

          if (i != j && issued_by (&child->cert, &parent->cert))
            {
              g_tls_certificate_mbedtls_set_issuer (child, parent);
              break;
            }
        }
    }

  if (certs->len > 0)
    leaf = g_object_ref (certs->pdata[0]);
  g_ptr_array_unref (certs);
  return leaf;
}

mbedtls_x509_crt *
g_tls_certificate_mbedtls_get_cert (GTlsCertificateMbedtls *self)
{
  return &self->cert;
}

mbedtls_pk_context *
g_tls_certificate_mbedtls_get_key (GTlsCertificateMbedtls *self)
{
  return self->have_key ? &self->key : NULL;
}

gboolean
g_tls_certificate_mbedtls_has_key (GTlsCertificateMbedtls *self)
{
  return self->have_key;
}

GTlsCertificateMbedtls *
g_tls_certificate_mbedtls_get_issuer (GTlsCertificateMbedtls *self)
{
  return self->issuer;
}

void
g_tls_certificate_mbedtls_set_issuer (GTlsCertificateMbedtls *self,
                                      GTlsCertificateMbedtls *issuer)
{
  /* a loop of issuers would never end a chain walk */
  GTlsCertificateMbedtls *up;

  for (up = issuer; up; up = up->issuer)
    if (up == self)
      return;

  g_set_object (&self->issuer, issuer);
  g_object_notify (G_OBJECT (self), "issuer");
}
