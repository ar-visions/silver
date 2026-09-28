/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509.h>

#include "gtlscertificate-mbedtls.h"
#include "gtlsconnection-mbedtls.h"

typedef struct
{
  mbedtls_ssl_config conf;
  mbedtls_ssl_context ssl;
  gchar **alpn;

  /* the conf points into these while it lives */
  GTlsCertificate *own_certificate;
  mbedtls_x509_crt own_chain;
} GTlsConnectionMbedtlsPrivate;

static void g_tls_connection_mbedtls_initable_iface_init (GInitableIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GTlsConnectionMbedtls, g_tls_connection_mbedtls, G_TYPE_TLS_CONNECTION_BASE,
                                  G_ADD_PRIVATE (GTlsConnectionMbedtls);
                                  G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                         g_tls_connection_mbedtls_initable_iface_init);)

static void
g_tls_connection_mbedtls_init (GTlsConnectionMbedtls *self)
{
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);

  mbedtls_ssl_config_init (&priv->conf);
  mbedtls_ssl_init (&priv->ssl);
  mbedtls_x509_crt_init (&priv->own_chain);
}

static void
g_tls_connection_mbedtls_finalize (GObject *object)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (object);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);

  mbedtls_ssl_free (&priv->ssl);
  mbedtls_ssl_config_free (&priv->conf);
  mbedtls_x509_crt_free (&priv->own_chain);
  g_clear_object (&priv->own_certificate);
  g_strfreev (priv->alpn);

  G_OBJECT_CLASS (g_tls_connection_mbedtls_parent_class)->finalize (object);
}

mbedtls_ssl_context *
g_tls_connection_mbedtls_get_ssl (GTlsConnectionMbedtls *self)
{
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);

  return &priv->ssl;
}

mbedtls_ssl_config *
g_tls_connection_mbedtls_get_config (GTlsConnectionMbedtls *self)
{
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);

  return &priv->conf;
}

/* the base stream, without blocking unless a timeout is set */
static int
send_callback (void                *ctx,
               const unsigned char *buf,
               size_t               len)
{
  GTlsConnectionBase *tls = ctx;
  GError **error = g_tls_connection_base_get_write_error (tls);
  gssize ret;

  /* an error left here was one mbedtls chose not to report */
  g_clear_error (error);

  ret = g_pollable_stream_write (G_OUTPUT_STREAM (g_tls_connection_base_get_base_ostream (tls)),
                                 buf, len,
                                 g_tls_connection_base_get_write_timeout (tls) != 0,
                                 g_tls_connection_base_get_write_cancellable (tls),
                                 error);
  if (ret >= 0)
    return ret;
  if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  return MBEDTLS_ERR_NET_SEND_FAILED;
}

