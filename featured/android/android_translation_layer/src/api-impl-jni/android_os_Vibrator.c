// no vibrator on a PC
#include <jni.h>
#include "generated_headers/android_hardware_SensorManager.h"

JNIEXPORT jint JNICALL Java_android_os_Vibrator_native_1constructor(JNIEnv *env, jobject this)
{
	return 0;
}

JNIEXPORT void JNICALL Java_android_os_Vibrator_native_1vibrate(JNIEnv *env, jobject this, jint fd, jlong duration)
{
}
