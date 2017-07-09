#include "h264_encoder.h"

#include "logging/log.h"

#include "segmenter.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <cassert>
#include <iostream>

using video::h264_encoder;

h264_encoder::h264_encoder() 
    : codec_(NULL)
    , codec_ctx_(NULL)
    , src_width_(0)
    , src_height_(0)
    , scaled_frame_(NULL)
    , scaled_frame_buf_(0)
    , src_pixfmt_(AV_PIX_FMT_NONE)
    , dst_pixfmt_(AV_PIX_FMT_NONE)
    , packet_(NULL)
    , initialized_(false)
	, segmenter_(NULL)
    , end_segment_pending_(false)
    , previous_segment_end_(0) {

    codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    assert(NULL != codec_);
    codec_ctx_ = avcodec_alloc_context3(codec_);
    assert(NULL != codec_ctx_);
    packet_ = av_packet_alloc();
    assert(NULL != packet_);
}

h264_encoder::~h264_encoder() {
    if(initialized_) {
        av_free(scaled_frame_buf_);
        av_frame_free(&scaled_frame_);
        sws_freeContext(img_convert_ctx_);
        avcodec_close(codec_ctx_);
    }
    av_packet_free(&packet_);
    avcodec_free_context(&codec_ctx_);
}

void h264_encoder::on_frame(AVFrame* frame) {
    assert(true == initialized_);
    assert(src_width_ == frame->width && src_height_ == frame->height);

    int ret = sws_scale(img_convert_ctx_, frame->data, frame->linesize, 0, src_height_, 
                        scaled_frame_->data, scaled_frame_->linesize
                        );
    assert(0 < ret);
    scaled_frame_->pts = frame->pts;
    ret = avcodec_send_frame(codec_ctx_, scaled_frame_);
    assert(0 == ret);
    while(true) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if(0 == ret) {
            if((packet_->flags & AV_PKT_FLAG_KEY) && end_segment_pending_) {
                LOG(common::log::info) << "key packet encoded and end_segment_pending" << common::log::end;
                if(previous_segment_end_ != 0) {
                    std::time_t current_segment_end = std::time(nullptr);
                    std::time_t diff = std::difftime(current_segment_end, previous_segment_end_);
                    previous_segment_end_ = current_segment_end;
                    LOG(common::log::info) << "current segment length: " << diff << common::log::end;
                } else {
                    previous_segment_end_ = std::time(nullptr);
                }
                segmenter_->on_segment_end();
                end_segment_pending_ = false;
                AVPacket pkt;
                pkt.data = codec_ctx_->extradata;
                pkt.size = codec_ctx_->extradata_size;
                segmenter_->on_packet(&pkt);
            }
            segmenter_->on_packet(packet_);
            av_packet_unref(packet_);
        } else {
            break;
        }
    }
}

void h264_encoder::on_segment_end() {
    assert(true == initialized_);
    end_segment_pending_ = true;
    // int ret = avcodec_send_frame(codec_ctx_, NULL);
    // assert(0 <= ret);
    // while(true) {
    //     ret = avcodec_receive_packet(codec_ctx_, packet_);
    //     if(AVERROR_EOF == ret) {
    //         break;
    //     }
    //     else if (ret < 0) { 
    //         assert(false); // TODO: handle error 
    //         break;
    //     }
    //     segmenter_->on_packet(packet_);
    //     av_packet_unref(packet_);
    // }
    // segmenter_->on_segment_end();
    // avcodec_flush_buffers(codec_ctx_);
    // avcodec_close(codec_ctx_);
    // ret = avcodec_open2(codec_ctx_, codec_, NULL);
    // assert(0 == ret);
}

void h264_encoder::on_eof() {
    assert(true == initialized_);
    int ret = avcodec_send_frame(codec_ctx_, NULL);
    assert(0 <= ret);
    while(true) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if(0 != ret) break;
        segmenter_->on_packet(packet_);
        av_packet_unref(packet_);
    }
    segmenter_->on_eof();
}

void h264_encoder::initialize(uint32_t width, 
                              uint32_t height, 
                              AVRational* tb, 
                              AVRational* fps, 
                              AVPixelFormat pixfmt,
                              int segment_length_sec
                              ) {
    assert(false == initialized_);
    
    src_width_ = width;
    src_height_ = height;
    src_pixfmt_ = pixfmt;
    assert(NULL != codec_->pix_fmts);
    dst_pixfmt_ = AV_PIX_FMT_YUV420P;
    assert(AV_PIX_FMT_NONE != dst_pixfmt_);

    codec_ctx_->width = src_width_;
    codec_ctx_->height = src_height_;
    codec_ctx_->time_base.den = fps->num;
    codec_ctx_->time_base.num = 1;
    codec_ctx_->framerate = *fps;
    codec_ctx_->pix_fmt = dst_pixfmt_;
    codec_ctx_->profile = FF_PROFILE_H264_BASELINE;
    codec_ctx_->qmin = 30;
    codec_ctx_->qmax = 70;
    codec_ctx_->gop_size = ((fps->num*segment_length_sec)/fps->den)/3;
    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(codec_ctx_, codec_, NULL);
    assert(0 == ret);

    // fwrite(codec_ctx_->extradata, 1, codec_ctx_->extradata_size, output_);

    img_convert_ctx_ = sws_getContext(src_width_, src_height_, src_pixfmt_, 
                                      src_width_, src_height_, dst_pixfmt_, 
                                      0, NULL, NULL, NULL
                                      );

    scaled_frame_buf_ = (uint8_t*)av_malloc(av_image_get_buffer_size(dst_pixfmt_, width, height, 1));
    assert(NULL != scaled_frame_buf_);
    scaled_frame_ = av_frame_alloc();
    scaled_frame_->width = width;
    scaled_frame_->height = height;
    scaled_frame_->format = codec_ctx_->pix_fmt;
    assert(NULL != scaled_frame_);
    ret = av_image_fill_arrays(scaled_frame_->data, 
                               scaled_frame_->linesize, 
                               scaled_frame_buf_, 
                               codec_ctx_->pix_fmt, 
                               scaled_frame_->width, 
                               scaled_frame_->height, 
                               1
                               );

    assert(0 < ret);
    initialized_ = true;
}

void h264_encoder::attach_sink(segmenter* seg) {
	assert(NULL == segmenter_);
	assert(NULL != seg);
    assert(true == initialized_);
    segmenter_ = seg;
    AVPacket pkt;
    pkt.data = codec_ctx_->extradata;
    pkt.size = codec_ctx_->extradata_size;
    segmenter_->on_packet(&pkt);
}
