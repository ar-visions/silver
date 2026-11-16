#define _DEFAULT_SOURCE
// a server's own identity: an ECDSA P-256 key and a self-signed
// certificate, made once into key_path and crt_path (PEM), then
// loaded into an ssl config. mbedtls 4 makes keys through PSA,
// whose setters are inline macros: C, not silver
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <psa/crypto.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/ssl.h>
#include <stdlib.h>

#define TLS_API __attribute__((visibility("default")))

static int file_there(const char* p)
{
    struct stat st;
    return p && stat(p, &st) == 0 && st.st_size > 0;
}

static int write_text(const char* p, const char* text)
{
    FILE* f = fopen(p, "wb");
    if (!f) return 0;
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    fclose(f);
    chmod(p, 0600);
    return ok;
}

TLS_API int tls_cert_make(const char* key_path,
                                  const char* crt_path,
                                  const char* name)
{
    if (file_there(key_path) && file_there(crt_path)) return 1;
    if (psa_crypto_init() != PSA_SUCCESS) return 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attr,
        PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attr, 256);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH
        | PSA_KEY_USAGE_VERIFY_HASH | PSA_KEY_USAGE_EXPORT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    mbedtls_svc_key_id_t id = MBEDTLS_SVC_KEY_ID_INIT;
    if (psa_generate_key(&attr, &id) != PSA_SUCCESS) return 0;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    int ok = 0;
    unsigned char key_pem[2048];
    unsigned char crt_pem[4096];
    if (mbedtls_pk_copy_from_psa(id, &pk) != 0) goto done;
    if (mbedtls_pk_write_key_pem(&pk, key_pem, sizeof key_pem) != 0)
        goto done;
    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);
    mbedtls_x509write_crt_set_version(&crt,
        MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&crt, &pk);
    mbedtls_x509write_crt_set_issuer_key(&crt, &pk);
    char dn[256];
    snprintf(dn, sizeof dn, "CN=%s", (name && *name) ? name : "commune");
    if (mbedtls_x509write_crt_set_subject_name(&crt, dn) != 0) goto crt_done;
    if (mbedtls_x509write_crt_set_issuer_name(&crt, dn) != 0) goto crt_done;
    unsigned char serial[8];
    time_t now = time(NULL);
    for (int i = 0; i < 8; i++) serial[i] = (now >> (8 * (7 - i))) & 0xff;
    serial[0] |= 1;
    mbedtls_x509write_crt_set_serial_raw(&crt, serial, sizeof serial);
    struct tm t0, t1;
    gmtime_r(&now, &t0);
    t1 = t0;
    t1.tm_year += 20;
    char from[16], to[16];
    strftime(from, sizeof from, "%Y%m%d%H%M%S", &t0);
    strftime(to, sizeof to, "%Y%m%d%H%M%S", &t1);
    if (mbedtls_x509write_crt_set_validity(&crt, from, to) != 0)
        goto crt_done;
    mbedtls_x509write_crt_set_basic_constraints(&crt, 0, -1);
    if (mbedtls_x509write_crt_pem(&crt, crt_pem, sizeof crt_pem) != 0)
        goto crt_done;
    ok = write_text(key_path, (const char*)key_pem)
      && write_text(crt_path, (const char*)crt_pem);
crt_done:
    mbedtls_x509write_crt_free(&crt);
done:
    mbedtls_pk_free(&pk);
    psa_destroy_key(id);
    return ok;
}

// the key and certificate loaded into conf as its own; the
// identity handed back, kept while the config is used
typedef struct {
    mbedtls_x509_crt crt;
    mbedtls_pk_context pk;
} tls_identity_t;

TLS_API void* tls_identity_load(const char* key_path,
                                const char* crt_path,
                                mbedtls_ssl_config* conf)
{
    tls_identity_t* id = calloc(1, sizeof *id);
    if (!id) return NULL;
    mbedtls_x509_crt_init(&id->crt);
    mbedtls_pk_init(&id->pk);
    if (mbedtls_x509_crt_parse_file(&id->crt, crt_path) != 0
        || mbedtls_pk_parse_keyfile(&id->pk, key_path, NULL) != 0
        || mbedtls_ssl_conf_own_cert(conf, &id->crt, &id->pk) != 0) {
        mbedtls_x509_crt_free(&id->crt);
        mbedtls_pk_free(&id->pk);
        free(id);
        return NULL;
    }
    return id;
}

// the certificate's bytes (DER) and their count
TLS_API const unsigned char* tls_identity_der(void* idp, size_t* n)
{
    tls_identity_t* id = idp;
    if (!id) { *n = 0; return NULL; }
    *n = id->crt.raw.len;
    return id->crt.raw.p;
}

TLS_API void tls_identity_free(void* idp)
{
    tls_identity_t* id = idp;
    if (!id) return;
    mbedtls_x509_crt_free(&id->crt);
    mbedtls_pk_free(&id->pk);
    free(id);
}
