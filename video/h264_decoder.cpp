#include "h264_decoder.h"

#include <va/va_compat.h>

#include "logging/log.h"

#include "h264_parser.h"

#include <cassert>
#include <cstring>

using video::h264_decoder;

#define TRACE LOG(common::log::info) << common::log::end;

AVPixelFormat h264_decoder::vaapi_get_format(AVCodecContext* avctx, const AVPixelFormat* pix_fmts) {

    // TRACE

    int i;
    for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        if (pix_fmts[i] == AV_PIX_FMT_VAAPI) {
            LOG(common::log::info) << "pix_fmts[" << i << "]=AV_PIX_FMT_VAAPI" << common::log::end;
            break;
        }
    }
    assert(AV_PIX_FMT_VAAPI == pix_fmts[i]);
    
    VAProfile profile = VAProfileNone; 
    switch(avctx->profile) {
        case FF_PROFILE_H264_BASELINE:
            LOG(common::log::info) << "profile is FF_PROFILE_H264_BASELINE" << common::log::end;
            profile = VAProfileH264Baseline;
            break;
        case FF_PROFILE_H264_CONSTRAINED_BASELINE:
            LOG(common::log::info) << "profile is FF_PROFILE_H264_CONSTRAINED_BASELINE" << common::log::end;
            profile = VAProfileH264ConstrainedBaseline;
            break;
        case FF_PROFILE_H264_MAIN:
            LOG(common::log::info) << "profile is FF_PROFILE_H264_MAIN" << common::log::end;
            profile = VAProfileH264Main;
            break;
        case FF_PROFILE_H264_HIGH:
            LOG(common::log::info) << "profile is FF_PROFILE_H264_HIGH" << common::log::end;
            profile = VAProfileH264High;
            break;
        default:
            LOG(common::log::info) << "profile " << avctx->profile << " is not supported" << common::log::end;
            assert(NULL == "Unsupported profile");
    }

    h264_decoder* decoder = static_cast<h264_decoder*>(avctx->opaque);
    decoder->va_init_decoder(profile, avctx->refs, avctx->coded_width, avctx->coded_height);

    return AV_PIX_FMT_VAAPI;
}

struct va_surface_holder {

    va_surface_holder(VASurfaceID id)
        : id_(id) {

    }

    VASurfaceID id_;

};

int h264_decoder::vaapi_get_buffer2(AVCodecContext *avctx, AVFrame *frame, int flags) {

    // TRACE

    if (!(avctx->codec->capabilities & CODEC_CAP_DR1)) {
        LOG(common::log::info) << "CODEC_CAP_DR1" << common::log::end;
        return avcodec_default_get_buffer2(avctx, frame, flags);
    }

    h264_decoder* dec = static_cast<h264_decoder*>(avctx->opaque);
    VASurfaceID surface;

    if(!dec->acquire_surface(&surface)) {
        LOG(common::log::info) << "cannot acquire VASurfaceID" << common::log::end;
        assert(false);
        return -1;
    }

    frame->buf[0] = av_buffer_create((uint8_t*)new va_surface_holder(surface), 0, 
                                     &h264_decoder::ff_release_buffer, dec, 
                                     AV_BUFFER_FLAG_READONLY
                                     );
    std::memset(frame->data, 0, sizeof(frame->data));
    frame->data[0] = (uint8_t *)(uintptr_t)surface;
    frame->data[3] = (uint8_t *)(uintptr_t)surface;
    memset(frame->linesize, 0, sizeof(frame->linesize));
    frame->linesize[0] = avctx->coded_width; 

}

void h264_decoder::ff_release_buffer(void* ctx, uint8_t* surface) {
    h264_decoder* dec = static_cast<h264_decoder*>(ctx);
    va_surface_holder* holder = (va_surface_holder*)surface;
    dec->recycle_surface(holder->id_);
    delete holder;
}

void h264_decoder::recycle_surface(VASurfaceID surface_id) {
    va_surfaces_.push_back(surface_id);
    // LOG(common::log::info) << "num_surfaces=" << va_surfaces_.size() << common::log::end;
}

bool h264_decoder::acquire_surface(VASurfaceID* surface_id) {
    // LOG(common::log::info) << "num_surfaces=" << va_surfaces_.size() << common::log::end;
    if(va_surfaces_.size() == 0) {
        return false;
    }
    *surface_id = *va_surfaces_.begin();
    va_surfaces_.erase(va_surfaces_.begin());
    return true;
}