static int
recv_callback (void          *ctx,
               unsigned char *buf,
               size_t         len)
{
  GTlsConnectionBase *tls = ctx;
  GError **error = g_tls_connection_base_get_read_error (tls);
  gssize ret;

  g_clear_error (error);

  ret = g_pollable_stream_read (G_INPUT_STREAM (g_tls_connection_base_get_base_istream (tls)),
                                buf, len,
                                g_tls_connection_base_get_read_timeout (tls) != 0,
                                g_tls_connection_base_get_read_cancellable (tls),
                                error);
  if (ret >= 0)
    return ret;
  if (g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    return MBEDTLS_ERR_SSL_WANT_READ;
  return MBEDTLS_ERR_NET_RECV_FAILED;
}

static void
set_mbedtls_error (GError      **error,
                   gint          code,
                   const gchar  *prefix,
                   int           ret)
{
  gchar reason[256];

  mbedtls_strerror (ret, reason, sizeof (reason));
  g_set_error (error, G_TLS_ERROR, code, "%s: %s", prefix, reason);
}

static GTlsConnectionBaseStatus
end_mbedtls_io (GTlsConnectionMbedtls  *self,
                GIOCondition            direction,
                int                     ret,
                GError                **error,
                const gchar            *err_prefix)
{
  GTlsConnectionBase *tls = G_TLS_CONNECTION_BASE (self);
  GTlsConnectionBaseStatus status;
  gboolean handshaking;
  gboolean ever_handshaked;
  GError *my_error = NULL;

  /* a TLS 1.3 ticket, not data: read again */
  if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
    return G_TLS_CONNECTION_BASE_TRY_AGAIN;

  /* a want with no socket error: mbedtls asks to call again */
  if ((ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) &&
      !*g_tls_connection_base_get_read_error (tls) &&
      !*g_tls_connection_base_get_write_error (tls))
    return G_TLS_CONNECTION_BASE_TRY_AGAIN;

  status = g_tls_connection_base_pop_io (tls, direction, ret >= 0, &my_error);
  if (status == G_TLS_CONNECTION_BASE_OK ||
      status == G_TLS_CONNECTION_BASE_WOULD_BLOCK ||
      status == G_TLS_CONNECTION_BASE_TIMED_OUT)
    {
      if (my_error)
        g_propagate_error (error, my_error);
      return status;
    }

  handshaking = g_tls_connection_base_is_handshaking (tls);
  ever_handshaked = g_tls_connection_base_ever_handshaked (tls);

  if (handshaking && !ever_handshaked &&
      (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_FAILED) ||
       g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_BROKEN_PIPE) ||
       ret == MBEDTLS_ERR_SSL_INVALID_RECORD ||
       ret == MBEDTLS_ERR_SSL_CONN_EOF))
    {
      g_clear_error (&my_error);
      set_mbedtls_error (error, G_TLS_ERROR_NOT_TLS, _("Peer failed to perform TLS handshake"), ret);
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == MBEDTLS_ERR_SSL_CONN_EOF)
    {
      g_clear_error (&my_error);
      if (g_tls_connection_get_require_close_notify (G_TLS_CONNECTION (self)))
        {
          g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_EOF,
                               _("TLS connection closed unexpectedly"));
          return G_TLS_CONNECTION_BASE_ERROR;
        }
      return G_TLS_CONNECTION_BASE_OK;
    }

  if (ret == MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE)
    {
      g_clear_error (&my_error);
      set_mbedtls_error (error, G_TLS_ERROR_MISC, _("Peer sent fatal TLS alert"), ret);
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED)
    {
      g_clear_error (&my_error);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Unacceptable TLS certificate"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  if (my_error)
    g_propagate_error (error, my_error);
  else
    set_mbedtls_error (error, G_TLS_ERROR_MISC, gettext (err_prefix), ret);

  return G_TLS_CONNECTION_BASE_ERROR;
}

#define BEGIN_MBEDTLS_IO(self, direction, timeout, cancellable)        \
  g_tls_connection_base_push_io (G_TLS_CONNECTION_BASE (self),         \
                                 direction, timeout, cancellable);     \
  do {

#define END_MBEDTLS_IO(self, direction, ret, status, errmsg, err)      \
    status = end_mbedtls_io (self, direction, ret, err, errmsg);       \
  } while (status == G_TLS_CONNECTION_BASE_TRY_AGAIN);

static gboolean
g_tls_connection_mbedtls_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (initable);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GDatagramBased *base_socket = NULL;
  gboolean client = G_IS_TLS_CLIENT_CONNECTION (self);
  int ret;

  g_object_get (self, "base-socket", &base_socket, NULL);
  if (base_socket)
    {
      g_object_unref (base_socket);
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                           _("DTLS is not supported by the mbedtls backend"));
      return FALSE;
    }

  ret = mbedtls_ssl_config_defaults (&priv->conf,
                                     client ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0)
    {
      set_mbedtls_error (error, G_TLS_ERROR_MISC, _("Could not create TLS connection"), ret);
      return FALSE;
    }

  /* the base checks the peer, so accept-certificate works */
  mbedtls_ssl_conf_authmode (&priv->conf, MBEDTLS_SSL_VERIFY_NONE);

  ret = mbedtls_ssl_setup (&priv->ssl, &priv->conf);
  if (ret != 0)
    {
      set_mbedtls_error (error, G_TLS_ERROR_MISC, _("Could not create TLS connection"), ret);
      return FALSE;
    }

  mbedtls_ssl_set_bio (&priv->ssl, self, send_callback, recv_callback, NULL);
  return TRUE;
}

