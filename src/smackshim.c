// SPDX-FileCopyrightText: 2026 Eike Hein
// SPDX-License-Identifier: MIT

#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/cpu.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

#define SMACKFILEHANDLE 0x01000UL

#define SMACKBUFFERREVERSED 0x00000001UL
#define SMACKBUFFER32 0x40000000UL
#define SMACKBUFFER555 0x80000000UL
#define SMACKBUFFER565 0xc0000000UL
#define SMACKBUFFER16 (SMACKBUFFER555 | SMACKBUFFER565)

#define SMACKSURFACESLOW 1

typedef struct SmackTag {
    u32 Version;
    u32 Width;
    u32 Height;
    u32 Frames;
    u32 MSPerFrame;
    u32 SmackerType;
    u32 LargestInTrack[7];
    u32 tablesize;
    u32 codesize;
    u32 absize;
    u32 detailsize;
    u32 typesize;
    u32 TrackType[7];
    u32 extra;
    u32 NewPalette;
    u8 Palette[772];
    u32 PalType;
    u32 FrameNum;
    u32 FrameSize;
    u32 SndSize;
    s32 LastRectx;
    s32 LastRecty;
    s32 LastRectw;
    s32 LastRecth;
    u32 OpenFlags;
    u32 LeftOfs;
    u32 TopOfs;
    u32 LargestFrameSize;
    u32 Highest1SecRate;
    u32 Highest1SecFrame;
    u32 ReadError;
    u32 addr32;
} Smack;

typedef struct SmackSumTag {
    u32 TotalTime;
    u32 MS100PerFrame;
    u32 TotalOpenTime;
    u32 TotalFrames;
    u32 SkippedFrames;
    u32 SoundSkips;
    u32 TotalBlitTime;
    u32 TotalReadTime;
    u32 TotalDecompTime;
    u32 TotalBackReadTime;
    u32 TotalReadSpeed;
    u32 SlowestFrameTime;
    u32 Slowest2FrameTime;
    u32 SlowestFrameNum;
    u32 Slowest2FrameNum;
    u32 AverageFrameSize;
    u32 HighestMemAmount;
    u32 TotalExtraMemory;
    u32 HighestExtraUsed;
} SmackSum;

typedef struct _SMACKBLIT {
    u32 Flags;
    u8 *Palette;
    u32 PalType;
    u16 *SmoothTable;
    u16 *Conv8to16Table;
    u32 whichmode;
    u32 palindex;
    u32 t16index;
    u32 smoothindex;
    u32 smoothtype;
    u32 firstpalette;
} SMACKBLIT;

typedef SMACKBLIT *HSMACKBLIT;

typedef struct SmackBufTag {
    u32 Reversed;
    u32 SurfaceType;
    u32 BlitType;
    u32 FullScreen;
    u32 Width;
    u32 Height;
    u32 Pitch;
    u32 Zoomed;
    u32 ZWidth;
    u32 ZHeight;
    u32 DispColors;
    u32 MaxPalColors;
    u32 PalColorsInUse;
    u32 StartPalColor;
    u32 EndPalColor;
    RGBQUAD Palette[256];
    u32 PalType;
    u32 forceredraw;
    u32 didapalette;
    void *Buffer;
    void *DIBRestore;
    u32 OurBitmap;
    u32 OrigBitmap;
    u32 OurPalette;
    u32 WinGDC;
    u32 FullFocused;
    u32 ParentHwnd;
    u32 OldParWndProc;
    u32 OldDispWndProc;
    u32 DispHwnd;
    u32 WinGBufHandle;
    void *lpDD;
    void *lpDDSP;
    u32 DDSurfaceType;
    HSMACKBLIT DDblit;
    s32 ddSoftwarecur;
    s32 didaddblit;
    s32 lastwasdd;
    RECT ddscreen;
    s32 manyblits;
    s32 *blitrects;
    s32 *rectsptr;
    s32 maxrects;
    s32 numrects;
    HDC lastdc;
} SmackBuf;

typedef struct FileIo {
    HANDLE handle;
    bool close_on_free;
    uint8_t *buffer;
    AVIOContext *avio;
} FileIo;

typedef struct AudioState {
    int stream_index;
    AVCodecContext *codec;
    AVFrame *frame;
    struct SwrContext *swr;
    AVChannelLayout input_layout;
    AVChannelLayout output_layout;
    int input_rate;
    int output_rate;
    uint8_t *pcm;
    size_t pcm_size;
    size_t pcm_capacity;
    WAVEFORMATEX format;
    LPDIRECTSOUNDBUFFER buffer;
    DWORD buffer_bytes;
    bool playing;
} AudioState;

typedef struct ShimSmack {
    Smack smk;
    FileIo io;
    AVFormatContext *format;
    AVCodecContext *video_codec;
    AVFrame *video_frame;
    AVPacket *packet;
    AVPacket *pending_video_packet;
    struct SwsContext *sws;
    int video_stream;
    bool has_pending_video_packet;
    bool demux_eof;
    bool video_sent_flush;
    bool decode_eof;

    uint8_t *scaled_pixels;
    int scaled_width;
    int scaled_height;
    enum AVPixelFormat scaled_format;
    size_t scaled_capacity;

    void *dest;
    s32 dest_left;
    s32 dest_top;
    u32 dest_pitch;
    u32 dest_height;
    u32 dest_bytes_per_pixel;
    bool dest_rgb565;
    bool dest_reversed;

    bool pending_rect;
    bool clock_started;
    u32 clock_start;
    u32 opened_at;
    u32 decoded_frames;
    u32 audio_starts_at_ms;
    bool audio_enabled;
    AudioState audio;
} ShimSmack;

static LPDIRECTSOUND g_direct_sound;
static u32 g_forced_frame_rate;
static bool g_ffmpeg_initialized;
static bool g_trespasser_ce_patch_attempted;

void __stdcall SmackClose(Smack *smk);

static ShimSmack *shim_from_smack(Smack *smk)
{
    return (ShimSmack *)smk;
}

static bool is_trespasser_ce_process(void)
{
    char path[MAX_PATH];
    const DWORD length = GetModuleFileNameA(NULL, path, sizeof(path));
    if (length == 0 || length >= sizeof(path)) {
        return false;
    }

    const char *name = path;
    for (const char *p = path; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            name = p + 1;
        }
    }
    return lstrcmpiA(name, "tpassp6.exe") == 0;
}

#define TRESPASSER_CE_IMAGE_BASE 0x00400000UL

typedef struct RuntimePatch {
    uintptr_t address;
    const u8 *expected;
    const u8 *replacement;
    size_t size;
} RuntimePatch;

