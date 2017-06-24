#include "h264_encoder.h"

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
    , scaled_frame_(NULL)
    , scaled_frame_buf_(0)
    , src_pixfmt_(AV_PIX_FMT_NONE)
    , dst_pixfmt_(AV_PIX_FMT_NONE)
    , packet_(NULL)
    , initialized_(false)
    , output_(NULL) {

    codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    assert(NULL != codec_);
    codec_ctx_ = avcodec_alloc_context3(codec_);
    assert(NULL != codec_ctx_);
    packet_ = av_packet_alloc();
    assert(NULL != packet_);
    output_ = std::fopen("output.h264", "w");
}

h264_encoder::~h264_encoder() {
    std::fclose(output_);
    av_packet_free(&packet_);
    if(initialized_) {
        av_free(scaled_frame_buf_);
        av_frame_free(&scaled_frame_);
        sws_freeContext(img_convert_ctx_);
        avcodec_close(codec_ctx_);
    }
    avcodec_free_context(&codec_ctx_);
}

void h264_encoder::on_packet(AVFrame* frame) {
    assert(true == initialized_);
    static int pktno = 0;
    int ret = sws_scale(img_convert_ctx_, frame->data, frame->linesize, 0, frame->height, scaled_frame_->data, scaled_frame_->linesize);
    assert(0 < ret);
    scaled_frame_->pts = frame->pts;
    ret = avcodec_send_frame(codec_ctx_, scaled_frame_);
    // std::cout << ret << std::endl;
    assert(0 == ret);
    while(true) {
        ret = avcodec_receive_packet(codec_ctx_, packet_);
        if(0 != ret) break;
        std::cout << (pktno++) << " packet ready " << packet_->size << std::endl;
        fwrite(packet_->data, 1, packet_->size, output_);
    }
}

void h264_encoder::on_eof() {
    assert(true == initialized_);
    std::cout << "eof" << std::endl;
}

void h264_encoder::initialize(uint32_t w, uint32_t h, AVRational* tb, AVRational* fps, AVPixelFormat pixfmt) {
    assert(false == initialized_);
    src_pixfmt_ = pixfmt;
    assert(NULL != codec_->pix_fmts);
    dst_pixfmt_ = avcodec_find_best_pix_fmt_of_list(codec_->pix_fmts, src_pixfmt_, 0, NULL);
    std::cout << w << " " << h << std::endl;
    assert(AV_PIX_FMT_NONE != dst_pixfmt_);
    codec_ctx_->width = w;
    codec_ctx_->height = h;
    codec_ctx_->time_base = *tb;
    codec_ctx_->framerate = *fps;
    codec_ctx_->gop_size = 7;
    codec_ctx_->max_b_frames = 1;
    codec_ctx_->pix_fmt = dst_pixfmt_;
    int ret = avcodec_open2(codec_ctx_, codec_, NULL);
    assert(0 == ret);
    img_convert_ctx_ = sws_getContext(w, h, src_pixfmt_, w, h, dst_pixfmt_, 0, NULL, NULL, NULL);
    
    scaled_frame_ = av_frame_alloc();
    scaled_frame_->width = w;
    scaled_frame_->height = h;
    scaled_frame_->format = dst_pixfmt_;
    assert(NULL != scaled_frame_);
    scaled_frame_buf_ = (uint8_t*)av_malloc(av_image_get_buffer_size(dst_pixfmt_, w, h, 1));
    assert(NULL != scaled_frame_buf_);
    ret = av_image_fill_arrays(scaled_frame_->data, scaled_frame_->linesize, scaled_frame_buf_,
                                dst_pixfmt_, scaled_frame_->width, scaled_frame_->height, 1
                                );
    assert(0 < ret);
    initialized_ = true;
}