gboolean
g_tls_connection_mbedtls_set_own_certificate (GTlsConnectionMbedtls  *self,
                                              GError                **error)
{
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GTlsCertificate *cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (self));
  mbedtls_pk_context *key;
  int ret;

  if (!cert || cert == priv->own_certificate)
    return TRUE;

  if (!G_IS_TLS_CERTIFICATE_MBEDTLS (cert))
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Certificate is not from the mbedtls backend"));
      return FALSE;
    }

  key = g_tls_certificate_mbedtls_get_key (G_TLS_CERTIFICATE_MBEDTLS (cert));
  if (!key)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Certificate has no private key"));
      return FALSE;
    }

  /* send the intermediates along with the leaf */
  mbedtls_x509_crt_free (&priv->own_chain);
  mbedtls_x509_crt_init (&priv->own_chain);
  g_tls_certificate_mbedtls_chain_der (G_TLS_CERTIFICATE_MBEDTLS (cert), &priv->own_chain);

  ret = mbedtls_ssl_conf_own_cert (&priv->conf, &priv->own_chain, key);
  if (ret != 0)
    {
      set_mbedtls_error (error, G_TLS_ERROR_BAD_CERTIFICATE, _("Could not use certificate"), ret);
      return FALSE;
    }

  g_set_object (&priv->own_certificate, cert);
  return TRUE;
}

static void
g_tls_connection_mbedtls_prepare_handshake (GTlsConnectionBase  *tls,
                                            gchar              **advertised_protocols)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GError *error = NULL;

  if (advertised_protocols && !priv->alpn)
    {
      /* mbedtls keeps the pointer: the list lives with us */
      priv->alpn = g_strdupv (advertised_protocols);
      mbedtls_ssl_conf_alpn_protocols (&priv->conf, (const char * const *)priv->alpn);
    }

  if (!g_tls_connection_mbedtls_set_own_certificate (self, &error))
    {
      g_warning ("TLS certificate not used: %s", error->message);
      g_error_free (error);
    }
}

static GTlsSafeRenegotiationStatus
g_tls_connection_mbedtls_handshake_thread_safe_renegotiation_status (GTlsConnectionBase *tls)
{
  /* renegotiation is off in mbedtls by default */
  return G_TLS_SAFE_RENEGOTIATION_UNSUPPORTED;
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_handshake_thread_request_rehandshake (GTlsConnectionBase  *tls,
                                                               gint64               timeout,
                                                               GCancellable        *cancellable,
                                                               GError             **error)
{
  return G_TLS_CONNECTION_BASE_OK;
}

static GTlsCertificate *
g_tls_connection_mbedtls_retrieve_peer_certificate (GTlsConnectionBase *tls)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  const mbedtls_x509_crt *peer = mbedtls_ssl_get_peer_cert (&priv->ssl);

  if (!peer || !peer->raw.p)
    return NULL;
  return g_tls_certificate_mbedtls_build_chain (peer);
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_handshake_thread_handshake (GTlsConnectionBase  *tls,
                                                     gint64               timeout,
                                                     GCancellable        *cancellable,
                                                     GError             **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GTlsConnectionBaseStatus status;
  int ret;

  BEGIN_MBEDTLS_IO (self, G_IO_IN | G_IO_OUT, timeout, cancellable);
  ret = mbedtls_ssl_handshake (&priv->ssl);
  END_MBEDTLS_IO (self, G_IO_IN | G_IO_OUT, ret, status,
                  N_("Error performing TLS handshake"), error);

  if (status != G_TLS_CONNECTION_BASE_OK)
    return status;

  /* before any data: the base checks the peer and may refuse */
  if (mbedtls_ssl_get_peer_cert (&priv->ssl) &&
      !g_tls_connection_base_handshake_thread_verify_certificate (tls))
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("Unacceptable TLS certificate"));
      return G_TLS_CONNECTION_BASE_ERROR;
    }

  return G_TLS_CONNECTION_BASE_OK;
}

