// windows audio backend for spectra: WASAPI, 16-bit interleaved.
// the .ag declares these as intern funcs; COM never leaves this file.
// silver attaches <module>.cc on every platform, so the whole file is
// windows-only: elsewhere alsa/coreaudio serve these classes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <string.h>
#include <stdlib.h>

// shared mode otherwise forces the device mix format on us; these two
// let a plain 16-bit ask through and windows converts behind it
static const DWORD conv_flags =
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

struct wasapi {
    IMMDeviceEnumerator* enumr;
    IMMDevice*           dev;
    IAudioClient*        client;
    IAudioCaptureClient* cap;
    IAudioRenderClient*  ren;
    short*               ring;       // capture fifo, interleaved
    int                  ring_cap;   // frames it holds
    int                  ring_len;   // frames in it now
    int                  channels;
    bool                 started;
};

static void wasapi_free(wasapi* w) {
    if (!w) return;
    if (w->client && w->started) w->client->Stop();
    if (w->cap)    w->cap->Release();
    if (w->ren)    w->ren->Release();
    if (w->client) w->client->Release();
    if (w->dev)    w->dev->Release();
    if (w->enumr)  w->enumr->Release();
    free(w->ring);
    free(w);
}

// com is per-thread; already-initialized is not an error for us
static void com_up(void) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
}

static wasapi* device_open(bool capture, int rate, int channels, int frames) {
    com_up();
    wasapi* w = (wasapi*)calloc(1, sizeof(wasapi));
    if (!w) return NULL;
    w->channels = channels > 0 ? channels : 1;

    // __uuidof, not the IID_ constants: those are EXTERN_C declarations whose
    // definitions ship in no sdk lib here. the interfaces carry the uuid on
    // themselves (MIDL_INTERFACE / DECLSPEC_UUID), so it resolves at compile
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&w->enumr);
    if (FAILED(hr)) { wasapi_free(w); return NULL; }

    hr = w->enumr->GetDefaultAudioEndpoint(capture ? eCapture : eRender,
                                           eConsole, &w->dev);
    if (FAILED(hr)) { wasapi_free(w); return NULL; }

    hr = w->dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&w->client);
    if (FAILED(hr)) { wasapi_free(w); return NULL; }

    WAVEFORMATEX wf;
    memset(&wf, 0, sizeof(wf));
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = (WORD)w->channels;
    wf.nSamplesPerSec  = (DWORD)rate;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = (WORD)(w->channels * 2);
    wf.nAvgBytesPerSec = (DWORD)(rate * wf.nBlockAlign);

    // hns units; a 100ms floor keeps short asks from starving the mixer
    REFERENCE_TIME dur = (REFERENCE_TIME)(10000000LL * (frames > 0 ? frames : 1024) * 4 / rate);
    if (dur < 1000000) dur = 1000000;

    hr = w->client->Initialize(AUDCLNT_SHAREMODE_SHARED, conv_flags, dur, 0, &wf, NULL);
    if (FAILED(hr)) { wasapi_free(w); return NULL; }

    if (capture) {
        hr = w->client->GetService(__uuidof(IAudioCaptureClient), (void**)&w->cap);
        if (FAILED(hr)) { wasapi_free(w); return NULL; }
        // room for several asks: packets arrive in device-sized bursts
        int cap_frames = (frames > 0 ? frames : 1024) * 8;
        if (cap_frames < rate / 2) cap_frames = rate / 2;
        w->ring     = (short*)calloc((size_t)cap_frames * w->channels, sizeof(short));
        w->ring_cap = cap_frames;
        if (!w->ring) { wasapi_free(w); return NULL; }
    } else {
        hr = w->client->GetService(__uuidof(IAudioRenderClient), (void**)&w->ren);
        if (FAILED(hr)) { wasapi_free(w); return NULL; }
    }
    return w;
}