static bool patch_range_is_committed(const void *address, size_t size)
{
    MEMORY_BASIC_INFORMATION info;
    if (size == 0 || !VirtualQuery(address, &info, sizeof(info))) {
        return false;
    }
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const uintptr_t region_base = (uintptr_t)info.BaseAddress;
    const uintptr_t range_base = (uintptr_t)address;
    if (range_base < region_base) {
        return false;
    }
    const uintptr_t range_offset = range_base - region_base;
    if (range_offset > info.RegionSize) {
        return false;
    }
    return size <= (size_t)(info.RegionSize - range_offset);
}

static void apply_runtime_patch(uintptr_t image_base, const RuntimePatch *patch)
{
    u8 *address = (u8 *)(image_base + patch->address - TRESPASSER_CE_IMAGE_BASE);
    if (!patch_range_is_committed(address, patch->size)) {
        return;
    }
    if (memcmp(address, patch->replacement, patch->size) == 0) {
        return;
    }
    if (memcmp(address, patch->expected, patch->size) != 0) {
        return;
    }

    DWORD old_protect = 0;
    if (!VirtualProtect(address, patch->size, PAGE_EXECUTE_READWRITE, &old_protect)) {
        return;
    }

    memcpy(address, patch->replacement, patch->size);
    FlushInstructionCache(GetCurrentProcess(), address, patch->size);

    DWORD ignored = 0;
    VirtualProtect(address, patch->size, old_protect, &ignored);
}

static void apply_trespasser_ce_video_patch(void)
{
    if (g_trespasser_ce_patch_attempted) {
        return;
    }
    g_trespasser_ce_patch_attempted = true;

    if (!is_trespasser_ce_process()) {
        return;
    }

    const HMODULE module = GetModuleHandleA(NULL);
    if (!module) {
        return;
    }

    static const u8 set_smack_565[] = { 0xc7, 0x85, 0x54, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0 };
    static const u8 set_smack_555[] = { 0xc7, 0x85, 0x54, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 };
    static const u8 set_smack_32[] = { 0xc7, 0x85, 0x54, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40 };
    static const u8 set_raster_bits_16[] = { 0xbb, 0x10, 0x00, 0x00, 0x00 };
    static const u8 set_raster_bits_32[] = { 0xbb, 0x20, 0x00, 0x00, 0x00 };
    static const RuntimePatch patches[] = {
        { 0x0042f3eb, set_smack_565, set_smack_32, sizeof(set_smack_32) },
        { 0x0042f3f7, set_smack_555, set_smack_32, sizeof(set_smack_32) },
        { 0x0042f56e, set_smack_565, set_smack_32, sizeof(set_smack_32) },
        { 0x0042f501, set_raster_bits_16, set_raster_bits_32, sizeof(set_raster_bits_32) },
    };

    for (size_t i = 0; i < sizeof(patches) / sizeof(patches[0]); ++i) {
        apply_runtime_patch((uintptr_t)module, &patches[i]);
    }
}

static void initialize_ffmpeg(void)
{
    if (g_ffmpeg_initialized) {
        return;
    }

    av_log_set_level(AV_LOG_QUIET);
    const int detected_flags = av_get_cpu_flags();
    const int realigned_x86_flags =
        detected_flags & (AV_CPU_FLAG_CMOV | AV_CPU_FLAG_MMX | AV_CPU_FLAG_MMXEXT | AV_CPU_FLAG_SSE |
                          AV_CPU_FLAG_SSE2 | AV_CPU_FLAG_SSE2SLOW | AV_CPU_FLAG_SSE3 | AV_CPU_FLAG_SSE3SLOW);
    av_force_cpu_flags(realigned_x86_flags | AV_CPU_FLAG_FORCE);
    g_ffmpeg_initialized = true;
}

static int64_t set_file_pointer64(HANDLE handle, int64_t offset, DWORD method)
{
    LARGE_INTEGER li;
    li.QuadPart = offset;
    li.LowPart = SetFilePointer(handle, li.LowPart, &li.HighPart, method);
    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        return -1;
    }
    return li.QuadPart;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    FileIo *io = (FileIo *)opaque;
    DWORD read_bytes = 0;
    if (!ReadFile(io->handle, buf, (DWORD)buf_size, &read_bytes, NULL)) {
        return AVERROR(EIO);
    }
    if (read_bytes == 0) {
        return AVERROR_EOF;
    }
    return (int)read_bytes;
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
    FileIo *io = (FileIo *)opaque;
    if (whence == AVSEEK_SIZE) {
        const int64_t current = set_file_pointer64(io->handle, 0, FILE_CURRENT);
        if (current < 0) {
            return AVERROR(EIO);
        }
        const int64_t size = set_file_pointer64(io->handle, 0, FILE_END);
        if (size < 0) {
            return AVERROR(EIO);
        }
        set_file_pointer64(io->handle, current, FILE_BEGIN);
        return size;
    }

    whence &= ~AVSEEK_FORCE;
    DWORD method = FILE_BEGIN;
    if (whence == SEEK_CUR) {
        method = FILE_CURRENT;
    } else if (whence == SEEK_END) {
        method = FILE_END;
    } else if (whence != SEEK_SET) {
        return AVERROR(EINVAL);
    }

    const int64_t result = set_file_pointer64(io->handle, offset, method);
    if (result < 0) {
        return AVERROR(EIO);
    }
    return result;
}

static void close_file_io(FileIo *io)
{
    if (io->avio) {
        av_freep(&io->avio->buffer);
        avio_context_free(&io->avio);
    } else {
        av_freep(&io->buffer);
    }
    if (io->close_on_free && io->handle && io->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(io->handle);
    }
    memset(io, 0, sizeof(*io));
}

static bool open_format_context(ShimSmack *movie, HANDLE handle, bool close_on_free)
{
    movie->io.handle = handle;
    movie->io.close_on_free = close_on_free;
    movie->io.buffer = av_malloc(64 * 1024);
    if (!movie->io.buffer) {
        return false;
    }

    movie->io.avio = avio_alloc_context(movie->io.buffer, 64 * 1024, 0, &movie->io, read_packet, NULL, seek_packet);
    if (!movie->io.avio) {
        close_file_io(&movie->io);
        return false;
    }
    movie->io.buffer = NULL;

    movie->format = avformat_alloc_context();
    if (!movie->format) {
        close_file_io(&movie->io);
        return false;
    }

    movie->format->pb = movie->io.avio;
    movie->format->flags |= AVFMT_FLAG_CUSTOM_IO;
    if (avformat_open_input(&movie->format, NULL, NULL, NULL) < 0) {
        return false;
    }
    if (avformat_find_stream_info(movie->format, NULL) < 0) {
        return false;
    }
    return true;
}

static bool read_and_dispatch_packet(ShimSmack *movie);

static DWORD audio_align_down(const AudioState *audio, uint64_t bytes)
{
    const DWORD block_align = audio->format.nBlockAlign ? audio->format.nBlockAlign : 4;
    return (DWORD)((bytes / block_align) * block_align);
}