static GTlsCertificateFlags
g_tls_connection_mbedtls_verify_chain (GTlsConnectionBase       *tls,
                                       GTlsCertificate          *chain,
                                       const gchar              *purpose,
                                       GSocketConnectable       *identity,
                                       GTlsInteraction          *interaction,
                                       GTlsDatabaseVerifyFlags   flags,
                                       GCancellable             *cancellable,
                                       GError                  **error)
{
  GTlsDatabase *database = g_tls_connection_get_database (G_TLS_CONNECTION (tls));

  if (!database)
    return G_TLS_CERTIFICATE_UNKNOWN_CA;
  return g_tls_database_verify_chain (database, chain, purpose, identity,
                                      interaction, flags, cancellable, error);
}

static void
g_tls_connection_mbedtls_complete_handshake (GTlsConnectionBase   *tls,
                                             gboolean              handshake_succeeded,
                                             gchar               **negotiated_protocol,
                                             GTlsProtocolVersion  *protocol_version,
                                             gchar               **ciphersuite_name,
                                             GError              **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  const char *alpn;

  if (!handshake_succeeded)
    return;

  alpn = mbedtls_ssl_get_alpn_protocol (&priv->ssl);
  if (alpn)
    {
      g_assert (!*negotiated_protocol);
      *negotiated_protocol = g_strdup (alpn);
    }

  switch (mbedtls_ssl_get_version_number (&priv->ssl))
    {
    case MBEDTLS_SSL_VERSION_TLS1_2:
      *protocol_version = G_TLS_PROTOCOL_VERSION_TLS_1_2;
      break;
    case MBEDTLS_SSL_VERSION_TLS1_3:
      *protocol_version = G_TLS_PROTOCOL_VERSION_TLS_1_3;
      break;
    default:
      *protocol_version = G_TLS_PROTOCOL_VERSION_UNKNOWN;
    }

  *ciphersuite_name = g_strdup (mbedtls_ssl_get_ciphersuite (&priv->ssl));
}

static gboolean
g_tls_connection_mbedtls_is_session_resumed (GTlsConnectionBase *tls)
{
  return FALSE;
}

