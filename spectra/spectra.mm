// CoreAudio voice mixer for apple targets. spectra's AudioMixer resampled and
// mixed every voice by hand on its own pthread; this hands both to AVAudioEngine
// so the sample-rate conversion, the pitch shift and the mix all run in Apple's
// own DSP path on the audio render thread instead of a scalar loop of ours.
//
// pitch is AVAudioUnitVarispeed, which resamples -- speed and pitch move
// together, exactly like the loop it replaces, so callers hear no change.
//
// pure ObjC++: no Au headers. spectra.ag talks to the plain C surface at the
// bottom of this file.
#include <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#define SP_VOICES 16

// one pool slot: a player and its pitch unit, wired to the mixer once at open
// silver builds .mm WITHOUT arc, so every alloc here is balanced by hand
@interface SpVoice : NSObject
@property (retain) AVAudioPlayerNode*    player;
@property (retain) AVAudioUnitVarispeed* rate;
@property (assign) const void*           key;      // owning clip's pcm pointer
@property (assign) bool                  busy;
// bumped every time the slot is handed out, so a handle held past the end of
// its sound cannot steer whatever took the slot next
@property (assign) int                   gen;
@end
@implementation SpVoice
- (void)dealloc {
    [_player release];
    [_rate release];
    [super dealloc];
}
@end

static AVAudioEngine*      g_engine;
static NSMutableArray*     g_voices;   // SpVoice*
static NSMutableDictionary* g_buffers; // clip key -> AVAudioPCMBuffer*
static AVAudioFormat*      g_fmt;      // the one format every player is wired with

// the engine mixes in ONE format; a clip is converted to it once, on first play,
// by AVAudioConverter. doing it here means the per-sample rate conversion never
// happens again -- varispeed only has to handle the pitch offset from there.
static AVAudioPCMBuffer* sp_buffer(const short* pcm, long frames, int rate, int channels) {
    if (!pcm || frames < 2 || channels < 1) return nil;
    NSValue* k = [NSValue valueWithPointer:pcm];
    AVAudioPCMBuffer* cached = g_buffers[k];
    if (cached) return cached;

    AVAudioFormat* src = [[[AVAudioFormat alloc]
        initWithCommonFormat:AVAudioPCMFormatFloat32
                  sampleRate:(double)rate
                    channels:(AVAudioChannelCount)channels
                 interleaved:NO] autorelease];
    if (!src) return nil;
    AVAudioPCMBuffer* in = [[[AVAudioPCMBuffer alloc] initWithPCMFormat:src
                                                 frameCapacity:(AVAudioFrameCount)frames] autorelease];
    if (!in) return nil;
    in.frameLength = (AVAudioFrameCount)frames;
    // interleaved S16 -> planar float, the format every apple audio api wants
    float* const* out = in.floatChannelData;
    for (long i = 0; i < frames; i++)
        for (int c = 0; c < channels; c++)
            out[c][i] = (float)pcm[i * channels + c] / 32768.0f;

    if ([src isEqual:g_fmt]) { g_buffers[k] = in; return in; }

    AVAudioConverter* conv = [[[AVAudioConverter alloc]
        initFromFormat:src toFormat:g_fmt] autorelease];
    if (!conv) return nil;
    // rate conversion changes the frame count; leave room and let it report
    double  ratio = g_fmt.sampleRate / (double)rate;
    AVAudioFrameCount cap = (AVAudioFrameCount)(frames * ratio) + 4096;
    AVAudioPCMBuffer* dst = [[[AVAudioPCMBuffer alloc]
        initWithPCMFormat:g_fmt frameCapacity:cap] autorelease];
    if (!dst) return nil;
    __block bool fed = false;
    NSError* err = nil;
    [conv convertToBuffer:dst error:&err withInputFromBlock:
        ^AVAudioBuffer*(AVAudioPacketCount need, AVAudioConverterInputStatus* status) {
            if (fed) { *status = AVAudioConverterInputStatus_EndOfStream; return nil; }
            fed = true;
            *status = AVAudioConverterInputStatus_HaveData;
            return in;
        }];
    if (err) return nil;
    g_buffers[k] = dst;
    return dst;
}

static SpVoice* sp_free_voice(void) {
    for (SpVoice* v in g_voices)
        if (!v.busy) return v;
    // every slot is speaking: steal the first, same as the old pool's fallback
    SpVoice* v = g_voices.firstObject;
    [v.player stop];
    v.busy = false;
    return v;
}