static DWORD audio_bytes_for_ms(const AudioState *audio, u32 ms)
{
    if (audio->format.nAvgBytesPerSec == 0) {
        return 0;
    }
    return audio_align_down(audio, ((uint64_t)audio->format.nAvgBytesPerSec * ms) / 1000U);
}

static void audio_release_buffer(AudioState *audio)
{
    if (audio->buffer) {
        IDirectSoundBuffer_Stop(audio->buffer);
        IDirectSoundBuffer_Release(audio->buffer);
    }
    audio->buffer = NULL;
    audio->playing = false;
    audio->buffer_bytes = 0;
}

static void audio_release(AudioState *audio)
{
    audio_release_buffer(audio);
    swr_free(&audio->swr);
    av_frame_free(&audio->frame);
    avcodec_free_context(&audio->codec);
    av_channel_layout_uninit(&audio->input_layout);
    av_channel_layout_uninit(&audio->output_layout);
    free(audio->pcm);
    memset(audio, 0, sizeof(*audio));
    audio->stream_index = -1;
}

static bool audio_append_pcm(AudioState *audio, const uint8_t *data, size_t size)
{
    if (size == 0) {
        return true;
    }
    if (audio->pcm_size > SIZE_MAX - size) {
        return false;
    }
    const size_t needed = audio->pcm_size + size;
    if (needed > audio->pcm_capacity) {
        size_t new_capacity = audio->pcm_capacity ? audio->pcm_capacity * 2 : 1024 * 1024;
        while (new_capacity < needed) {
            if (new_capacity > SIZE_MAX / 2) {
                new_capacity = needed;
                break;
            }
            new_capacity *= 2;
        }
        uint8_t *new_pcm = realloc(audio->pcm, new_capacity);
        if (!new_pcm) {
            return false;
        }
        audio->pcm = new_pcm;
        audio->pcm_capacity = new_capacity;
    }
    memcpy(audio->pcm + audio->pcm_size, data, size);
    audio->pcm_size = needed;
    return true;
}

static void decode_audio_frame(AudioState *audio, AVFrame *frame)
{
    const int channels = 2;
    const int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    const int64_t delay = swr_get_delay(audio->swr, audio->input_rate);
    const int max_samples = (int)av_rescale_rnd(delay + frame->nb_samples, audio->output_rate,
                                                audio->input_rate, AV_ROUND_UP);
    uint8_t *converted = NULL;
    int linesize = 0;
    if (max_samples <= 0 || bytes_per_sample <= 0) {
        return;
    }
    if (av_samples_alloc(&converted, &linesize, channels, max_samples, AV_SAMPLE_FMT_S16, 0) < 0) {
        return;
    }
    const int samples = swr_convert(audio->swr, &converted, max_samples,
                                    (const uint8_t **)frame->extended_data, frame->nb_samples);
    if (samples > 0) {
        const size_t bytes = (size_t)samples * (size_t)channels * (size_t)bytes_per_sample;
        audio_append_pcm(audio, converted, bytes);
    }
    av_freep(&converted);
}

static void decode_audio_packet(AudioState *audio, AVPacket *packet)
{
    if (!audio->codec || !audio->swr || !audio->frame) {
        return;
    }
    if (avcodec_send_packet(audio->codec, packet) < 0) {
        return;
    }
    for (;;) {
        const int ret = avcodec_receive_frame(audio->codec, audio->frame);
        if (ret == AVERROR(EAGAIN)) {
            break;
        }
        if (ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            break;
        }
        decode_audio_frame(audio, audio->frame);
        av_frame_unref(audio->frame);
    }
}

static bool rewind_video_stream(ShimSmack *movie)
{
    AVStream *stream = movie->format->streams[movie->video_stream];
    const int64_t start_time = stream->start_time != AV_NOPTS_VALUE ? stream->start_time : 0;
    if (av_seek_frame(movie->format, movie->video_stream, 0, AVSEEK_FLAG_BACKWARD) < 0 &&
        av_seek_frame(movie->format, -1, start_time, AVSEEK_FLAG_BACKWARD) < 0) {
        return false;
    }
    avformat_flush(movie->format);
    if (movie->video_codec) {
        avcodec_flush_buffers(movie->video_codec);
    }
    if (movie->has_pending_video_packet) {
        av_packet_unref(movie->pending_video_packet);
        movie->has_pending_video_packet = false;
    }
    movie->demux_eof = false;
    movie->video_sent_flush = false;
    movie->decode_eof = false;
    return true;
}

static bool predecode_audio(ShimSmack *movie)
{
    AudioState *audio = &movie->audio;
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        return false;
    }

    while (av_read_frame(movie->format, packet) >= 0) {
        if (packet->stream_index == audio->stream_index) {
            decode_audio_packet(audio, packet);
        }
        av_packet_unref(packet);
    }
    decode_audio_packet(audio, NULL);
    av_packet_free(&packet);

    return audio->pcm_size > 0 && rewind_video_stream(movie);
}

static bool open_audio_decoder(ShimSmack *movie)
{
    AudioState *audio = &movie->audio;
    audio->stream_index = av_find_best_stream(movie->format, AVMEDIA_TYPE_AUDIO, -1, movie->video_stream, NULL, 0);
    if (audio->stream_index < 0) {
        return false;
    }

    AVStream *stream = movie->format->streams[audio->stream_index];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        audio->stream_index = -1;
        return false;
    }

    audio->codec = avcodec_alloc_context3(decoder);
    audio->frame = av_frame_alloc();
    if (!audio->codec || !audio->frame) {
        audio_release(audio);
        return false;
    }
    if (avcodec_parameters_to_context(audio->codec, stream->codecpar) < 0) {
        audio_release(audio);
        return false;
    }
    audio->codec->thread_count = 1;
    if (avcodec_open2(audio->codec, decoder, NULL) < 0) {
        audio_release(audio);
        return false;
    }

    if (audio->codec->ch_layout.nb_channels > 0) {
        if (av_channel_layout_copy(&audio->input_layout, &audio->codec->ch_layout) < 0) {
            audio_release(audio);
            return false;
        }
    } else {
        av_channel_layout_default(&audio->input_layout, 2);
    }
    av_channel_layout_default(&audio->output_layout, 2);

    audio->input_rate = audio->codec->sample_rate > 0 ? audio->codec->sample_rate : 44100;
    audio->output_rate = audio->input_rate;
    if (swr_alloc_set_opts2(&audio->swr, &audio->output_layout, AV_SAMPLE_FMT_S16, audio->output_rate,
                            &audio->input_layout, audio->codec->sample_fmt, audio->input_rate, 0, NULL) < 0) {
        audio_release(audio);
        return false;
    }
    if (!audio->swr || swr_init(audio->swr) < 0) {
        audio_release(audio);
        return false;
    }

    memset(&audio->format, 0, sizeof(audio->format));
    audio->format.wFormatTag = WAVE_FORMAT_PCM;
    audio->format.nChannels = 2;
    audio->format.nSamplesPerSec = (DWORD)audio->output_rate;
    audio->format.wBitsPerSample = 16;
    audio->format.nBlockAlign = (WORD)(audio->format.nChannels * audio->format.wBitsPerSample / 8);
    audio->format.nAvgBytesPerSec = audio->format.nSamplesPerSec * audio->format.nBlockAlign;
    if (!predecode_audio(movie)) {
        audio_release(audio);
        return false;
    }
    return true;
}

