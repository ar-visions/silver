// no keyring: no saved accounts
#include <jni.h>
#include "../generated_headers/android_accounts_AccountManager.h"

JNIEXPORT jstring JNICALL Java_android_accounts_AccountManager_nativeGet(JNIEnv *env, jclass clazz, jstring type_jstr, jstring name_jstr, jstring key_jstr)
{
	return 0;
}

JNIEXPORT jboolean JNICALL Java_android_accounts_AccountManager_nativeSet(JNIEnv *env, jclass clazz, jstring type_jstr, jstring name_jstr, jstring key_jstr, jstring value_jstr)
{
	return 0;
}

JNIEXPORT jboolean JNICALL Java_android_accounts_AccountManager_nativeRemove(JNIEnv *env, jclass clazz, jstring type_jstr, jstring name_jstr)
{
	return 0;
}

JNIEXPORT jstring JNICALL Java_android_accounts_AccountManager_nativeListAccounts(JNIEnv *env, jclass clazz)
{
	return 0;
}
