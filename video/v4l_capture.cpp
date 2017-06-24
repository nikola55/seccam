#include "v4l_capture.h"

#include "h264_encoder.h"

extern "C" {
// #include <libavutil/imgutils.h>
// #include <libavutil/samplefmt.h>
// #include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
// #include <libavdevice/avdevice.h>
}

#include <cassert>
#include <iostream>

using video::v4l_capture;

v4l_capture::v4l_capture(uint32_t width, uint32_t height, AVRational* fps)   
    : width_(width)
    , height_(height)
    , format_context_(NULL)
    , codec_(NULL)
    , codec_ctx_(NULL)
    , frame_(NULL)
    , encoder_(0)
    , capture_stopped_(true) {

    fps_.num = fps->num;
    fps_.den = fps->den;

    AVInputFormat *input_format = av_find_input_format("v4l2");
    assert(NULL != input_format);
    
    char frame_rate_str[33], width_str[17], height_str[17];
    sprintf(frame_rate_str, "%d/%d", fps_.num, fps_.den);
    sprintf(width_str, "%d", width_);
    sprintf(height_str, "%d", height_);

    std::cout << height_str << std::endl;

    AVDictionary *options = NULL;
    av_dict_set(&options, "framerate", frame_rate_str, 0);
    av_dict_set(&options, "width", width_str, 0);
    av_dict_set(&options, "height", height_str, 0);
    
    avformat_open_input(&format_context_, "/dev/video0", input_format, &options);
    assert(NULL != format_context_);
    av_dict_free(&options);

    assert(1 == format_context_->nb_streams);
    AVStream* stream = format_context_->streams[0];
    codec_ = avcodec_find_decoder(stream->codecpar->codec_id);
    assert(NULL != codec_);
    codec_ctx_ = avcodec_alloc_context3(codec_);
    assert(NULL != codec_ctx_);
    codec_ctx_->pix_fmt = (AVPixelFormat)stream->codecpar->format;
    codec_ctx_->width = width_;
    codec_ctx_->height = height_;
    int codec_open = avcodec_open2(codec_ctx_, codec_, NULL);
    assert(0 == codec_open);
    frame_ = av_frame_alloc();
    assert(NULL != frame_);

}

v4l_capture::~v4l_capture() {
    av_frame_free(&frame_);
    avcodec_close(codec_ctx_);
    avcodec_free_context(&codec_ctx_);
    avformat_close_input(&format_context_);
}

void v4l_capture::attach_sink(h264_encoder* enc) {
    assert(0 == encoder_);
    assert(0 != enc);
    encoder_ = enc;
    assert(1 == format_context_->nb_streams);
    AVStream* stream = format_context_->streams[0];
    std::cout << "attach_sink" << width_ << " " << height_ << std::endl;
    encoder_->initialize(width_, height_, &stream->time_base, &fps_, (AVPixelFormat)codec_ctx_->pix_fmt);
}

void v4l_capture::start_capture() {

    assert(true == capture_stopped_);
    assert(0 != encoder_);

    capture_stopped_ = false;
    while(!capture_stopped_) {
        AVPacket* pkt = av_packet_alloc();
        if(0 != av_read_frame(format_context_, pkt)) {
            encoder_->on_eof();
            capture_stopped_ = true; 
            break;
        }
        int ret = avcodec_send_packet(codec_ctx_, pkt);
        assert(ret >= 0);
        while(true) {
            ret = avcodec_receive_frame(codec_ctx_, frame_);
            if(ret != 0) break;
            encoder_->on_packet(frame_);
        }
        av_packet_free(&pkt);
    }
}

void v4l_capture::stop_capture() {
    assert(false == capture_stopped_);
    encoder_->on_eof();
    capture_stopped_ = true;
}