static void audio_prepare(ShimSmack *movie)
{
    AudioState *audio = &movie->audio;
    if (audio->buffer || !audio->pcm || audio->pcm_size == 0 || !g_direct_sound) {
        return;
    }
    if (audio->pcm_size > UINT32_MAX) {
        return;
    }

    const DWORD buffer_bytes = audio_align_down(audio, audio->pcm_size);
    if (buffer_bytes == 0) {
        return;
    }
    DSBUFFERDESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = buffer_bytes;
    desc.lpwfxFormat = &audio->format;

    if (FAILED(IDirectSound_CreateSoundBuffer(g_direct_sound, &desc, &audio->buffer, NULL))) {
        return;
    }

    audio->buffer_bytes = buffer_bytes;
    void *part1 = NULL;
    void *part2 = NULL;
    DWORD part1_size = 0;
    DWORD part2_size = 0;
    if (SUCCEEDED(IDirectSoundBuffer_Lock(audio->buffer, 0, audio->buffer_bytes,
                                          &part1, &part1_size, &part2, &part2_size, 0))) {
        if (part1 && part1_size) {
            memcpy(part1, audio->pcm, part1_size);
        }
        if (part2 && part2_size) {
            memcpy(part2, audio->pcm + part1_size, part2_size);
        }
        IDirectSoundBuffer_Unlock(audio->buffer, part1, part1_size, part2, part2_size);
    }
    if (!part1 && !part2) {
        audio_release_buffer(audio);
    }
}

static DWORD audio_offset_for_ms(const AudioState *audio, u32 ms)
{
    if (audio->format.nAvgBytesPerSec == 0 || audio->buffer_bytes == 0) {
        return 0;
    }
    DWORD offset = audio_bytes_for_ms(audio, ms);
    if (offset >= audio->buffer_bytes) {
        offset = audio->buffer_bytes - audio->format.nBlockAlign;
    }
    return offset;
}

static void audio_start(ShimSmack *movie)
{
    AudioState *audio = &movie->audio;
    if (!movie->audio_enabled || !audio->pcm) {
        return;
    }
    audio_prepare(movie);
    if (!audio->buffer) {
        return;
    }
    if (audio->playing) {
        IDirectSoundBuffer_Stop(audio->buffer);
        audio->playing = false;
    }
    IDirectSoundBuffer_SetCurrentPosition(audio->buffer, audio_offset_for_ms(audio, movie->audio_starts_at_ms));
    if (SUCCEEDED(IDirectSoundBuffer_Play(audio->buffer, 0, 0, 0))) {
        audio->playing = true;
    }
}

static void audio_stop(AudioState *audio)
{
    if (audio->buffer) {
        IDirectSoundBuffer_Stop(audio->buffer);
    }
    audio->playing = false;
}

static u32 now_ms(void)
{
    return timeGetTime();
}

static void ensure_clock_started(ShimSmack *movie)
{
    if (movie->clock_started) {
        return;
    }
    movie->clock_start = now_ms() - movie->audio_starts_at_ms;
    movie->clock_started = true;
    audio_start(movie);
}

static u32 playback_clock_ms(ShimSmack *movie)
{
    return now_ms() - movie->clock_start;
}

static u32 frame_ms(const ShimSmack *movie, u32 frame)
{
    const u32 ms_per_frame = movie->smk.MSPerFrame ? movie->smk.MSPerFrame : 33;
    return frame * ms_per_frame;
}

static bool ensure_scaled_buffer(ShimSmack *movie, int width, int height,
                                 enum AVPixelFormat format, u32 bytes_per_pixel)
{
    if (width <= 0 || height <= 0 || bytes_per_pixel == 0) {
        return false;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return false;
    }
    const size_t pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / bytes_per_pixel) {
        return false;
    }
    const size_t needed = pixels * bytes_per_pixel;

    if (movie->scaled_pixels && movie->scaled_capacity >= needed &&
        movie->scaled_width == width && movie->scaled_height == height && movie->scaled_format == format) {
        return true;
    }
    uint8_t *new_pixels = av_realloc(movie->scaled_pixels, needed);
    if (!new_pixels) {
        return false;
    }
    movie->scaled_pixels = new_pixels;
    movie->scaled_capacity = needed;
    movie->scaled_width = width;
    movie->scaled_height = height;
    movie->scaled_format = format;
    return true;
}

static void render_frame(ShimSmack *movie, const AVFrame *frame)
{
    const u32 bytes_per_pixel = movie->dest_bytes_per_pixel ? movie->dest_bytes_per_pixel : 2;
    if (!movie->dest || movie->dest_pitch < bytes_per_pixel || movie->dest_height == 0) {
        return;
    }

    const int source_width = frame->width;
    const int source_height = frame->height;
    const int dest_width = (int)(movie->dest_pitch / bytes_per_pixel);
    const int dest_height = (int)movie->dest_height;
    if (source_width <= 0 || source_height <= 0 || dest_width <= 0 || dest_height <= 0) {
        return;
    }

    int output_width = source_width;
    int output_height = source_height;
    bool scale_to_fit = false;
    if (source_width > dest_width || source_height > dest_height) {
        const double sx = (double)dest_width / (double)source_width;
        const double sy = (double)dest_height / (double)source_height;
        const double scale = sx < sy ? sx : sy;
        output_width = (int)((double)source_width * scale);
        output_height = (int)((double)source_height * scale);
        if (output_width < 1) {
            output_width = 1;
        }
        if (output_height < 1) {
            output_height = 1;
        }
        scale_to_fit = true;
    }

    const enum AVPixelFormat out_format = bytes_per_pixel == 4
        ? AV_PIX_FMT_BGRA
        : (movie->dest_rgb565 ? AV_PIX_FMT_RGB565LE : AV_PIX_FMT_RGB555LE);
    if (!ensure_scaled_buffer(movie, output_width, output_height, out_format, bytes_per_pixel)) {
        return;
    }

    movie->sws = sws_getCachedContext(movie->sws, source_width, source_height, (enum AVPixelFormat)frame->format,
                                      output_width, output_height, out_format, SWS_BILINEAR, NULL, NULL, NULL);
    if (!movie->sws) {
        return;
    }

    uint8_t *dst_slices[4] = { movie->scaled_pixels, NULL, NULL, NULL };
    int dst_stride[4] = { output_width * (int)bytes_per_pixel, 0, 0, 0 };
    if (sws_scale(movie->sws, (const uint8_t *const *)frame->data, frame->linesize, 0,
                  source_height, dst_slices, dst_stride) <= 0) {
        return;
    }

    int dest_x = scale_to_fit ? (dest_width - output_width) / 2 : movie->dest_left;
    int dest_y = scale_to_fit ? (dest_height - output_height) / 2 : movie->dest_top;
    int src_x = 0;
    int src_y = 0;
    int copy_width = output_width;
    int copy_height = output_height;

    if (dest_x < 0) {
        src_x = -dest_x;
        copy_width -= src_x;
        dest_x = 0;
    }
    if (dest_y < 0) {
        src_y = -dest_y;
        copy_height -= src_y;
        dest_y = 0;
    }
    if (dest_x + copy_width > dest_width) {
        copy_width = dest_width - dest_x;
    }
    if (dest_y + copy_height > dest_height) {
        copy_height = dest_height - dest_y;
    }
    if (copy_width <= 0 || copy_height <= 0) {
        return;
    }

    uint8_t *dest = (uint8_t *)movie->dest;
    const uint8_t *src = movie->scaled_pixels + (size_t)(src_y * output_width + src_x) * bytes_per_pixel;
    const size_t src_stride = (size_t)output_width * bytes_per_pixel;
    const size_t row_bytes = (size_t)copy_width * bytes_per_pixel;
    for (int y = 0; y < copy_height; ++y) {
        int row = dest_y + y;
        if (movie->dest_reversed) {
            row = dest_height - 1 - row;
        }
        memcpy(dest + (size_t)row * movie->dest_pitch + (size_t)dest_x * bytes_per_pixel,
               src + (size_t)y * src_stride, row_bytes);
    }

    movie->smk.LastRectx = dest_x;
    movie->smk.LastRecty = dest_y;
    movie->smk.LastRectw = copy_width;
    movie->smk.LastRecth = copy_height;
    movie->pending_rect = true;
}