h264_decoder::h264_decoder() 
    : native_display_(XOpenDisplay(NULL))
    , va_display_(vaGetDisplay(native_display_))
    , va_profile_(VAProfileNone)
    , va_initialized_(false)
    , codec_ctx_(NULL)
    , source_(nullptr)
    , packet_(av_packet_alloc())
    , frame_(av_frame_alloc())
    , frames_decoded_(0)
    , eof_reached_(false)
    , input_needed_(true) {

    assert(NULL != native_display_);
    assert(NULL != va_display_);
    assert(NULL != packet_);
    assert(NULL != frame_);

    int maj, min;
    VAStatus status = vaInitialize(va_display_, &maj, &min);
    assert(VA_STATUS_SUCCESS == status);
    LOG(common::log::info) << "Using libva version " << maj << "." << min << common::log::end;

    std::memset(&va_context_, 0, sizeof(va_context_));
    va_context_.config_id = VA_INVALID_ID;
    va_context_.context_id = VA_INVALID_ID;
    va_context_.display = va_display_;

    AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    assert(NULL != codec);

    codec_ctx_ = avcodec_alloc_context3(codec);
    assert(NULL != codec_ctx_);
    codec_ctx_->opaque = this;
    codec_ctx_->hwaccel_context = &va_context_;
    codec_ctx_->thread_count = 1;
    codec_ctx_->draw_horiz_band = 0;
    codec_ctx_->slice_flags = SLICE_FLAG_CODED_ORDER|SLICE_FLAG_ALLOW_FIELD;
    codec_ctx_->get_format = &h264_decoder::vaapi_get_format;
    codec_ctx_->get_buffer2 = &h264_decoder::vaapi_get_buffer2;

    int ret = avcodec_open2(codec_ctx_, codec, NULL);
    assert(0 == ret);
}

h264_decoder::~h264_decoder() {
    av_frame_free(&frame_);
    av_packet_free(&packet_);
    if(va_initialized_) {
        vaDestroyContext(va_display_, va_context_.context_id);
        vaDestroySurfaces(va_display_, &va_surfaces_[0], va_surfaces_.size());
        vaDestroyConfig(va_display_, va_context_.config_id);
    }
    avcodec_free_context(&codec_ctx_);
    vaTerminate(va_display_);
    XCloseDisplay(native_display_);
}

void h264_decoder::attach_source(h264_parser* parser) {
    assert(nullptr == source_);
    assert(nullptr != parser);
    source_ = parser;
    source_->initialize(codec_ctx_);
}

bool h264_decoder::read_frame() {
    
    bool result = false;

    while(true) {

        int ret;

        if(input_needed_ && !eof_reached_) {
            AVPacket* pkt = packet_;
            ret = source_->read_packet(pkt);
            if(0 == ret) {
                pkt = NULL;
                eof_reached_ = true;
                LOG(common::log::info) << "parser EOF reached" << common::log::end;
            } else if (ret < 0) {
                break;
            }
            ret = avcodec_send_packet(codec_ctx_, pkt);
            if(0 != ret) {
                break;
            }
            input_needed_ = false;
        }

        ret = avcodec_receive_frame(codec_ctx_, frame_); 

        if(ret == 0) {
            result = true;
            frames_decoded_++;
            break;
        } else if (ret == AVERROR(EAGAIN)) {
            input_needed_ = true;
            continue;
        } else if (ret == AVERROR_EOF) {
            // TODO: notify EOF
            LOG(common::log::info) << "Decoder context drained. Decoded frames: " << frames_decoded_ << common::log::end;
            result = false;
            break;
        } else {
            result = false;
            break;
        }

    }
    return result;
}

void h264_decoder::va_init_decoder(VAProfile profile, int refs, int width, int height) {
    va_profile_ = profile;
    VAConfigAttrib attr = { VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420 };
    VAStatus status = vaCreateConfig(va_display_, profile, VAEntrypointVLD, &attr, 1, &va_context_.config_id);
    assert(VA_STATUS_SUCCESS == status);
    
    LOG(common::log::info) << "refs=" << refs 
                           << " width=" << width 
                           << " height=" << height 
                           << common::log::end;
    
    va_surfaces_.resize(refs+1+5);
    status = vaCreateSurfaces(va_display_, width, height, VA_RT_FORMAT_YUV420, va_surfaces_.size(), &va_surfaces_[0]);
    assert(VA_STATUS_SUCCESS == status);
    status = vaCreateContext(va_display_, va_context_.config_id, 
                             width, height, VA_PROGRESSIVE, &va_surfaces_[0], va_surfaces_.size(), 
                             &va_context_.context_id
                             );
    assert(VA_STATUS_SUCCESS == status);

    va_initialized_ = true;
}