#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>

#include "looper.h"

ALooper *ALooper_forThread(void);

/* Android's AChoreographer: frame callbacks on the thread's looper.
 * No display pulse here: a frame comes from a timer at 60 Hz, the
 * timer an fd on the ALooper, so the callbacks run as it polls */

#define FRAME_NANOS 16666667

typedef struct AChoreographer AChoreographer;
typedef struct AChoreographerFrameCallbackData {
	int64_t frame_time;
} AChoreographerFrameCallbackData;

typedef void (*AChoreographer_frameCallback)(long frameTimeNanos, void *data);
typedef void (*AChoreographer_frameCallback64)(int64_t frameTimeNanos, void *data);
typedef void (*AChoreographer_vsyncCallback)(const AChoreographerFrameCallbackData *data, void *data2);
typedef void (*AChoreographer_refreshRateCallback)(int64_t vsyncPeriodNanos, void *data);

struct pending {
	int kind; /* 0 frame, 1 frame64, 2 vsync */
	void *fn;
	void *data;
	int64_t due;
};

struct AChoreographer {
	ALooper *looper;
	int fd;
	GMutex lock;
	GQueue *pending;
};

static __thread AChoreographer *thread_choreographer;

static int64_t now_nanos(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static void arm(AChoreographer *c)
{
	int64_t due = 0;
	for (GList *l = c->pending->head; l; l = l->next) {
		struct pending *p = l->data;
		if (!due || p->due < due)
			due = p->due;
	}
	struct itimerspec it = {0};
	if (due) {
		int64_t at = due > now_nanos() + 1 ? due : now_nanos() + 1;
		it.it_value.tv_sec = at / 1000000000;
		it.it_value.tv_nsec = at % 1000000000;
	}
	timerfd_settime(c->fd, TFD_TIMER_ABSTIME, &it, NULL);
}

static int on_frame(int fd, int events, void *data)
{
	AChoreographer *c = data;
	uint64_t ticks;
	if (read(fd, &ticks, sizeof ticks) < 0) {}
	int64_t frame = now_nanos();
	GQueue *ready = g_queue_new();
	g_mutex_lock(&c->lock);
	for (GList *l = c->pending->head; l;) {
		GList *next = l->next;
		struct pending *p = l->data;
		if (p->due <= frame) {
			g_queue_unlink(c->pending, l);
			g_queue_push_tail(ready, p);
		}
		l = next;
	}
	arm(c);
	g_mutex_unlock(&c->lock);
	struct pending *p;
	while ((p = g_queue_pop_head(ready))) {
		if (p->kind == 0)
			((AChoreographer_frameCallback)p->fn)((long)frame, p->data);
		else if (p->kind == 1)
			((AChoreographer_frameCallback64)p->fn)(frame, p->data);
		else {
			AChoreographerFrameCallbackData d = { .frame_time = frame };
			((AChoreographer_vsyncCallback)p->fn)(&d, p->data);
		}
		free(p);
	}
	g_queue_free(ready);
	return 1;
}

AChoreographer *AChoreographer_getInstance(void)
{
	if (thread_choreographer)
		return thread_choreographer;
	ALooper *looper = ALooper_forThread();
	if (!looper)
		return NULL;
	AChoreographer *c = calloc(1, sizeof *c);
	c->looper = looper;
	c->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	g_mutex_init(&c->lock);
	c->pending = g_queue_new();
	ALooper_addFd(looper, c->fd, 0, ALOOPER_EVENT_INPUT, on_frame, c);
	thread_choreographer = c;
	return c;
}

static void post(AChoreographer *c, int kind, void *fn, void *data, int64_t delay)
{
	if (!c || !fn)
		return;
	struct pending *p = calloc(1, sizeof *p);
	p->kind = kind;
	p->fn = fn;
	p->data = data;
	p->due = now_nanos() + (delay > FRAME_NANOS ? delay : FRAME_NANOS);
	g_mutex_lock(&c->lock);
	g_queue_push_tail(c->pending, p);
	arm(c);
	g_mutex_unlock(&c->lock);
}

void AChoreographer_postFrameCallback(AChoreographer *c, AChoreographer_frameCallback fn, void *data)
{
	post(c, 0, fn, data, 0);
}

void AChoreographer_postFrameCallbackDelayed(AChoreographer *c, AChoreographer_frameCallback fn, void *data, long delayMillis)
{
	post(c, 0, fn, data, (int64_t)delayMillis * 1000000);
}

void AChoreographer_postFrameCallback64(AChoreographer *c, AChoreographer_frameCallback64 fn, void *data)
{
	post(c, 1, fn, data, 0);
}

void AChoreographer_postFrameCallbackDelayed64(AChoreographer *c, AChoreographer_frameCallback64 fn, void *data, uint32_t delayMillis)
{
	post(c, 1, fn, data, (int64_t)delayMillis * 1000000);
}

void AChoreographer_postVsyncCallback(AChoreographer *c, AChoreographer_vsyncCallback fn, void *data)
{
	post(c, 2, fn, data, 0);
}

/* the rate is told once at registration, as Android does */
void AChoreographer_registerRefreshRateCallback(AChoreographer *c, AChoreographer_refreshRateCallback fn, void *data)
{
	if (fn)
		fn(FRAME_NANOS, data);
}

void AChoreographer_unregisterRefreshRateCallback(AChoreographer *c, AChoreographer_refreshRateCallback fn, void *data)
{
}

int64_t AChoreographerFrameCallbackData_getFrameTimeNanos(const AChoreographerFrameCallbackData *d)
{
	return d->frame_time;
}

size_t AChoreographerFrameCallbackData_getFrameTimelinesLength(const AChoreographerFrameCallbackData *d)
{
	return 1;
}

size_t AChoreographerFrameCallbackData_getPreferredFrameTimelineIndex(const AChoreographerFrameCallbackData *d)
{
	return 0;
}

int64_t AChoreographerFrameCallbackData_getFrameTimelineVsyncId(const AChoreographerFrameCallbackData *d, size_t index)
{
	return -1;
}

int64_t AChoreographerFrameCallbackData_getFrameTimelineExpectedPresentationTimeNanos(const AChoreographerFrameCallbackData *d, size_t index)
{
	return d->frame_time + FRAME_NANOS;
}

int64_t AChoreographerFrameCallbackData_getFrameTimelineDeadlineNanos(const AChoreographerFrameCallbackData *d, size_t index)
{
	return d->frame_time + FRAME_NANOS;
}