static bool send_pending_video_packet(ShimSmack *movie)
{
    if (!movie->has_pending_video_packet) {
        return true;
    }

    const int ret = avcodec_send_packet(movie->video_codec, movie->pending_video_packet);
    if (ret == AVERROR(EAGAIN)) {
        return false;
    }
    av_packet_unref(movie->pending_video_packet);
    movie->has_pending_video_packet = false;
    return ret >= 0;
}

static bool dispatch_video_packet(ShimSmack *movie, AVPacket *packet)
{
    const int ret = avcodec_send_packet(movie->video_codec, packet);
    if (ret == AVERROR(EAGAIN)) {
        if (movie->pending_video_packet && !movie->has_pending_video_packet) {
            av_packet_move_ref(movie->pending_video_packet, packet);
            movie->has_pending_video_packet = true;
        } else {
            av_packet_unref(packet);
        }
        return false;
    }
    av_packet_unref(packet);
    return ret >= 0;
}

static bool read_and_dispatch_packet(ShimSmack *movie)
{
    if (movie->demux_eof || movie->has_pending_video_packet) {
        return false;
    }

    const int read_ret = av_read_frame(movie->format, movie->packet);
    if (read_ret < 0) {
        movie->demux_eof = true;
        return false;
    }

    if (movie->packet->stream_index == movie->video_stream) {
        return dispatch_video_packet(movie, movie->packet);
    }
    av_packet_unref(movie->packet);
    return true;
}

static bool receive_video_frame(ShimSmack *movie)
{
    for (;;) {
        const int receive_ret = avcodec_receive_frame(movie->video_codec, movie->video_frame);
        if (receive_ret == 0) {
            return true;
        }
        if (receive_ret == AVERROR_EOF) {
            movie->decode_eof = true;
            return false;
        }
        if (receive_ret != AVERROR(EAGAIN)) {
            movie->decode_eof = true;
            return false;
        }

        if (!send_pending_video_packet(movie)) {
            continue;
        }

        if (movie->demux_eof) {
            if (!movie->video_sent_flush) {
                avcodec_send_packet(movie->video_codec, NULL);
                movie->video_sent_flush = true;
                continue;
            }
            movie->decode_eof = true;
            return false;
        }

        read_and_dispatch_packet(movie);
    }
}

static void populate_timing_and_header(ShimSmack *movie, u32 flags)
{
    Smack *smk = &movie->smk;
    AVStream *stream = movie->format->streams[movie->video_stream];
    memset(smk, 0, sizeof(*smk));

    smk->Version = 2;
    smk->Width = (u32)movie->video_codec->width;
    smk->Height = (u32)movie->video_codec->height;
    smk->OpenFlags = flags;
    smk->PalType = 0;
    smk->TrackType[0] = movie->audio.codec ? 0x70 : 0;

    AVRational fps = av_guess_frame_rate(movie->format, stream, NULL);
    if (g_forced_frame_rate) {
        smk->MSPerFrame = g_forced_frame_rate;
    } else if (fps.num > 0 && fps.den > 0) {
        const double ms = 1000.0 * (double)fps.den / (double)fps.num;
        smk->MSPerFrame = (u32)(ms + 0.5);
        if (smk->MSPerFrame == 0) {
            smk->MSPerFrame = 1;
        }
    } else {
        smk->MSPerFrame = 33;
    }

    int64_t frames = stream->nb_frames;
    if (frames <= 0 && stream->duration > 0 && fps.num > 0 && fps.den > 0) {
        const double seconds = (double)stream->duration * av_q2d(stream->time_base);
        frames = (int64_t)(seconds * ((double)fps.num / (double)fps.den) + 0.5);
    }
    if (frames <= 0 && movie->format->duration != AV_NOPTS_VALUE && fps.num > 0 && fps.den > 0) {
        const double seconds = (double)movie->format->duration / (double)AV_TIME_BASE;
        frames = (int64_t)(seconds * ((double)fps.num / (double)fps.den) + 0.5);
    }
    if (frames <= 0 || frames > INT_MAX) {
        frames = INT_MAX;
    }
    smk->Frames = (u32)frames;
}

