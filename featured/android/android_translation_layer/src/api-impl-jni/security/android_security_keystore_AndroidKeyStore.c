// no keyring: no stored keys
#include <jni.h>
#include "../generated_headers/android_security_keystore_AndroidKeyStore.h"

JNIEXPORT void JNICALL Java_android_security_keystore_AndroidKeyStore_nativeSetKey(JNIEnv *env, jclass clazz, jstring package_name_jstr, jstring alias_jstr, jbyteArray key_jobj)
{
}

JNIEXPORT jbyteArray JNICALL Java_android_security_keystore_AndroidKeyStore_nativeGetKey(JNIEnv *env, jclass clazz, jstring package_name_jstr, jstring alias_jstr)
{
	return 0;
}
