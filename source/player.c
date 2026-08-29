#include "player.h"
#include "video.h"
#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define WAVEBUFCOUNT  3

static THEORA_Context vidCtx;
static TH3DS_Frame frame;
static Thread vthread = NULL;
static size_t buffSize = 8 * 4096;
static ndspWaveBuf waveBuf[WAVEBUFCOUNT];
static int16_t* audioBuffer = NULL;
static LightEvent soundEvent;
static float scaleframe = 1.0f;
static volatile bool isplaying = false;
static bool loop_current_video = false;
static bool frame_initialized = false;
static bool is_paused = false;

static inline float getFrameScalef(float wi, float hi, float targetw, float targeth) {
    float w = targetw / wi;
    float h = targeth / hi;
    return fabs(w) > fabs(h) ? h : w;
}

static void audioInit(THEORA_audioinfo* ainfo) {
    ndspChnReset(0);
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, ainfo->channels == 2 ? NDSP_INTERP_POLYPHASE : NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, ainfo->rate);
    ndspChnSetFormat(0, ainfo->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

    if (!audioBuffer) {
        audioBuffer = linearAlloc((buffSize * sizeof(int16_t)) * WAVEBUFCOUNT);
    }

    memset(waveBuf, 0, sizeof(waveBuf));
    for (unsigned i = 0; i < WAVEBUFCOUNT; ++i) {
        waveBuf[i].data_vaddr = &audioBuffer[i * buffSize];
        waveBuf[i].nsamples = buffSize;
        waveBuf[i].status = NDSP_WBUF_DONE;
    }
}

static void audioClose(void) {
    ndspChnReset(0);
    if (audioBuffer) {
        linearFree(audioBuffer);
        audioBuffer = NULL;
    }
}

static void videoDecode_thread(void* nul) {
    THEORA_videoinfo* vinfo = THEORA_vidinfo(&vidCtx);
    THEORA_audioinfo* ainfo = THEORA_audinfo(&vidCtx);

    if (THEORA_HasAudio(&vidCtx)) audioInit(ainfo);
    if (THEORA_HasVideo(&vidCtx)) {
        frameInit(&frame, vinfo);
        frame_initialized = true;
        scaleframe = getFrameScalef(vinfo->width, vinfo->height, SCREEN_WIDTH, SCREEN_HEIGHT);
    }

    isplaying = true;

    while (isplaying) {
        if (is_paused) {
            svcSleepThread(10 * 1000 * 1000);
            continue;
        }

        if (THEORA_eos(&vidCtx)) {
            if (loop_current_video) {
                THEORA_reset(&vidCtx);
                continue;
            } else {
                break;
            }
        }

        if (THEORA_HasVideo(&vidCtx)) {
            th_ycbcr_buffer ybr;
            if (THEORA_getvideo(&vidCtx, ybr)) {
                frameWrite(&frame, vinfo, ybr);
            }
        }

        if (THEORA_HasAudio(&vidCtx)) {
            for (int cur_wvbuf = 0; cur_wvbuf < WAVEBUFCOUNT; cur_wvbuf++) {
                ndspWaveBuf *buf = &waveBuf[cur_wvbuf];
                if (buf->status == NDSP_WBUF_DONE) {
                    size_t read = THEORA_readaudio(&vidCtx, (char *)buf->data_pcm16, buffSize);
                    if (read <= 0) break;
                    else if (read <= buffSize) buf->nsamples = read / ainfo->channels;
                    ndspChnWaveBufAdd(0, buf);
                }
                DSP_FlushDataCache(buf->data_pcm16, buffSize * sizeof(int16_t));
            }
        }
    }

    isplaying = false;
    threadExit(0);
}

static void audioCallback(void *const arg_) {
    (void)arg_;
    if (!isplaying) return;
    LightEvent_Signal(&soundEvent);
}

bool player_init(void) {
    ndspSetCallback(audioCallback, NULL);
    return true;
}

void player_cleanup(void) {
    player_stop_stream();
    audioClose();
}

void player_stop_stream(void) {
    isplaying = false;
    if (vthread) {
        threadJoin(vthread, U64_MAX);
        threadFree(vthread);
        vthread = NULL;
    }

    THEORA_Close(&vidCtx);
    ndspChnReset(0);

    if (frame_initialized) {
        frameDelete(&frame);
        frame_initialized = false;
    }
}

bool player_start_stream(const char* filename, bool loop) {
    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "sdmc:/assets/video/%s", filename);

    player_stop_stream();

    FILE* fp = fopen(fullpath, "r");
    if (!fp) return false;
    fclose(fp);

    int ret = THEORA_Create(&vidCtx, fullpath);
    if (ret != 0) return false;

    is_paused = false;
    loop_current_video = loop;
    s32 prio;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    vthread = threadCreate(videoDecode_thread, NULL, 32 * 1024, prio - 1, -1, false);
    return true;
}

void player_pause(bool pause) {
    is_paused = pause;
}

bool player_is_playing(void) {
    return isplaying;
}

void player_draw_frame(float center_x, float center_y) {
    if (frame_initialized) {
        frameDrawAtCentered(&frame, center_x, center_y, 0.5f, scaleframe, scaleframe);
    }
}