static gboolean
g_tls_connection_mbedtls_get_channel_binding_data (GTlsConnectionBase      *tls,
                                                   GTlsChannelBindingType   type,
                                                   GByteArray              *data,
                                                   GError                 **error)
{
  g_set_error (error, G_TLS_CHANNEL_BINDING_ERROR, G_TLS_CHANNEL_BINDING_ERROR_NOT_IMPLEMENTED,
               _("Channel binding is not implemented by the mbedtls backend"));
  return FALSE;
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_read (GTlsConnectionBase  *tls,
                               void                *buffer,
                               gsize                count,
                               gint64               timeout,
                               gssize              *nread,
                               GCancellable        *cancellable,
                               GError             **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GTlsConnectionBaseStatus status;
  int ret;

  BEGIN_MBEDTLS_IO (self, G_IO_IN, timeout, cancellable);
  ret = mbedtls_ssl_read (&priv->ssl, buffer, count);
  /* the peer's close_notify is an orderly end of data */
  if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
    ret = 0;
  END_MBEDTLS_IO (self, G_IO_IN, ret, status, N_("Error reading data from TLS socket"), error);

  *nread = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_write (GTlsConnectionBase  *tls,
                                const void          *buffer,
                                gsize                count,
                                gint64               timeout,
                                gssize              *nwrote,
                                GCancellable        *cancellable,
                                GError             **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GTlsConnectionBaseStatus status;
  int ret;

  BEGIN_MBEDTLS_IO (self, G_IO_OUT, timeout, cancellable);
  ret = mbedtls_ssl_write (&priv->ssl, buffer, count);
  END_MBEDTLS_IO (self, G_IO_OUT, ret, status, N_("Error writing data to TLS socket"), error);

  *nwrote = MAX (ret, 0);
  return status;
}

static GTlsConnectionBaseStatus
datagram_unsupported (GError **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("DTLS is not supported by the mbedtls backend"));
  return G_TLS_CONNECTION_BASE_ERROR;
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_read_message (GTlsConnectionBase  *tls,
                                       GInputVector        *vectors,
                                       guint                num_vectors,
                                       gint64               timeout,
                                       gssize              *nread,
                                       GCancellable        *cancellable,
                                       GError             **error)
{
  *nread = 0;
  return datagram_unsupported (error);
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_write_message (GTlsConnectionBase  *tls,
                                        GOutputVector       *vectors,
                                        guint                num_vectors,
                                        gint64               timeout,
                                        gssize              *nwrote,
                                        GCancellable        *cancellable,
                                        GError             **error)
{
  *nwrote = 0;
  return datagram_unsupported (error);
}

static GTlsConnectionBaseStatus
g_tls_connection_mbedtls_close (GTlsConnectionBase  *tls,
                                gint64               timeout,
                                GCancellable        *cancellable,
                                GError             **error)
{
  GTlsConnectionMbedtls *self = G_TLS_CONNECTION_MBEDTLS (tls);
  GTlsConnectionMbedtlsPrivate *priv = g_tls_connection_mbedtls_get_instance_private (self);
  GTlsConnectionBaseStatus status;
  int ret;

  BEGIN_MBEDTLS_IO (self, G_IO_IN | G_IO_OUT, timeout, cancellable);
  ret = mbedtls_ssl_close_notify (&priv->ssl);
  END_MBEDTLS_IO (self, G_IO_IN | G_IO_OUT, ret, status, N_("Error performing TLS close"), error);

  return status;
}

static void
g_tls_connection_mbedtls_class_init (GTlsConnectionMbedtlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsConnectionBaseClass *base_class = G_TLS_CONNECTION_BASE_CLASS (klass);

  gobject_class->finalize = g_tls_connection_mbedtls_finalize;

  base_class->prepare_handshake                          = g_tls_connection_mbedtls_prepare_handshake;
  base_class->handshake_thread_safe_renegotiation_status = g_tls_connection_mbedtls_handshake_thread_safe_renegotiation_status;
  base_class->handshake_thread_request_rehandshake       = g_tls_connection_mbedtls_handshake_thread_request_rehandshake;
  base_class->handshake_thread_handshake                 = g_tls_connection_mbedtls_handshake_thread_handshake;
  base_class->retrieve_peer_certificate                  = g_tls_connection_mbedtls_retrieve_peer_certificate;
  base_class->verify_chain                               = g_tls_connection_mbedtls_verify_chain;
  base_class->complete_handshake                         = g_tls_connection_mbedtls_complete_handshake;
  base_class->is_session_resumed                         = g_tls_connection_mbedtls_is_session_resumed;
  base_class->get_channel_binding_data                   = g_tls_connection_mbedtls_get_channel_binding_data;
  base_class->read_fn                                    = g_tls_connection_mbedtls_read;
  base_class->read_message_fn                            = g_tls_connection_mbedtls_read_message;
  base_class->write_fn                                   = g_tls_connection_mbedtls_write;
  base_class->write_message_fn                           = g_tls_connection_mbedtls_write_message;
  base_class->close_fn                                   = g_tls_connection_mbedtls_close;
}

static void
g_tls_connection_mbedtls_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_tls_connection_mbedtls_initable_init;
}
