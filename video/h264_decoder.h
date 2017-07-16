#ifndef H264_DECODER_H
#define H264_DECODER_H

#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/vaapi.h>
}

#include <va/va_x11.h>
#include <va/va.h>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;

namespace video {

class h264_parser;

class h264_decoder {
public:
    h264_decoder();
    ~h264_decoder();
private:
    h264_decoder(const h264_decoder&) = delete;
    void operator=(const h264_decoder&) = delete;
public:
    void attach_source(h264_parser*);
public:
    bool read_frame();
private:
    static AVPixelFormat vaapi_get_format(AVCodecContext*, const AVPixelFormat*);
    void va_init_decoder(VAProfile profile, int refs, int width, int height);

    static int vaapi_get_buffer2(AVCodecContext *avctx, AVFrame *frame, int flags);
    bool acquire_surface(VASurfaceID* surface_id);
    static void ff_release_buffer(void*, uint8_t*);
    void recycle_surface(VASurfaceID surface_id);
private:
    Display* native_display_;
    VADisplay va_display_;
    vaapi_context va_context_;
    VAProfile va_profile_;
    AVCodecContext* codec_ctx_;
    bool va_initialized_;
    std::vector<VASurfaceID> va_surfaces_;
    h264_parser* source_;
    AVPacket* packet_;
    AVFrame* frame_;
    int frames_decoded_;
    bool eof_reached_;
    bool input_needed_;
};

}

#endif