Smack *__stdcall SmackOpen(const char *name, u32 flags, u32 extrabuf)
{
    apply_trespasser_ce_video_patch();
    initialize_ffmpeg();

    ShimSmack *movie = calloc(1, sizeof(*movie));
    if (!movie) {
        return NULL;
    }
    movie->video_stream = -1;
    movie->audio.stream_index = -1;
    movie->opened_at = now_ms();
    movie->dest_bytes_per_pixel = 2;
    movie->dest_rgb565 = true;

    HANDLE handle = INVALID_HANDLE_VALUE;
    bool close_handle = false;
    if (flags & SMACKFILEHANDLE) {
        handle = (HANDLE)(uintptr_t)name;
        set_file_pointer64(handle, 0, FILE_BEGIN);
    } else if (name) {
        handle = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        close_handle = true;
    }

    if (handle == INVALID_HANDLE_VALUE || !open_format_context(movie, handle, close_handle)) {
        SmackClose(&movie->smk);
        return NULL;
    }

    movie->video_stream = av_find_best_stream(movie->format, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (movie->video_stream < 0) {
        SmackClose(&movie->smk);
        return NULL;
    }

    AVStream *stream = movie->format->streams[movie->video_stream];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        SmackClose(&movie->smk);
        return NULL;
    }

    movie->video_codec = avcodec_alloc_context3(decoder);
    movie->video_frame = av_frame_alloc();
    movie->packet = av_packet_alloc();
    movie->pending_video_packet = av_packet_alloc();
    if (!movie->video_codec || !movie->video_frame || !movie->packet || !movie->pending_video_packet) {
        SmackClose(&movie->smk);
        return NULL;
    }
    if (avcodec_parameters_to_context(movie->video_codec, stream->codecpar) < 0) {
        SmackClose(&movie->smk);
        return NULL;
    }
    movie->video_codec->thread_count = 0;
    movie->video_codec->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    if (avcodec_open2(movie->video_codec, decoder, NULL) < 0) {
        SmackClose(&movie->smk);
        return NULL;
    }

    open_audio_decoder(movie);
    populate_timing_and_header(movie, flags);
    return &movie->smk;
}

u32 __stdcall SmackDoFrame(Smack *smk)
{
    if (!smk) {
        return 0;
    }
    ShimSmack *movie = shim_from_smack(smk);
    movie->pending_rect = false;
    smk->LastRectx = 0;
    smk->LastRecty = 0;
    smk->LastRectw = 0;
    smk->LastRecth = 0;

    if (movie->decode_eof || !receive_video_frame(movie)) {
        smk->Frames = smk->FrameNum + 1;
        return 0;
    }

    smk->FrameSize = (u32)av_image_get_buffer_size((enum AVPixelFormat)movie->video_frame->format,
                                                   movie->video_frame->width, movie->video_frame->height, 1);
    render_frame(movie, movie->video_frame);
    av_frame_unref(movie->video_frame);
    ensure_clock_started(movie);
    movie->decoded_frames++;
    return 0;
}

void __stdcall SmackNextFrame(Smack *smk)
{
    if (!smk) {
        return;
    }
    if (smk->FrameNum + 1 < smk->Frames) {
        smk->FrameNum++;
    }
}

u32 __stdcall SmackWait(Smack *smk)
{
    if (!smk) {
        return 1;
    }
    ShimSmack *movie = shim_from_smack(smk);
    if (!movie->clock_started) {
        return 0;
    }
    const u32 due = frame_ms(movie, smk->FrameNum);
    const u32 now = playback_clock_ms(movie);
    const s32 wait = (s32)(due - now);
    if (wait <= 0) {
        return 0;
    }
    Sleep(wait > 5 ? 5 : (DWORD)wait);
    return 1;
}

void __stdcall SmackClose(Smack *smk)
{
    if (!smk) {
        return;
    }
    ShimSmack *movie = shim_from_smack(smk);
    audio_release(&movie->audio);
    sws_freeContext(movie->sws);
    av_freep(&movie->scaled_pixels);
    av_packet_free(&movie->pending_video_packet);
    av_packet_free(&movie->packet);
    av_frame_free(&movie->video_frame);
    avcodec_free_context(&movie->video_codec);
    if (movie->format) {
        AVIOContext *avio = movie->format->pb;
        movie->format->pb = NULL;
        avformat_close_input(&movie->format);
        if (avio) {
            av_freep(&avio->buffer);
            avio_context_free(&avio);
            movie->io.avio = NULL;
        }
    }
    close_file_io(&movie->io);
    free(movie);
}

void __stdcall SmackToBuffer(Smack *smk, u32 left, u32 top, u32 pitch, u32 destheight, const void *buf, u32 flags)
{
    if (!smk) {
        return;
    }
    ShimSmack *movie = shim_from_smack(smk);
    movie->dest = (void *)buf;
    movie->dest_left = (s32)left;
    movie->dest_top = (s32)top;
    movie->dest_pitch = pitch;
    movie->dest_height = destheight;
    movie->dest_reversed = (flags & SMACKBUFFERREVERSED) != 0;
    const u32 buffer_format = flags & SMACKBUFFER16;
    if (buffer_format == SMACKBUFFER32) {
        movie->dest_bytes_per_pixel = 4;
        movie->dest_rgb565 = true;
    } else if (buffer_format == SMACKBUFFER555) {
        movie->dest_bytes_per_pixel = 2;
        movie->dest_rgb565 = false;
    } else if (buffer_format == SMACKBUFFER565) {
        movie->dest_bytes_per_pixel = 2;
        movie->dest_rgb565 = true;
    }
}

u32 __stdcall SmackToBufferRect(Smack *smk, u32 SmackSurface)
{
    if (!smk) {
        return 0;
    }
    ShimSmack *movie = shim_from_smack(smk);
    if (!movie->pending_rect) {
        smk->LastRectw = 0;
        smk->LastRecth = 0;
        return 0;
    }
    movie->pending_rect = false;
    return 1;
}

void __stdcall SmackGoto(Smack *smk, u32 frame)
{
    if (!smk) {
        return;
    }
    ShimSmack *movie = shim_from_smack(smk);
    AVStream *stream = movie->format->streams[movie->video_stream];
    const u32 target_ms = frame_ms(movie, frame);
    const int64_t ts = av_rescale_q((int64_t)target_ms, (AVRational){ 1, 1000 }, stream->time_base);
    if (av_seek_frame(movie->format, movie->video_stream, ts, AVSEEK_FLAG_BACKWARD) >= 0) {
        avformat_flush(movie->format);
        avcodec_flush_buffers(movie->video_codec);
        if (movie->has_pending_video_packet) {
            av_packet_unref(movie->pending_video_packet);
            movie->has_pending_video_packet = false;
        }
        movie->demux_eof = false;
        movie->video_sent_flush = false;
        movie->decode_eof = false;
        audio_stop(&movie->audio);
        smk->FrameNum = frame;
        movie->audio_starts_at_ms = target_ms;
        if (movie->clock_started) {
            movie->clock_start = now_ms() - target_ms;
        }
        if (movie->audio_enabled && movie->clock_started) {
            audio_start(movie);
        }
    }
}

u32 __stdcall SmackSoundOnOff(Smack *smk, u32 on)
{
    if (!smk) {
        return 0;
    }
    ShimSmack *movie = shim_from_smack(smk);
    movie->audio_enabled = on != 0;
    if (movie->audio_enabled) {
        if (movie->clock_started) {
            movie->audio_starts_at_ms = playback_clock_ms(movie);
            audio_start(movie);
        }
    } else {
        audio_stop(&movie->audio);
    }
    return movie->audio_enabled ? 1 : 0;
}

