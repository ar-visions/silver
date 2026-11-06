// Java's TLS sockets over our tls module (libsilver-tls)
#include <jni.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// tls.ag's plain calls
int tls_wrap(int fd, const char *host, const char *alpn, bool verify, int wait_ms);
int tls_failure(int id);
int64_t tls_read(int id, uint8_t *data, int64_t n, int wait_ms);
int tls_write(int id, uint8_t *data, int64_t n, int wait_ms);
int64_t tls_pending(int id);
void tls_close(int id);
uint32_t tls_verify_flags(int id);
const char *tls_cipher(int id);
const char *tls_version(int id);
const char *tls_alpn(int id);
uint8_t *tls_peer_cert(int id, int i, int64_t *len);
int tls_verify_text(uint32_t flags, uint8_t *out, int64_t cap);
void tls_error_text(int code, uint8_t *out, int64_t cap);
int64_t tls_check_chain(uint8_t *ders, int64_t *lens, int n, const char *host);
int tls_cipher_count(void);
const char *tls_cipher_name(int i);

static jstring utf(JNIEnv *env, const char *s)
{
	return s ? (*env)->NewStringUTF(env, s) : NULL;
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nWrap(JNIEnv *env, jclass cls, jint fd, jstring host, jstring alpn, jboolean verify, jint ms)
{
	const char *h = host ? (*env)->GetStringUTFChars(env, host, NULL) : NULL;
	const char *a = alpn ? (*env)->GetStringUTFChars(env, alpn, NULL) : NULL;
	int id = tls_wrap(fd, h, a, verify, ms);
	if (h)
		(*env)->ReleaseStringUTFChars(env, host, h);
	if (a)
		(*env)->ReleaseStringUTFChars(env, alpn, a);
	return id;
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nFailure(JNIEnv *env, jclass cls, jint id)
{
	return tls_failure(id);
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nRead(JNIEnv *env, jclass cls, jint id, jbyteArray b, jint off, jint len, jint ms)
{
	uint8_t tmp[16384];
	if (len > (jint)sizeof(tmp))
		len = sizeof(tmp);
	int64_t r = tls_read(id, tmp, len, ms);
	if (r > 0)
		(*env)->SetByteArrayRegion(env, b, off, (jsize)r, (jbyte *)tmp);
	return (jint)r;
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nWrite(JNIEnv *env, jclass cls, jint id, jbyteArray b, jint off, jint len, jint ms)
{
	jbyte *p = (*env)->GetByteArrayElements(env, b, NULL);
	int r = tls_write(id, (uint8_t *)p + off, len, ms);
	(*env)->ReleaseByteArrayElements(env, b, p, JNI_ABORT);
	return r;
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nPending(JNIEnv *env, jclass cls, jint id)
{
	return (jint)tls_pending(id);
}

JNIEXPORT void JNICALL Java_com_android_org_conscrypt_TlsNative_nClose(JNIEnv *env, jclass cls, jint id)
{
	tls_close(id);
}

JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nVerifyResult(JNIEnv *env, jclass cls, jint id)
{
	return (jint)tls_verify_flags(id);
}

JNIEXPORT jstring JNICALL Java_com_android_org_conscrypt_TlsNative_nVerifyInfo(JNIEnv *env, jclass cls, jint flags)
{
	uint8_t buf[1024] = {0};
	if (tls_verify_text((uint32_t)flags, buf, sizeof(buf)) < 0)
		buf[0] = 0;
	return utf(env, (const char *)buf);
}

JNIEXPORT jstring JNICALL Java_com_android_org_conscrypt_TlsNative_nErrorText(JNIEnv *env, jclass cls, jint code)
{
	uint8_t buf[256] = {0};
	tls_error_text(code, buf, sizeof(buf));
	return utf(env, (const char *)buf);
}

JNIEXPORT jobjectArray JNICALL Java_com_android_org_conscrypt_TlsNative_nPeerChain(JNIEnv *env, jclass cls, jint id)
{
	int64_t len;
	int n = 0;
	while (tls_peer_cert(id, n, &len))
		n++;
	jobjectArray out = (*env)->NewObjectArray(env, n, (*env)->FindClass(env, "[B"), NULL);
	for (int i = 0; i < n; i++) {
		uint8_t *p = tls_peer_cert(id, i, &len);
		jbyteArray der = (*env)->NewByteArray(env, (jsize)len);
		(*env)->SetByteArrayRegion(env, der, 0, (jsize)len, (const jbyte *)p);
		(*env)->SetObjectArrayElement(env, out, i, der);
		(*env)->DeleteLocalRef(env, der);
	}
	return out;
}

JNIEXPORT jstring JNICALL Java_com_android_org_conscrypt_TlsNative_nCipher(JNIEnv *env, jclass cls, jint id)
{
	return utf(env, tls_cipher(id));
}

JNIEXPORT jstring JNICALL Java_com_android_org_conscrypt_TlsNative_nVersion(JNIEnv *env, jclass cls, jint id)
{
	return utf(env, tls_version(id));
}

JNIEXPORT jstring JNICALL Java_com_android_org_conscrypt_TlsNative_nAlpn(JNIEnv *env, jclass cls, jint id)
{
	return utf(env, tls_alpn(id));
}

// DER certificates joined end to end for tls_check_chain
JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nVerifyChain(JNIEnv *env, jclass cls, jobjectArray ders, jstring host)
{
	jsize n = (*env)->GetArrayLength(env, ders);
	int64_t *lens = calloc(n ? n : 1, sizeof(int64_t));
	int64_t total = 0;
	for (jsize i = 0; i < n; i++) {
		jbyteArray d = (*env)->GetObjectArrayElement(env, ders, i);
		lens[i] = (*env)->GetArrayLength(env, d);
		total += lens[i];
		(*env)->DeleteLocalRef(env, d);
	}
	uint8_t *all = malloc(total ? total : 1);
	int64_t at = 0;
	for (jsize i = 0; i < n; i++) {
		jbyteArray d = (*env)->GetObjectArrayElement(env, ders, i);
		(*env)->GetByteArrayRegion(env, d, 0, (jsize)lens[i], (jbyte *)all + at);
		at += lens[i];
		(*env)->DeleteLocalRef(env, d);
	}
	const char *h = host ? (*env)->GetStringUTFChars(env, host, NULL) : NULL;
	int64_t flags = tls_check_chain(all, lens, n, h);
	if (h)
		(*env)->ReleaseStringUTFChars(env, host, h);
	free(all);
	free(lens);
	return (jint)flags;
}

JNIEXPORT jobjectArray JNICALL Java_com_android_org_conscrypt_TlsNative_nCipherNames(JNIEnv *env, jclass cls)
{
	int n = tls_cipher_count();
	jobjectArray out = (*env)->NewObjectArray(env, n, (*env)->FindClass(env, "java/lang/String"), NULL);
	for (int i = 0; i < n; i++) {
		jstring s = utf(env, tls_cipher_name(i));
		(*env)->SetObjectArrayElement(env, out, i, s);
		(*env)->DeleteLocalRef(env, s);
	}
	return out;
}

// a field of an object, or null (the exception cleared)
static jobject field(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig)
{
	jclass c = (*env)->FindClass(env, cls);
	jfieldID f = c ? (*env)->GetFieldID(env, c, name, sig) : NULL;
	if (!f) {
		(*env)->ExceptionClear(env);
		return NULL;
	}
	return (*env)->GetObjectField(env, o, f);
}

// a plain socket's descriptor, read from its SocketImpl
JNIEXPORT jint JNICALL Java_com_android_org_conscrypt_TlsNative_nFd(JNIEnv *env, jclass cls, jobject sock)
{
	jobject impl = field(env, sock, "java/net/Socket", "impl", "Ljava/net/SocketImpl;");
	jobject fdo = impl ? field(env, impl, "java/net/SocketImpl", "fd", "Ljava/io/FileDescriptor;") : NULL;
	if (!fdo)
		return -1;
	jclass fc = (*env)->FindClass(env, "java/io/FileDescriptor");
	jfieldID d = (*env)->GetFieldID(env, fc, "descriptor", "I");
	if (!d) {
		(*env)->ExceptionClear(env);
		return -1;
	}
	return (*env)->GetIntField(env, fdo, d);
}
