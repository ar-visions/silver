#include <stdint.h>
#include <stdlib.h>

#include <jni.h>

/* Android's ASurfaceTexture over the Java SurfaceTexture, which
 * here holds no frames: attach, detach and update answer done */

JNIEnv *get_jni_env(void);

typedef struct ASurfaceTexture {
	jobject texture;
} ASurfaceTexture;

ASurfaceTexture *ASurfaceTexture_fromSurfaceTexture(JNIEnv *env, jobject surfacetexture)
{
	if (!surfacetexture)
		return NULL;
	ASurfaceTexture *st = calloc(1, sizeof *st);
	st->texture = (*env)->NewGlobalRef(env, surfacetexture);
	return st;
}

void ASurfaceTexture_release(ASurfaceTexture *st)
{
	if (!st)
		return;
	JNIEnv *env = get_jni_env();
	if (env && st->texture)
		(*env)->DeleteGlobalRef(env, st->texture);
	free(st);
}

int ASurfaceTexture_attachToGLContext(ASurfaceTexture *st, uint32_t texName)
{
	return 0;
}

int ASurfaceTexture_detachFromGLContext(ASurfaceTexture *st)
{
	return 0;
}

int ASurfaceTexture_updateTexImage(ASurfaceTexture *st)
{
	return 0;
}

void ASurfaceTexture_getTransformMatrix(ASurfaceTexture *st, float mtx[16])
{
	for (int i = 0; i < 16; i++)
		mtx[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

int64_t ASurfaceTexture_getTimestamp(ASurfaceTexture *st)
{
	return 0;
}