u8 __stdcall SmackSoundUseDirectSound(void *dd)
{
    if (g_direct_sound) {
        IDirectSound_Release(g_direct_sound);
        g_direct_sound = NULL;
    }
    g_direct_sound = (LPDIRECTSOUND)dd;
    if (g_direct_sound) {
        IDirectSound_AddRef(g_direct_sound);
    }
    return 1;
}

void __stdcall SmackSoundSetDirectSoundHWND(HWND hw)
{
    if (g_direct_sound && hw) {
        IDirectSound_SetCooperativeLevel(g_direct_sound, hw, DSSCL_NORMAL);
    }
}

void __stdcall SmackSummary(Smack *smk, SmackSum *sum)
{
    if (!sum) {
        return;
    }
    memset(sum, 0, sizeof(*sum));
    if (!smk) {
        return;
    }
    ShimSmack *movie = shim_from_smack(smk);
    const u32 elapsed = now_ms() - movie->opened_at;
    sum->TotalTime = elapsed ? elapsed : 1;
    sum->MS100PerFrame = smk->MSPerFrame * 100;
    sum->TotalFrames = movie->decoded_frames;
}

void __stdcall SmackFrameRate(u32 forcerate)
{
    g_forced_frame_rate = forcerate;
}

void __stdcall SmackSimulate(u32 sim)
{
}

void __stdcall SmackVolumePan(Smack *smk, u32 trackflag, u32 volume, u32 pan)
{
    if (!smk) {
        return;
    }
    ShimSmack *movie = shim_from_smack(smk);
    if (!movie->audio.buffer) {
        return;
    }
    LONG ds_volume = DSBVOLUME_MAX;
    if (volume < 32768) {
        ds_volume = DSBVOLUME_MIN + (LONG)((volume * (u32)(DSBVOLUME_MAX - DSBVOLUME_MIN)) / 32768U);
    }
    IDirectSoundBuffer_SetVolume(movie->audio.buffer, ds_volume);
    IDirectSoundBuffer_SetPan(movie->audio.buffer, (LONG)pan);
}

u32 __stdcall SmackSoundInTrack(Smack *smk, u32 trackflags)
{
    if (!smk) {
        return 0;
    }
    ShimSmack *movie = shim_from_smack(smk);
    return movie->audio.codec ? 1 : 0;
}

u32 __stdcall SmackGetTrackData(Smack *smk, void *dest, u32 trackflag)
{
    return 0;
}

void __stdcall SmackSoundCheck(void)
{
}

u8 __stdcall SmackSoundUseMSS(void *dd)
{
    return 0;
}

u8 __stdcall SmackSoundUseDW(u32 openfreq, u32 openbits, u32 openchans)
{
    return 0;
}

u8 __stdcall SmackSoundUseWin(void)
{
    return 0;
}

void __stdcall SmackColorRemapWithTrans(Smack *smk, const void *remappal, u32 numcolors, u32 paltype, u32 transindex)
{
}

void __stdcall SmackColorTrans(Smack *smk, const void *trans)
{
}

void __stdcall SmackToScreen(Smack *smk, u32 left, u32 top, u32 BytePS, const u16 *WinTbl, void *SetBank, u32 Flags)
{
}

u32 __stdcall SmackSetSystemRes(u32 mode)
{
    return 0;
}

u32 __stdcall SmackUseMMX(u32 flag)
{
    return 1;
}

s32 __stdcall SmackDDSurfaceType(void *lpDDS)
{
    return SMACKSURFACESLOW;
}

s32 __stdcall SmackIsSoftwareCursor(void *lpDDSP, HCURSOR cur)
{
    return 0;
}

s32 __stdcall SmackCheckCursor(HWND wnd, s32 x, s32 y, s32 w, s32 h)
{
    return 0;
}

void __stdcall SmackRestoreCursor(s32 checkcount)
{
}

SmackBuf *__stdcall SmackBufferOpen(HWND wnd, u32 BlitType, u32 width, u32 height, u32 ZoomW, u32 ZoomH)
{
    SmackBuf *buf = calloc(1, sizeof(*buf));
    if (!buf) {
        return NULL;
    }
    buf->SurfaceType = SMACKSURFACESLOW;
    buf->BlitType = BlitType;
    buf->Width = width;
    buf->Height = height;
    buf->Pitch = width * 2;
    buf->ZWidth = ZoomW;
    buf->ZHeight = ZoomH;
    buf->MaxPalColors = 256;
    buf->PalColorsInUse = 256;
    buf->EndPalColor = 255;
    buf->ParentHwnd = (u32)(uintptr_t)wnd;
    buf->Buffer = calloc((size_t)height, (size_t)buf->Pitch);
    if (!buf->Buffer) {
        free(buf);
        return NULL;
    }
    return buf;
}

