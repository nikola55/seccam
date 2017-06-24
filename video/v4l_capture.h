#ifndef V4L_CAPTURE_H
#define V4L_CAPTURE_H

extern "C" {
#include <libavutil/rational.h>
}

#include <cstdint>

struct AVFormatContext;
struct AVRational;
struct AVCodec;
struct AVCodecContext;
struct AVFrame;

namespace video {

class h264_encoder;

class v4l_capture {
public:
    v4l_capture(uint32_t width, uint32_t height, AVRational* fps);
    ~v4l_capture();
private:
    v4l_capture(const v4l_capture&) = delete;
    void operator=(const v4l_capture&) = delete;
public:
    void attach_sink(h264_encoder* enc);
public:
    void start_capture();
    void stop_capture();
private:
    uint32_t width_;
    uint32_t height_;
    AVRational fps_;
    AVFormatContext* format_context_;
    AVCodec* codec_;
    AVCodecContext* codec_ctx_;
    AVFrame* frame_;
    h264_encoder* encoder_;
    volatile bool capture_stopped_;
};

}

#endif // V4L_CAPTURE_H
