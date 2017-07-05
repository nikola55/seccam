#ifndef H264_ENCODER_H
#define H264_ENCODER_H

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <cstdint>

struct AVFrame;
struct AVCodec;
struct AVCodecContext;
struct AVRational;
struct AVPacket;
struct SwsContext;

namespace video {

class segmenter;

class h264_encoder {
public:
    h264_encoder();
    ~h264_encoder();
private:
    h264_encoder(const h264_encoder&);
    h264_encoder& operator=(const h264_encoder&);
public:
    void on_frame(AVFrame*);
    void on_eof();
public:
    void on_segment_end();
public:
    void initialize(uint32_t w, uint32_t h, AVRational* tb, AVRational* fps, AVPixelFormat pixfmt);
public:
	void attach_sink(segmenter* seg);
private:
    AVCodec* codec_;
    AVCodecContext* codec_ctx_;
    uint32_t src_width_;
    uint32_t src_height_;
    SwsContext* img_convert_ctx_;
    AVFrame* scaled_frame_;
    uint8_t* scaled_frame_buf_;
    AVPixelFormat src_pixfmt_;
    AVPixelFormat dst_pixfmt_;
    AVPacket* packet_;
    bool initialized_;
	segmenter* segmenter_;
};

}

#endif // H264_ENCODER_H