u32 __stdcall SmackBufferBlit(SmackBuf *sbuf, HDC dc, s32 hwndx, s32 hwndy, s32 subx, s32 suby, s32 subw, s32 subh)
{
    if (!sbuf || !sbuf->Buffer || !dc || subw <= 0 || subh <= 0) {
        return 0;
    }
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = (LONG)sbuf->Width;
    bmi.bmiHeader.biHeight = -(LONG)sbuf->Height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 16;
    bmi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(dc, hwndx + subx, hwndy + suby, subw, subh,
                  subx, suby, subw, subh, sbuf->Buffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
    return 0;
}

void __stdcall SmackBufferClose(SmackBuf *sbuf)
{
    if (!sbuf) {
        return;
    }
    free(sbuf->Buffer);
    free(sbuf);
}

void __stdcall SmackBufferNewPalette(SmackBuf *sbuf, const void *pal, u32 paltype)
{
    if (sbuf && pal) {
        memcpy(sbuf->Palette, pal, sizeof(sbuf->Palette));
        sbuf->PalType = paltype;
    }
}

u32 __stdcall SmackBufferSetPalette(SmackBuf *sbuf)
{
    return 0;
}

void __stdcall SmackBufferClear(SmackBuf *destbuf, u32 color)
{
    if (destbuf && destbuf->Buffer) {
        for (u32 y = 0; y < destbuf->Height; ++y) {
            uint16_t *row = (uint16_t *)((uint8_t *)destbuf->Buffer + (size_t)y * destbuf->Pitch);
            for (u32 x = 0; x < destbuf->Width; ++x) {
                row[x] = (uint16_t)color;
            }
        }
    }
}

void __stdcall SmackBufferFromScreen(SmackBuf *destbuf, HWND hw, s32 x, s32 y)
{
}

void __stdcall SmackBufferStartMultipleBlits(SmackBuf *sbuf)
{
}

void __stdcall SmackBufferEndMultipleBlits(SmackBuf *sbuf)
{
}

char *__stdcall SmackBufferString(SmackBuf *sb, char *dest)
{
    if (dest) {
        strcpy(dest, "FFmpeg Smacker buffer shim");
    }
    return dest;
}

void __stdcall SmackBufferToBuffer(SmackBuf *destbuf, s32 destx, s32 desty, const SmackBuf *sourcebuf, s32 sourcex, s32 sourcey, s32 sourcew, s32 sourceh)
{
    if (!destbuf || !sourcebuf || !destbuf->Buffer || !sourcebuf->Buffer || sourcew <= 0 || sourceh <= 0) {
        return;
    }
    for (s32 y = 0; y < sourceh; ++y) {
        uint8_t *dst = (uint8_t *)destbuf->Buffer + (size_t)(desty + y) * destbuf->Pitch + (size_t)destx * 2U;
        const uint8_t *src = (const uint8_t *)sourcebuf->Buffer + (size_t)(sourcey + y) * sourcebuf->Pitch + (size_t)sourcex * 2U;
        memcpy(dst, src, (size_t)sourcew * 2U);
    }
}

void __stdcall SmackBufferToBufferTrans(SmackBuf *destbuf, s32 destx, s32 desty, const SmackBuf *sourcebuf, s32 sourcex, s32 sourcey, s32 sourcew, s32 sourceh, u32 TransColor)
{
    SmackBufferToBuffer(destbuf, destx, desty, sourcebuf, sourcex, sourcey, sourcew, sourceh);
}

void __stdcall SmackBufferToBufferMask(SmackBuf *destbuf, s32 destx, s32 desty, const SmackBuf *sourcebuf, s32 sourcex, s32 sourcey, s32 sourcew, s32 sourceh, u32 TransColor, const SmackBuf *maskbuf)
{
    SmackBufferToBuffer(destbuf, destx, desty, sourcebuf, sourcex, sourcey, sourcew, sourceh);
}

void __stdcall SmackBufferToBufferMerge(SmackBuf *destbuf, s32 destx, s32 desty, const SmackBuf *sourcebuf, s32 sourcex, s32 sourcey, s32 sourcew, s32 sourceh, u32 TransColor, const SmackBuf *mergebuf)
{
    SmackBufferToBuffer(destbuf, destx, desty, sourcebuf, sourcex, sourcey, sourcew, sourceh);
}

void __stdcall SmackBufferCopyPalette(SmackBuf *destbuf, SmackBuf *sourcebuf, u32 remap)
{
    if (destbuf && sourcebuf) {
        memcpy(destbuf->Palette, sourcebuf->Palette, sizeof(destbuf->Palette));
    }
}

u32 __stdcall SmackBufferFocused(SmackBuf *sbuf)
{
    return 1;
}

HSMACKBLIT __stdcall SmackBlitOpen(u32 flags)
{
    HSMACKBLIT blit = calloc(1, sizeof(*blit));
    if (blit) {
        blit->Flags = flags;
    }
    return blit;
}

void __stdcall SmackBlitSetPalette(HSMACKBLIT sblit, void *Palette, u32 PalType)
{
    if (sblit) {
        sblit->Palette = Palette;
        sblit->PalType = PalType;
    }
}

u32 __stdcall SmackBlitSetFlags(HSMACKBLIT sblit, u32 flags)
{
    if (!sblit) {
        return 0;
    }
    const u32 old = sblit->Flags;
    sblit->Flags = flags;
    return old;
}

void __stdcall SmackBlit(HSMACKBLIT sblit, void *dest, u32 destpitch, u32 destx, u32 desty, void *src, u32 srcpitch, u32 srcx, u32 srcy, u32 srcw, u32 srch)
{
    if (!dest || !src) {
        return;
    }
    for (u32 y = 0; y < srch; ++y) {
        memcpy((uint8_t *)dest + (size_t)(desty + y) * destpitch + (size_t)destx * 2U,
               (const uint8_t *)src + (size_t)(srcy + y) * srcpitch + (size_t)srcx * 2U,
               (size_t)srcw * 2U);
    }
}

void __stdcall SmackBlitClear(HSMACKBLIT sblit, void *dest, u32 destpitch, u32 destx, u32 desty, u32 destw, u32 desth, s32 color)
{
    if (!dest) {
        return;
    }
    for (u32 y = 0; y < desth; ++y) {
        uint16_t *row = (uint16_t *)((uint8_t *)dest + (size_t)(desty + y) * destpitch + (size_t)destx * 2U);
        for (u32 x = 0; x < destw; ++x) {
            row[x] = (uint16_t)color;
        }
    }
}

void __stdcall SmackBlitClose(HSMACKBLIT sblit)
{
    free(sblit);
}

void __stdcall SmackBlitTrans(HSMACKBLIT sblit, void *dest, u32 destpitch, u32 destx, u32 desty, void *src, u32 srcpitch, u32 srcx, u32 srcy, u32 srcw, u32 srch, u32 trans)
{
    SmackBlit(sblit, dest, destpitch, destx, desty, src, srcpitch, srcx, srcy, srcw, srch);
}

void __stdcall SmackBlitMask(HSMACKBLIT sblit, void *dest, u32 destpitch, u32 destx, u32 desty, void *src, u32 srcpitch, u32 srcx, u32 srcy, u32 srcw, u32 srch, u32 trans, void *mask)
{
    SmackBlit(sblit, dest, destpitch, destx, desty, src, srcpitch, srcx, srcy, srcw, srch);
}

void __stdcall SmackBlitMerge(HSMACKBLIT sblit, void *dest, u32 destpitch, u32 destx, u32 desty, void *src, u32 srcpitch, u32 srcx, u32 srcy, u32 srcw, u32 srch, u32 trans, void *back)
{
    SmackBlit(sblit, dest, destpitch, destx, desty, src, srcpitch, srcx, srcy, srcw, srch);
}

char *__stdcall SmackBlitString(HSMACKBLIT sblit, char *dest)
{
    if (dest) {
        strcpy(dest, "FFmpeg Smacker blit shim");
    }
    return dest;
}

void __stdcall SmackGet(Smack *smk, void *dest)
{
}

void __stdcall SmackBufferGet(SmackBuf *sbuf, void *dest)
{
}

void *__stdcall radmalloc(u32 bytes)
{
    return malloc(bytes);
}

void __stdcall radfree(void *ptr)
{
    free(ptr);
}

u32 __stdcall CopyData(void *dest, const void *src, u32 bytes, u32 a, u32 b)
{
    if (dest && src && bytes) {
        memcpy(dest, src, bytes);
    }
    return bytes;
}

void __stdcall TimerFunc(u32 id, u32 msg, u32 user, u32 dw1, u32 dw2)
{
}

void __stdcall ailcallback(void *ptr)
{
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        initialize_ffmpeg();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_direct_sound) {
            IDirectSound_Release(g_direct_sound);
            g_direct_sound = NULL;
        }
    }
    return TRUE;
}
