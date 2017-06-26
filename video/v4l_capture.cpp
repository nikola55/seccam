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

v4l_capture::v4l_capture()   
    : format_context_(NULL)
    , codec_(NULL)
    , codec_ctx_(NULL)
    , frame_(NULL)
    , encoder_(0) {

    AVInputFormat *input_format = av_find_input_format("v4l2");
    assert(NULL != input_format);

    avformat_open_input(&format_context_, "/dev/video0", input_format, NULL);
    assert(NULL != format_context_);
    
    int ret = avformat_find_stream_info(format_context_, NULL);
    assert(0 <= ret);
    ret = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, &codec_, 0);
    assert(0 <= ret);
    assert(NULL != codec_);
    AVStream* stream = format_context_->streams[ret];
    assert(NULL != stream);

    codec_ctx_ = avcodec_alloc_context3(codec_);
    assert(NULL != codec_ctx_);
    assert(NULL != stream->codecpar);
    ret = avcodec_parameters_to_context(codec_ctx_, stream->codecpar);
    assert(0 <= ret);
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
    encoder_->initialize(codec_ctx_->width, codec_ctx_->height, &stream->time_base, &stream->avg_frame_rate, codec_ctx_->pix_fmt);
}

bool v4l_capture::capture() {

    assert(0 != encoder_);

    AVPacket* pkt = av_packet_alloc();
    assert(NULL != pkt);
    if(0 != av_read_frame(format_context_, pkt)) {
        av_packet_free(&pkt);
        stop_capture();
        return false;
    }

    int ret = avcodec_send_packet(codec_ctx_, pkt);
    assert(ret >= 0);
    while(true) {
        ret = avcodec_receive_frame(codec_ctx_, frame_);
        if(ret == 0) {
            encoder_->on_frame(frame_);
            av_frame_unref(frame_);
        } else {
            break;
        }
    }
    av_packet_free(&pkt);

    return true;
}

void v4l_capture::stop_capture() {
    assert(0 != encoder_);
    int ret = avcodec_send_packet(codec_ctx_, NULL);
    assert(ret >= 0);
    while(true) {
        ret = avcodec_receive_frame(codec_ctx_, frame_);
        if(0 != ret) break;
        encoder_->on_frame(frame_);
    }
    encoder_->on_eof();
}