// append frames, dropping the oldest when the fifo is full
static void ring_push(wasapi* w, const short* src, int frames) {
    if (frames <= 0) return;
    int ch = w->channels;
    if (frames > w->ring_cap) {
        if (src) src += (size_t)(frames - w->ring_cap) * ch;
        frames = w->ring_cap;
    }
    int over = w->ring_len + frames - w->ring_cap;
    if (over > 0) {
        memmove(w->ring, w->ring + (size_t)over * ch,
                (size_t)(w->ring_len - over) * ch * sizeof(short));
        w->ring_len -= over;
    }
    short* dst = w->ring + (size_t)w->ring_len * ch;
    if (src) memcpy(dst, src, (size_t)frames * ch * sizeof(short));
    else     memset(dst, 0,   (size_t)frames * ch * sizeof(short));
    w->ring_len += frames;
}

// drain every queued packet into the fifo
static void ring_pump(wasapi* w) {
    for (;;) {
        UINT32 packet = 0;
        if (FAILED(w->cap->GetNextPacketSize(&packet)) || !packet) return;
        BYTE*  data  = NULL;
        UINT32 avail = 0;
        DWORD  flags = 0;
        if (FAILED(w->cap->GetBuffer(&data, &avail, &flags, NULL, NULL))) return;
        bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        ring_push(w, silent ? NULL : (const short*)data, (int)avail);
        w->cap->ReleaseBuffer(avail);
    }
}

extern "C" {

void* wasapi_cap_open(const char* device, int rate, int channels, int frames) {
    (void)device;   // endpoints are not named the way alsa names them
    return device_open(true, rate, channels, frames);
}

int wasapi_cap_read(void* ctx, short* dst, int frames) {
    wasapi* w = (wasapi*)ctx;
    if (!w || !w->cap || frames <= 0) return -1;
    if (!w->started) {
        if (FAILED(w->client->Start())) return -1;
        w->started = true;
    }
    // the snd_pcm_readi this stands in for blocks, so block too
    for (int spin = 0; spin < 2000; spin++) {
        ring_pump(w);
        if (w->ring_len >= frames) break;
        Sleep(1);
    }
    if (w->ring_len < frames) return -1;
    int ch = w->channels;
    memcpy(dst, w->ring, (size_t)frames * ch * sizeof(short));
    memmove(w->ring, w->ring + (size_t)frames * ch,
            (size_t)(w->ring_len - frames) * ch * sizeof(short));
    w->ring_len -= frames;
    return frames;
}

void wasapi_cap_reset(void* ctx) {
    wasapi* w = (wasapi*)ctx;
    if (!w || !w->client) return;
    if (w->started) { w->client->Stop(); w->started = false; }
    w->client->Reset();
    w->ring_len = 0;
}

void wasapi_cap_close(void* ctx) {
    wasapi_free((wasapi*)ctx);
}

void* wasapi_out_open(const char* device, int rate, int channels, int frames) {
    (void)device;
    return device_open(false, rate, channels, frames);
}

int wasapi_out_write(void* ctx, const short* src, int frames) {
    wasapi* w = (wasapi*)ctx;
    if (!w || !w->ren || frames <= 0) return -1;
    if (!w->started) {
        if (FAILED(w->client->Start())) return -1;
        w->started = true;
    }
    UINT32 total = 0;
    if (FAILED(w->client->GetBufferSize(&total))) return -1;
    int ch   = w->channels;
    int done = 0;
    while (done < frames) {
        UINT32 pad = 0;
        if (FAILED(w->client->GetCurrentPadding(&pad))) return -1;
        UINT32 space = total - pad;
        if (!space) { Sleep(1); continue; }
        UINT32 n = (UINT32)(frames - done);
        if (n > space) n = space;
        BYTE* out = NULL;
        if (FAILED(w->ren->GetBuffer(n, &out))) return -1;
        memcpy(out, src + (size_t)done * ch, (size_t)n * ch * sizeof(short));
        w->ren->ReleaseBuffer(n, 0);
        done += (int)n;
    }
    return done;
}

void wasapi_out_close(void* ctx) {
    wasapi_free((wasapi*)ctx);
}

}

#endif
