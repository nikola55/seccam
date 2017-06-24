#ifndef H264_ENCODER
#define H264_ENCODER

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <cstdint>
#include <cstdio>

struct AVFrame;
struct AVCodec;
struct AVCodecContext;
struct AVRational;
struct AVPacket;
struct SwsContext;

namespace video {

class h264_encoder {
public:
    h264_encoder();
    ~h264_encoder();
private:
    h264_encoder(const h264_encoder&);
    h264_encoder& operator=(const h264_encoder&);
public:
    void on_packet(AVFrame*);
    void on_eof();
public:
    void initialize(uint32_t w, uint32_t h, AVRational* tb, AVRational* fps, AVPixelFormat pixfmt);
private:
    AVCodec* codec_;
    AVCodecContext* codec_ctx_;
    SwsContext* img_convert_ctx_;
    AVFrame* scaled_frame_;
    uint8_t* scaled_frame_buf_;
    AVPixelFormat src_pixfmt_;
    AVPixelFormat dst_pixfmt_;
    AVPacket* packet_;
    bool initialized_;
    FILE *output_;
};

}

#endif // !H264_ENCODER