extern "C" {

// bring the engine up. returns false if audio is unavailable, which leaves the
// caller free to fall back exactly as it did when AudioQueue failed to open.
bool sp_av_open(int sample_rate) {
    if (g_engine) return true;
#if TARGET_OS_IPHONE
    // ios routes nothing until a session is set and made active -- the reason a
    // device stays silent while the simulator and the mac play fine
    AVAudioSession* s = [AVAudioSession sharedInstance];
    NSError* serr = nil;
    [s setCategory:AVAudioSessionCategoryPlayback error:&serr];
    if (serr) fprintf(stderr, "spectra: session category: %s\n",
        serr.localizedDescription.UTF8String);
    serr = nil;
    [s setActive:YES error:&serr];
    if (serr) fprintf(stderr, "spectra: session active: %s\n",
        serr.localizedDescription.UTF8String);
#endif
    g_engine  = [[AVAudioEngine alloc] init];
    g_buffers = [[NSMutableDictionary alloc] init];
    g_voices  = [[NSMutableArray alloc] init];
    g_fmt = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                             sampleRate:(double)sample_rate
                                               channels:2
                                            interleaved:NO];
    for (int i = 0; i < SP_VOICES; i++) {
        SpVoice* v = [[[SpVoice alloc] init] autorelease];
        v.player = [[[AVAudioPlayerNode alloc] init] autorelease];
        v.rate   = [[[AVAudioUnitVarispeed alloc] init] autorelease];
        [g_engine attachNode:v.player];
        [g_engine attachNode:v.rate];
        [g_engine connect:v.player to:v.rate format:g_fmt];
        [g_engine connect:v.rate to:g_engine.mainMixerNode format:g_fmt];
        [g_voices addObject:v];
    }
    NSError* err = nil;
    if (![g_engine startAndReturnError:&err]) {
        fprintf(stderr, "spectra: engine start failed: %s\n",
            err.localizedDescription.UTF8String);
        [g_engine release];  g_engine  = nil;
        [g_buffers release]; g_buffers = nil;
        [g_voices release];  g_voices  = nil;
        [g_fmt release];     g_fmt     = nil;
        return false;
    }
    return true;
}

// schedule one clip on a free voice. pitch rides AVAudioUnitVarispeed, whose
// documented range is 0.25..4.0 -- outside it the unit refuses and the voice
// would play at the wrong speed, so clamp rather than pass it through
// returns a handle for steering THIS voice: cars share one engine clip, so a
// clip-keyed setter would move every car's pitch at once. 0 = not playing.
int sp_av_play(const short* pcm, long frames, int rate, int channels,
               bool loop, float gain, float pitch) {
    if (!g_engine) return 0;
    AVAudioPCMBuffer* buf = sp_buffer(pcm, frames, rate, channels);
    if (!buf) return 0;
    SpVoice* v = sp_free_voice();
    if (!v) return 0;
    if (pitch < 0.25f) pitch = 0.25f;
    if (pitch > 4.0f)  pitch = 4.0f;
    v.rate.rate     = pitch;
    v.player.volume = gain;
    v.key  = pcm;
    v.busy = true;
    [v.player stop];
    AVAudioPlayerNodeBufferOptions opt = loop ? AVAudioPlayerNodeBufferLoops
                                              : (AVAudioPlayerNodeBufferOptions)0;
    [v.player scheduleBuffer:buf atTime:nil options:opt completionHandler:^{
        if (!loop) v.busy = false;
    }];
    [v.player play];
    v.gen = (v.gen + 1) & 0x7fff;
    return (((int)[g_voices indexOfObject:v] + 1) & 0xffff) | (v.gen << 16);
}

// decode a handle back to its slot, refusing one whose sound has since ended
// and whose slot was reused
static SpVoice* sp_by_handle(int h) {
    if (h <= 0 || !g_voices) return nil;
    int idx = (h & 0xffff) - 1;
    int gen = (h >> 16) & 0x7fff;
    if (idx < 0 || idx >= (int)g_voices.count) return nil;
    SpVoice* v = g_voices[idx];
    return (v.gen == gen && v.busy) ? v : nil;
}

void sp_av_voice_pitch(int h, float pitch) {
    SpVoice* v = sp_by_handle(h);
    if (!v) return;
    if (pitch < 0.25f) pitch = 0.25f;
    if (pitch > 4.0f)  pitch = 4.0f;
    v.rate.rate = pitch;
}

void sp_av_voice_gain(int h, float gain) {
    SpVoice* v = sp_by_handle(h);
    if (v) v.player.volume = gain;
}

void sp_av_voice_stop(int h) {
    SpVoice* v = sp_by_handle(h);
    if (!v) return;
    [v.player stop];
    v.busy = false;
}

// pitch on a voice that is ALREADY playing -- a looping engine note tracking
// rpm, which scheduling-time pitch cannot express. varispeed takes the change
// live on the render thread, so this is just a parameter write
void sp_av_set_pitch(const short* pcm, float pitch) {
    if (!g_engine) return;
    if (pitch < 0.25f) pitch = 0.25f;
    if (pitch > 4.0f)  pitch = 4.0f;
    for (SpVoice* v in g_voices)
        if (v.busy && v.key == pcm) v.rate.rate = pitch;
}

// gain and stop address the CLIP, not one voice: spectra's set_gain/stop_clip
// have always meant "every voice playing this sound"
void sp_av_set_gain(const short* pcm, float gain) {
    if (!g_engine) return;
    for (SpVoice* v in g_voices)
        if (v.busy && v.key == pcm) v.player.volume = gain;
}

void sp_av_stop(const short* pcm) {
    if (!g_engine) return;
    for (SpVoice* v in g_voices)
        if (v.key == pcm) { [v.player stop]; v.busy = false; }
}

void sp_av_close(void) {
    if (!g_engine) return;
    for (SpVoice* v in g_voices) [v.player stop];
    [g_engine stop];
    [g_engine release];  g_engine  = nil;
    [g_voices release];  g_voices  = nil;
    [g_buffers release]; g_buffers = nil;
    [g_fmt release];     g_fmt     = nil;
}

} // extern "C"
